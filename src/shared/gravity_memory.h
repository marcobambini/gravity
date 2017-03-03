//
//  gravity_memory.h
//	gravity
//
//  Created by Marco Bambini on 20/03/16.
//  Copyright © 2016 Creolabs. All rights reserved.
//

#ifndef __GRAVITY_MEMORY__
#define __GRAVITY_MEMORY__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// memory debugger must be turned on ONLY with GuardMalloc ON
#define GRAVITY_MEMORY_DEBUG		0

#if GRAVITY_MEMORY_DEBUG
#define mem_init()					memdebug_init()
#define mem_stat()					memdebug_stat()
#define mem_alloc					memdebug_malloc0
#define mem_calloc					memdebug_calloc
#define mem_realloc					memdebug_realloc
#define mem_free(v)					memdebug_free((void *)v)
#define mem_check(v)				memdebug_setcheck(v)
#define mem_status					memdebug_status
#define mem_leaks()					memdebug_leaks()
#define mem_remove					memdebug_remove
#else
#define mem_init()
#define mem_stat()
#define mem_alloc(size)				calloc(1, size)
#define mem_calloc					calloc
#define mem_realloc					realloc
#define mem_free(v)					free((void *)v)
#define mem_check(v)
#define mem_status()				0
#define mem_leaks()					0
#define mem_remove(_v)
#endif

#if GRAVITY_MEMORY_DEBUG
void	memdebug_init(void);
void	*memdebug_malloc(size_t size);
void	*memdebug_malloc0(size_t size);
void	*memdebug_calloc(size_t num, size_t size);
void	*memdebug_realloc(void *ptr, size_t new_size);
void	memdebug_free(void *ptr);
size_t	memdebug_leaks (void);
size_t	memdebug_status (void);
void	memdebug_setcheck(bool flag);
void	memdebug_stat(void);
bool	memdebug_remove(void *ptr);
#endif

#endif
