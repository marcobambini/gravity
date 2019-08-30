//
//  gravity_array.h
//    gravity
//
//  Created by Marco Bambini on 31/07/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_MUTABLE_ARRAY__
#define __GRAVITY_MUTABLE_ARRAY__

#include <stdint.h>

// Inspired by https://github.com/attractivechaos/klib/blob/master/kvec.h

#define MARRAY_DEFAULT_SIZE         8
#define marray_t(type)              struct {size_t n, m; type *p;}
#define marray_init(v)              ((v).n = (v).m = 0, (v).p = 0)
#define marray_decl_init(_t,_v)     _t _v; marray_init(_v)
#define marray_destroy(v)           if ((v).p) free((v).p)
#define marray_get(v, i)            ((v).p[(i)])
#define marray_setnull(v, i)        ((v).p[(i)] = NULL)
#define marray_pop(v)               ((v).p[--(v).n])
#define marray_last(v)              ((v).p[(v).n-1])
#define marray_size(v)              ((v).n)
#define marray_max(v)               ((v).m)
#define marray_inc(v)               (++(v).n)
#define marray_dec(v)               (--(v).n)
#define marray_nset(v,N)            ((v).n = N)
#define marray_push(type, v, x)     {if ((v).n == (v).m) {                                        \
                                    (v).m = (v).m? (v).m<<1 : MARRAY_DEFAULT_SIZE;                \
                                    (v).p = (type*)realloc((v).p, sizeof(type) * (v).m);}        \
                                    (v).p[(v).n++] = (x);}
#define marray_resize(type, v, n)   (v).m += n; (v).p = (type*)realloc((v).p, sizeof(type) * (v).m)
#define marray_resize0(type, v, n)  (v).p = (type*)realloc((v).p, sizeof(type) * ((v).m+n));    \
                                    (v).m ? memset((v).p+(sizeof(type) * n), 0, (sizeof(type) * n)) : memset((v).p, 0, (sizeof(type) * n)); (v).m += n
#define marray_npop(v,k)            ((v).n -= k)
#define marray_reset(v,k)           ((v).n = k)
#define marray_reset0(v)            marray_reset(v, 0)
#define marray_set(v,i,x)           (v).p[i] = (x)

// commonly used arrays
typedef marray_t(uint16_t)          uint16_r;
typedef marray_t(uint32_t)          uint32_r;
typedef marray_t(void *)            void_r;
typedef marray_t(const char *)      cstring_r;

#endif
