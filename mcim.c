/*
 * Copyright (c) 2021, 2022 ipc
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curses.h>
#include <poll.h>
#include <unistd.h>

#include "mcim.h"

static char *progname;
static int dispc;

void
err(int f, char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	vfprintf(stderr, fmt, arg);
	va_end(arg);
	if (f) {
		endwin();
		exit(EXIT_FAILURE);
	}
}

static void
usage(void)
{
	err(1, "usage: %s [-n] [-t file] file\n", progname);
}

int
allocmem(struct mcim *p, uint n)
{
	if ((p->mem = realloc(p->mem, p->nmem+n)) == NULL)
		return -1;
	memset(p->mem+p->nmem, 0, n);
	return 0;
}

static int
loadproc(struct mcim *p, struct mcimp *c, FILE *f)
{
	uchar hdr[4];
	uint n;

	if (fread(hdr, 1, sizeof hdr, f) < 4)
		return -1;
	if ((n = lget(hdr)) > MEMLIM)
		return -1;
	allocmem(p, n);
	if (fread(p->mem+p->nmem, 1, n, f) < n)
		return -1;
	c->p = p;
	c->epc = p->nmem;
	c->pc = c->epc;
	c->stat = 0;
	memset(c->r, 0, sizeof c->r);
	p->nmem += n;
	return 0;
}

#define MOVCLR(y) \
	move(y, 0); \
	clrtoeol()
#define MOVINFO() \
	MOVCLR(p->nproc)

static void
brkp(struct mcim *p, int cmd)
{
	struct mcimp *c;
	int i;

	nocbreak();
	echo();
	MOVINFO();
	scanw("%4d", &i);
	if (i >= 0 && i < p->nproc) {
		c = &p->proc[i];
		c->stat ^= STATBRK;
	}
	noecho();
	cbreak();
}

static void
view(struct mcim *p, int cmd)
{
	char *s;

	s = "lmp";
	dispc = strchr(s, cmd) - s;
	clear();
}  

static void
exec(struct mcim *p, int cmd)
{
	char s[256]; 
	FILE *f;
	int n;

	nocbreak();
	echo();
	MOVINFO();
	scanw("%255s", s);
	if ((f = fopen(s, "r")) == NULL) {
		MOVINFO();
		printw("%s: couldn't open file", s);
		goto enable;
	}
	n = loadproc(p, &p->proc[p->nproc], f);
	fclose(f);
	if (n == -1) {
		MOVINFO();
		printw("%s: invalid process", s);
	} else
		p->nproc++;
enable:
	cbreak();
	noecho();
} 

static void
start(struct mcim *p, int cmd)
{
	struct mcimp *c;
	int i;

	i = -1;
	nocbreak();
	echo();
	MOVINFO();
	scanw("%4d", &i);
	cbreak();
	noecho();
	if (i < 0 || i > p->nproc - 1) {
		MOVINFO();
		printw("%s", i<0?"bad input":"out of range");
		return;
	}
	c = &p->proc[i];
	memset(c->r, 0, sizeof c->r);
	c->pc = c->epc;
	c->stat = 0;
}

void (*comtab[]) (struct mcim *, int) = {
	&brkp,
	&view,
	&view,
	&exec,
	&view,
	&start,
};

static int
get(struct mcim *p)
{
	int c;
	char *cmd;
	char *cp;

	c = 0;
	cmd = "blmops";
	read(STDIN_FILENO, &c, 1);
	if (c == 'q')
		return -1;
	if ((cp = strchr(cmd, c)) == NULL) {
		MOVINFO();
		printw("invalid command");
		return 0;
	}
	comtab[cp - cmd](p, c);
	return 0;
}

static void
pdisp(struct mcim *p)
{
	int i, j;
	struct mcimp *c;

	for (i = 0; i < p->nproc; i++) {
		c = &p->proc[i];
		MOVCLR(i);
		mvprintw(i, 0, "%d %08x %08x %x ", i, c->epc, c->pc, c->stat); 
		for (j = 0; j < 16; j++)
			printw("%x%s", c->r[j], j == 15 ? "" : ":");
	}
	refresh();
}

static void
mdisp(struct mcim *p)
{
	int i;

	move(0, 0);
	clrtoeol();
	for (i = 0; i < p->nmem; i++)
		printw("%02x%s", p->mem[i],
		       i == p->nmem-1 || (i+1)%12 == 0 ? "\n" : " ");	
	refresh();
}

static void
tdisp(struct mcim *p)
{
	int i;

	move(0, 0);
	for (i = 0; i < p->ntty; i++)
		printw("%c", p->tty[i]);
	printw("\n");
	refresh();
}

void (*disptab[]) (struct mcim *) = {
	&pdisp, &mdisp, &tdisp
};

int
main(int argc, char *argv[])
{
	int iproc;
	char *s;
	char *ttyout;
	FILE *f;
	int n;
	int log;
	struct mcim c;
	struct pollfd pfd[1];

	iproc = 1;
	ttyout = NULL;
	f = NULL;
	progname = *argv[0]!='\0'?argv[0]:"mcim";
	while (--argc > 0 && (*++argv)[0] == '-')
		for (s = argv[0]+1; *s != '\0'; s++)
			switch (*s) {
			case 'n':
				iproc = 0;
				break;
			case 't':
				if (argc < 2)
					usage();
				ttyout = argv[1];
				argc--;
				argv++;
				break;
			default:
				usage();
				break;
			}
	memset(&c, 0, sizeof c);
	if (iproc) {
		if (argc < 1)
			usage();
		if ((f = fopen(argv[0], "rb")) == NULL)
			err(1, "%s: couldn't open '%s'\n", progname, argv[0]);
		c.nproc++;
		n = loadproc(&c, &c.proc[0], f);
		fclose(f);
		if (n == -1)
			err(1, "%s: bad executable\n", progname);
	}
	if (ttyout != NULL)
		if ((f = fopen(ttyout, "w")) == NULL)
			err(1, "%s: couldn't open '%s'\n", progname, ttyout);
	n = 0;
	log = LOGCNT; 
	pfd[0].fd = STDIN_FILENO;
	pfd[0].events = POLLIN;
	initscr();
	cbreak();
	noecho();
	while ((n = poll(pfd, 1, 10)) != -1) {
		if (pfd[0].revents & (POLLERR|POLLHUP|POLLNVAL))
			break;
		if (n > 0 && get(&c) == -1)
			break;
		disptab[dispc](&c);
		step(&c);
		if (c.ntty > 0 && ttyout != NULL && !--log) {
			log = LOGCNT;
			fseek(f, 0L, SEEK_SET);
			fwrite(c.tty, 1, c.ntty, f);
		}
	}
	free(c.mem);
	endwin();
	return 0;
}
