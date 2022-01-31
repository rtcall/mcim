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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mcimc.h"

enum {
	IDNT,
	LBL,
	REG,
	ADDR,
	PROC
};

#define SYMLEN 1024
#define SYMALLOC 8192 

#define MAXLBL 8192 
#define MAXREG 16

struct sym {
	int t;
	int l;
	char s[SYMLEN];
};

struct ins {
	char s[SYMLEN];
	char fmt[4];
};

struct lbl {
	char s[SYMLEN];
	uint addr; 
	int l;
	struct lbl *next;
};

static struct sym *tab;
static struct sym *sp;
static struct lbl *lab[MAXLBL];
static struct lbl *lbp;
static int nsym;
static int asym;
static int errc;
static uchar *buf;
static int abuf;
static uint pc;
struct ins instab[];

static FILE *f;
static char in[1024];
static char *progname;

static void mfree(void);

static void
err(int f, char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	vfprintf(stderr, fmt, arg);
	va_end(arg);
	errc++;
	if (f) {
		mfree();
		exit(EXIT_FAILURE);
	}
}

static void
usage(void)
{
	err(1, "usage: %s [-o out] file\n", progname);
}

static void
mfree(void)
{
	int i;
	struct lbl *p, *t;

	free(tab);
	free(buf);
	for (i = 0; i < MAXLBL; i++) {
		p = lab[i];
		while (p != NULL) {
			t = p;
			p = p->next;
			free(t);
		}
	}
}

static int
allocsym(long n)
{
	asym += n;
	if ((tab = realloc(tab, asym*sizeof(struct sym))) == NULL)
		return -1;
	return 0;
}

static struct sym *
makesym(int t, int l, char *s)
{
	struct sym *p;

	if ((p = malloc(sizeof(struct sym))) == NULL)
		return NULL;
	p->t = t;
	p->l = l;
	strcpy(p->s, s);
	return p;
} 

static struct sym *
pushsym(struct sym *p)
{
	if (nsym >= asym)
		if (allocsym(SYMALLOC) == -1)
			return NULL;
	tab[nsym++] = *p;
	return p;
}

static struct lbl *
makelbp(char *s, uint n, uint l)
{
	struct lbl *p;

	if ((p = malloc(sizeof(struct lbl))) == NULL)
		return NULL;
	strcpy(p->s, s);
	p->addr = n;
	p->l = l;
	p->next = lbp;
	lbp = p;
	return p;
}

static int l = 1;

static int
cget(void)
{
	int c;

	c = getc(f);
	l += c == '\n';
	return c;
}

static int
get(struct sym **p)
{
	char s[SYMLEN];
	int n;
	int c;
	int t;
	int lp;

	n = 0;
	t = -1;
	while ((c = cget()) != EOF && isspace(c))
		;
	switch (c) {
	case '%':
		t = REG;
		break;
	case '$':
		t = ADDR; 
		break;
	case '.':
		t = PROC;
		while ((c = cget()) != EOF && c != '\n') {
			if (n >= SYMLEN-1)
				break;
			s[n++] = c;
		}
		s[n] = '\0';
		lp = c == '\n' ? l-1 : l;
		*p = makesym(t, lp, s);
		return 1;
	case ';':
		while ((c = cget()) != EOF && c != '\n')
			;
		return 0;
	}
	if (t == -1 && isalpha(c)) {
		t = IDNT;
		ungetc(c, f);
		while ((c = cget()) != EOF) {
			if (c == ':')
				t = LBL;
			if (!isalnum(c))
				break;
			if (n >= SYMLEN-1)
				break; 
			s[n++] = c;
		}
		s[n] = '\0';
		lp = c == '\n' ? l-1 : l;
		*p = makesym(t, lp, s);
		return 1; 
	}
	if (t == REG || t == ADDR) {
		while ((c = cget()) != EOF) {
			if (!isxdigit(c) && !isspace(c)) {
				err(0, "%s:%d: bad address\n", in, l);
				break;
			} else if (!isxdigit(c))
				break;
			if (n >= SYMLEN-1)
				break;
			s[n++] = c;
		}
		s[n] = '\0';
		lp = c == '\n' ? l-1 : l;
		*p = makesym(t, lp, s); 
		return 1; 
	}
	if (c != EOF) {
		err(0, "%s:%d: unknown symbol\n", in, l);
		return 0;
	}
	return -1;
}

static int
com(char *s)
{
	uint n;
	uchar *p;

	n = 0;
	for (p = (uchar*) s; *p != '\0'; p++)
		n = n * 31 + *p;
	return n % MAXLBL;
}

static struct lbl *
lookup(char *s, int c)
{
	int n;
	struct lbl *p;

	n = com(s);
	for (p = lab[n]; p != NULL; p = p->next)
		if (strcmp(s, p->s) == 0)
			return p;
	if (c) {
		if ((p = malloc(sizeof(struct lbl))) == NULL)
			return NULL;
		strcpy(p->s, s);
		p->addr = pc;
		p->next = lab[n];
		lab[n] = p;
		return NULL;
	}
	return p;
}

static void
lp(uchar *p, uint l)
{
	*p = l;
	*(p+1) = l >> 8;
	*(p+2) = l >> 16;
	*(p+3) = l >> 24;
}

static int
allocb(void)
{
	abuf += 8192;
	buf = realloc(buf, abuf);
	return buf == NULL ? -1 : 0;
}

static void
cput(uchar c)
{
	if (pc+1 >= abuf)
		allocb();
	buf[pc++] = c;
}

static void
lput(uint l)
{
	if (pc+4 >= abuf)
		allocb();
	lp(buf+pc, l);
	pc += 4;
}

static void
reg(void)
{
	int n;

	if (sp->t != REG)
		err(0, "%s:%d: expected register\n", in, sp->l);
	else if ((n = strtol(sp->s, NULL, 16)) >= MAXREG)
		err(0, "%s:%d: bad register %02x\n", in, sp->l, n);
	else
		cput(n);
	sp++;
}

static void
addr(void)
{
	int n;

	n = 0;
	if (sp->t == ADDR)
		n = strtol(sp->s, NULL, 16);
	else if (sp->t == IDNT)
		makelbp(sp->s, pc, sp->l);
	else {
		err(0, "%s:%d: expected immediate\n", in, sp->l);
		sp++;
		return;
	}
	lput(n);
	sp++;
}

static int gen(void);

static int
pget(char *p, char *s)
{
	if (*p != '\'')
		return -1;	
	for (p++; *p != '\0' && *p != '\''; p++)
		*s++ = *p;
	if (*p != '\'')
		return -1;
	*s = '\0';
	return 0;
}

static void
inc(struct sym *sp, char *p)
{
	char path[1024];
	char mpath[1024];
	FILE *fp;
	int lp;

	if (pget(p, path) == -1) {
		err(0, "%s:%d: expected value in include\n", in, sp->l);
		return;
	}
	fp = f;
	if ((f = fopen(path, "r")) == NULL) {
		err(0, "%s:%d: couldn't open '%s'\n", in, sp->l, path);
		f = fp;
		return;
	}
	strcpy(mpath, in);
	strcpy(in, path);
	lp = l;
	gen();
	f = fp;
	l = lp;
	strcpy(in, mpath);
}

struct proc {
	char *s;
	void (*fn) (struct sym *, char *);
};

static struct proc proctab[] = {
	{ "include", &inc },
	{ NULL,      NULL }
};

static void
proc(struct sym *sp)
{
	int i;
	char *p;
	char *s;
	char com[1024];

	p = sp->s;
	s = com;
	while (*p != '\0' && !isspace(*p))
		*s++ = *p++;
	*s = '\0';
	while (isspace(*p))
		p++;
	for (i = 0; proctab[i].s != NULL; i++)
		if (strcmp(com, proctab[i].s) == 0) {
			proctab[i].fn(sp, p);
			return;
		}
	err(0, "%s:%d: unknown directive '%s'\n", in, sp->l, com); 
}

static void
ins(void)
{
	char *p;
	struct ins *ip;
	int i;

	if (sp->t == LBL) {
		if (lookup(sp->s, 1) != NULL)
			err(0, "%s:%d: redefining label '%s'\n", in, sp->l,
			     sp->s);
		sp++;
		return;
	}
	if (sp->t != IDNT) {
		err(0, "%s:%d: expected instruction\n", in, sp->l);
		sp++;
		return;
	}
	for (ip = NULL, i = 0; i < AINS; i++)
		if (strcmp(sp->s, instab[i].s) == 0) {
			ip = &instab[i];
			break;
		}
	if (ip == NULL) {
		err(0, "%s:%d: invalid instruction '%s'\n", in, sp->l, sp->s);
		sp++;
		return;
	}
	cput(i);
	sp++;
	for (p = ip->fmt; *p != '\0'; p++)
		switch (*p) {
		case 'r':
			reg();
			break;
		case 'i':
			addr();
			break;
		}
}

static void
lbl(void)
{
	struct lbl *p;

	while (lbp != NULL) {
		if ((p = lookup(lbp->s, 0)) == NULL)
			err(0, "%s:%d: no such label '%s'\n", in, lbp->l,
			     lbp->s);
		else
			lp(buf+lbp->addr, p->addr);
		p = lbp;
		lbp = lbp->next;
		free(p);
	}
}

static int
gen(void)
{
	struct sym *s;
	struct sym n;
	int c;

	while ((c = get(&s)) != -1)
		if (c != 0) {
			if (s->t == PROC)
				proc(s);
			else
				pushsym(s);
		}
	fclose(f);
	n.t = -1;
	pushsym(&n);
	while (sp->t != -1)
		ins();
	sp++;
        if (errc > 0) {
                err(0, "%s: %d error%s\n", in, errc, errc>1?"s":"");
		errc--;
		return -1;
	}
	errc = 0;
	return 0;
}

int
main(int argc, char *argv[])
{
	char *out;
	char *s;
	int n;
	uchar hd[4];

	progname = *argv[0]!='\0'?argv[0]:"mcimc";
	out = "a";
	while (--argc > 0 && (*++argv)[0] == '-')
		for (s = argv[0]+1; *s != '\0'; s++)
			switch (*s) {
			case 'o':
				if (argc < 2)
					usage();
				out = argv[1];
				argc--;
				argv++;
				break;
			default:
				usage();
				break;
			}
	if (argc < 1)
		usage();
	strcpy(in, argv[0]);
	if ((f = fopen(in, "r")) == NULL)
		err(1, "%s: couldn't open '%s'\n", progname, in);
	if (allocsym(SYMALLOC) == -1) {
		fclose(f);
		return EXIT_FAILURE;	
	}
	sp = tab;
	allocb();
	n = gen();
	if (n == -1) {
		mfree();
		return EXIT_FAILURE;
	}
	lbl();
	if ((f = fopen(out, "wb")) == NULL)
		err(1, "%s: couldn't open '%s'\n", progname, out);
	lp(hd, pc);
	fwrite(hd, 1, sizeof hd, f);
	fwrite(buf, 1, pc, f);
	fclose(f);
	mfree();
	return 0;
}

struct ins instab[AINS] = {
	{ "lw",   "ri"  },
	{ "lwu",  "ri"  },
	{ "lb",   "ri"  },
	{ "li",   "ri"  },
	{ "sw",   "ri"  },
	{ "swu",  "ri"  },
	{ "sb",   "ri"  },
	{ "sr",   "ri"  },
	{ "add",  "rrr" },
	{ "addi", "rir" },
	{ "sub",  "rrr" },
	{ "mul",  "rrr" },
	{ "div",  "rrr" },
	{ "ble",  "rri" },
	{ "bgt",  "rri" },
	{ "beq",  "rri" },
	{ "bne",  "rri" },
	{ "j",    "i"   },
	{ "jr",   "r"   },
	{ "jal",  "i"   },
	{ "sys",  "r"   }
};
