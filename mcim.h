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

enum {
	SYSE,
	SYSP,
	SYSW
};

#define MEMLIM 0xff000
#define SCHCNT 300
#define LOGCNT 50 

#define STATEXIT 1 << 1
#define STATBRK  1 << 2

typedef unsigned int uint;
typedef unsigned char uchar;

struct mcimp {
	struct mcim *p;
	uint r[16];
        uint epc;
        uint pc;
	uint stat;
};

struct mcim {
	struct mcimp proc[4096];
	uchar *mem;
	uchar tty[8192];
	uint ntty;
	uint nproc;
	uint nmem;
};

void err(int f, char *fmt, ...);
int allocmem(struct mcim *p, uint n);
uint lget(uchar *p);
void step(struct mcim *p); 
