//
//  gravity_memory.h
//    gravity
//
//  Created by Marco Bambini on 20/03/16.
//  Copyright © 2016 Creolabs. All rights reserved.
//

#ifndef __GRAVITY_MEMORY__
#define __GRAVITY_MEMORY__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// memory debugger must be turned on ONLY with Xcode GuardMalloc ON
#define GRAVITY_MEMORY_DEBUG            0

#ifndef GRAVITY_VM_DEFINED
#define GRAVITY_VM_DEFINED
typedef struct gravity_vm                gravity_vm;
#endif

#if GRAVITY_MEMORY_DEBUG
#define mem_init()                      memdebug_init()
#define mem_stat()                      memdebug_stat()
#define mem_alloc(_vm,_size)            memdebug_malloc0(_vm,_size)
#define mem_calloc(_vm,_count,_size)    memdebug_calloc(_vm,_count,_size)
#define mem_realloc(_vm,_ptr,_size)     memdebug_realloc(_vm,_ptr,_size)
#define mem_free(v)                     memdebug_free((void *)v)
#define mem_check(v)                    memdebug_setcheck(v)
#define mem_status                      memdebug_status
#define mem_leaks()                     memdebug_leaks()
#define mem_remove                      memdebug_remove
#else
#define mem_init()
#define mem_stat()
#define mem_alloc(_vm,_size)            gravity_calloc(_vm, 1, _size)
#define mem_calloc(_vm,_count,_size)    gravity_calloc(_vm, _count, _size)
#define mem_realloc(_vm,_ptr,_size)     gravity_realloc(_vm, _ptr, _size)
#define mem_free(v)                     free((void *)v)
#define mem_check(v)
#define mem_status()                    0
#define mem_leaks()                     0
#define mem_remove(_v)
#endif

#if GRAVITY_MEMORY_DEBUG
void    memdebug_init (void);
void    *memdebug_malloc (gravity_vm *vm, size_t size);
void    *memdebug_malloc0 (gravity_vm *vm, size_t size);
void    *memdebug_calloc (gravity_vm *vm, size_t num, size_t size);
void    *memdebug_realloc (gravity_vm *vm, void *ptr, size_t new_size);
void    memdebug_free (void *ptr);
size_t  memdebug_leaks (void);
size_t  memdebug_status (void);
void    memdebug_setcheck (bool flag);
void    memdebug_stat (void);
bool    memdebug_remove (void *ptr);
#else
void    *gravity_calloc (gravity_vm *vm, size_t count, size_t size);
void    *gravity_realloc (gravity_vm *vm, void *ptr, size_t new_size);
#endif

#endif
