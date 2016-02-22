/*
 * Copyright (C)2015 Haxe Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef HLC_H
#define HLC_H

#include <math.h>
#include <hl.h>

#ifdef HL_64
#	define PAD_64_VAL	,0
#else
#	define PAD_64_VAL
#endif

#ifdef HL_VCC
#	pragma warning(disable:4702) // unreachable code 
#	pragma warning(disable:4100) // unreferenced param
#	pragma warning(disable:4101) // unreferenced local var
#	pragma warning(disable:4723) // potential divide by 0
#endif

#undef CONST

static void hl_null_access() {
	hl_error_msg(USTR("Null access"));
}

static vdynamic *hl_oalloc( hl_type *t ) {
	return (vdynamic*)hl_alloc_obj(t);
}

#include <setjmp.h>

typedef struct _hl_trap_ctx hl_trap_ctx;

struct _hl_trap_ctx {
	jmp_buf buf;
	hl_trap_ctx *prev;
};

extern hl_trap_ctx *current_trap;
extern vdynamic *current_exc;

#define hlc_trap(ctx,r,label) { ctx.prev = current_trap; current_trap = &ctx; if( setjmp(ctx.buf) ) { r = current_exc; goto label; } }
#define hlc_endtrap(ctx) current_trap = ctx.prev

#endif