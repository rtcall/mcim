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

#include "mcim.h"

uint
lget(uchar *p)
{
	uint n;

	n = *p;
	n |= *(p+1) << 8;
	n |= *(p+2) << 16;
	n |= *(p+3) << 24;
	return n;
}

#define MEM(p) p->mem

static uint
lcget(struct mcimp *p)
{
	uint n;

	n = lget(&p->MEM(p)[p->pc]);
	p->pc += 4;
	return n;
}

static uint
rget(struct mcimp *p)
{
	uint n;

	n = p->MEM(p)[p->pc];
	p->pc += 1;
	return n;
}

#define R()  rget(p)
#define L()  lcget(p)
#define J(n) p->pc = n + p->epc

static void
lw(struct mcimp *p)
{
	uint r;
	uint l;

	r = R();
	if ((l = L() + 4) >= p->p->nmem)
		return;
	p->r[r] = lget(&p->MEM(p)[l+p->r[8]]);
}

static void
lb(struct mcimp *p)
{
	uint r;
	uint l;

	r = R();
	if ((l = L()) >= p->p->nmem)
		return;
	p->r[r] = p->MEM(p)[l+p->r[8]];
}

static void
li(struct mcimp *p)
{
	uint r;

	r = R();
	p->r[r] = L();
}

static void
sw(struct mcimp *p)
{
	uint r;
	uint l;
	uint c;

	r = R();
	l = L();
	l += p->r[8];
	if (l > p->p->nmem) {
		c = (l - p->p->nmem) + 4;
		allocmem(p->p, c);
		p->p->nmem += c;
	}
	p->MEM(p)[l] = p->r[r];
	p->MEM(p)[l+1] = p->r[r] >> 8;
	p->MEM(p)[l+2] = p->r[r] >> 16;
	p->MEM(p)[l+3] = p->r[r] >> 24;
}

static void
sb(struct mcimp *p)
{
	uint r;
	uint l;
	uint c;

	r = R();
	l = L();
	l += p->r[8];
	if (l >= p->p->nmem) {
		c = l - p->p->nmem;
		allocmem(p->p, c+128);
		p->p->nmem += c+128;
	}
	p->MEM(p)[l] = p->r[r];
}

static void
add(struct mcimp *p)
{
	uint r[3];

	r[0] = R();
	r[1] = R();
	r[2] = R();
	p->r[r[2]] = p->r[r[0]] + p->r[r[1]];
}

static void
addi(struct mcimp *p)
{
	uint r[2];
	uint l;

	r[0] = R();
	l = L();
	r[1] = R();
	p->r[r[1]] = p->r[r[0]] + l;
}

static void
ble(struct mcimp *p)
{
	uint r[2];
	uint l;

	r[0] = R();
	r[1] = R();
	l = L();
	if (p->r[r[0]] < p->r[r[1]])
		J(l);
}

static void
bgt(struct mcimp *p)
{
	uint r[2];
	uint l;

	r[0] = R();
	r[1] = R();
	l = L();
	if (p->r[r[0]] > p->r[r[1]])
		J(l);
}

static void
beq(struct mcimp *p)
{
	uint r[2];
	uint l;

	r[0] = R();
	r[1] = R();
	l = L();
	if (p->r[r[0]] == p->r[r[1]])
		J(l);
}

static void
bne(struct mcimp *p)
{
	uint r[2];
	uint l;

	r[0] = R();
	r[1] = R();
	l = L();
	if (p->r[r[0]] != p->r[r[1]])
		J(l);
}

static void
j(struct mcimp *p)
{
	J(L());
}

static void
jr(struct mcimp *p)
{
	p->pc = p->r[R()];
}

static void
jal(struct mcimp *p)
{
	uint pc;

	pc = L();
	p->r[3] = p->pc;
	J(pc);
}
 
static void
sys(struct mcimp *p)
{
	uint r;

	r = p->r[R()];
	switch (r) {
	case SYSE:
		p->stat |= STATEXIT;
		break;
	case SYSW:
		if (p->p->ntty >= sizeof p->p->tty)
			p->p->ntty = 0;
		p->p->tty[p->p->ntty++] = p->r[1];
		break;
	default:
		err(1, "fatal: illegal syscall (%08x)\n", r);
		break;
	}
}

static void (*op[]) (struct mcimp *) = {
	&lw,  NULL,  &lb,  &li,  &sw,  NULL, &sb,  NULL,
	&add, &addi, NULL, NULL, NULL, &ble, &bgt, &beq,
	&bne, &j,    &jr,  &jal, &sys
};

#define brk(p) (p->stat & STATEXIT || p->stat & STATBRK)

void
step(struct mcim *p)
{
	int n;
	uint i;
	struct mcimp *c;

	for (i = 0; i < p->nproc; i++) {
		n = SCHCNT;
		c = &p->proc[i];
		while (n-- && !brk(c))
			op[p->mem[c->pc++]](c);
	}
}
