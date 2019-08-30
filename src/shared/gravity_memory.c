//
//  gravity_memory.c
//    gravity
//
//  Created by Marco Bambini on 20/03/16.
//  Copyright © 2016 Creolabs. All rights reserved.
//

#include "gravity_memory.h"
#include "gravity_vm.h"

#if !GRAVITY_MEMORY_DEBUG

void *gravity_calloc(gravity_vm *vm, size_t count, size_t size) {
    if (vm && ((count * size) >= gravity_vm_maxmemblock(vm))) {
        gravity_vm_seterror(vm, "Maximum memory allocation block size reached (req: %d, max: %lld).", (count * size), (int64_t)gravity_vm_maxmemblock(vm));
        return NULL;
    }
    return calloc(count, size);
}

void *gravity_realloc(gravity_vm *vm, void *ptr, size_t new_size) {
    if (vm && (new_size >= gravity_vm_maxmemblock(vm))) {
        gravity_vm_seterror(vm, "Maximum memory re-allocation block size reached (req: %d, max: %lld).", new_size, (int64_t)gravity_vm_maxmemblock(vm));
        return NULL;
    }
    return realloc(ptr, new_size);
}

#else
#if _WIN32
#include <imagehlp.h>
#else
#include <execinfo.h>
#endif

static void _ptr_add (void *ptr, size_t size);
static void _ptr_replace (void *old_ptr, void *new_ptr, size_t new_size);
static void _ptr_remove (void *ptr);
static uint32_t _ptr_lookup (void *ptr);
static char **_ptr_stacktrace (size_t *nframes);
static bool _is_internal(const char *s);

#define STACK_DEPTH                128
#define SLOT_MIN                128
#define SLOT_NOTFOUND            UINT32_MAX
#define BUILD_ERROR(...)        char current_error[1024]; snprintf(current_error, sizeof(current_error), __VA_ARGS__)
#define BUILD_STACK(v1,v2)        size_t v1; char **v2 = _ptr_stacktrace(&v1)
#define CHECK_FLAG()            if (!check_flag) return

typedef struct {
    bool                    deleted;
    void                    *ptr;
    size_t                    size;
    size_t                    nrealloc;

    // record where it has been allocated/reallocated
    size_t                    nframe;
    char                    **frames;

    // record where is has been freed
    size_t                    nframe2;
    char                    **frames2;
} memslot;

typedef struct {
    uint32_t                nalloc;
    uint32_t                nrealloc;
    uint32_t                nfree;
    uint32_t                currmem;
    uint32_t                maxmem;
    uint32_t                nslot;        // number of slot filled with data
    uint32_t                aslot;        // number of allocated slot
    memslot                    *slot;
} _memdebug;

static _memdebug memdebug;
static bool check_flag = true;

static void memdebug_report (char *str, char **stack, size_t nstack, memslot *slot) {
    printf("%s\n", str);
    for (size_t i=0; i<nstack; ++i) {
        if (_is_internal(stack[i])) continue;
        printf("%s\n", stack[i]);
    }

    if (slot) {
        printf("\nallocated:\n");
        for (size_t i=0; i<slot->nframe; ++i) {
            if (_is_internal(slot->frames[i])) continue;
            printf("%s\n", slot->frames[i]);
        }

        printf("\nfreed:\n");
        for (size_t i=0; i<slot->nframe2; ++i) {
            if (_is_internal(slot->frames2[i])) continue;
            printf("%s\n", slot->frames2[i]);
        }
    }

    abort();
}

void memdebug_init (void) {
    if (memdebug.slot) free(memdebug.slot);
    bzero(&memdebug, sizeof(_memdebug));

    memdebug.slot = (memslot *) malloc(sizeof(memslot) * SLOT_MIN);
    memdebug.aslot = SLOT_MIN;
}

void *memdebug_malloc(gravity_vm *vm, size_t size) {
    #pragma unused(vm)
    void *ptr = malloc(size);
    if (!ptr) {
        BUILD_ERROR("Unable to allocated a block of %zu bytes", size);
        BUILD_STACK(n, stack);
        memdebug_report(current_error, stack, n, NULL);
        return NULL;
    }

    _ptr_add(ptr, size);
    return ptr;
}

void *memdebug_malloc0(gravity_vm *vm, size_t size) {
    #pragma unused(vm)
    return memdebug_calloc(vm, 1, size);
}

void *memdebug_calloc(gravity_vm *vm, size_t num, size_t size) {
    #pragma unused(vm)
    void *ptr = calloc(num, size);
    if (!ptr) {
        BUILD_ERROR("Unable to allocated a block of %zu bytes", size);
        BUILD_STACK(n, stack);
        memdebug_report(current_error, stack, n, NULL);
        return NULL;
    }

    _ptr_add(ptr, num*size);
    return ptr;
}

void *memdebug_realloc(gravity_vm *vm, void *ptr, size_t new_size) {
    #pragma unused(vm)
    // ensure ptr has been previously allocated by malloc, calloc or realloc and not yet freed with free
    uint32_t index = _ptr_lookup(ptr);
    if (index == SLOT_NOTFOUND) {
        BUILD_ERROR("Pointer being reallocated was now previously allocated");
        BUILD_STACK(n, stack);
        memdebug_report(current_error, stack, n, NULL);
        return NULL;
    }

    void *new_ptr = realloc(ptr, new_size);
    if (!ptr) {
        BUILD_ERROR("Unable to reallocate a block of %zu bytes", new_size);
        BUILD_STACK(n, stack);
        memdebug_report(current_error, stack, n, &memdebug.slot[index]);
        return NULL;
    }

    _ptr_replace(ptr, new_ptr, new_size);
    return new_ptr;
}

bool memdebug_remove(void *ptr) {
    // ensure ptr has been previously allocated by malloc, calloc or realloc and not yet freed with free
    if (check_flag) {
        uint32_t index = _ptr_lookup(ptr);
        if (index == SLOT_NOTFOUND) {
            BUILD_ERROR("Pointer being freed was not previously allocated");
            BUILD_STACK(n, stack);
            memdebug_report(current_error, stack, n, NULL);
            return false;
        }

        memslot m = memdebug.slot[index];
        if (m.deleted) {
            BUILD_ERROR("Pointer already freed");
            BUILD_STACK(n, stack);
            memdebug_report(current_error, stack, n, &m);
            return false;
        }
    }

    _ptr_remove(ptr);
    return true;
}

void memdebug_free(void *ptr) {
    if (!memdebug_remove(ptr)) return;
    free(ptr);
}
void memdebug_setcheck(bool flag) {
    check_flag = flag;
}

void memdebug_stat(void) {
    printf("\n========== MEMORY STATS ==========\n");
    printf("Allocations count: %d\n", memdebug.nalloc);
    printf("Reallocations count: %d\n", memdebug.nrealloc);
    printf("Free count: %d\n", memdebug.nfree);
    printf("Leaked: %d (bytes)\n", memdebug.currmem);
    printf("Max memory usage: %d (bytes)\n", memdebug.maxmem);
    printf("==================================\n\n");

    if (memdebug.currmem > 0) {
        printf("\n");
        for (uint32_t i=0; i<memdebug.nslot; ++i) {
            memslot m = memdebug.slot[i];
            if ((!m.ptr) || (m.deleted)) continue;

            printf("Block %p size: %zu (reallocated %zu)\n", m.ptr, m.size, m.nrealloc);
            printf("Call stack:\n");
            printf("===========\n");
            for (size_t j=0; j<m.nframe; ++j) {
                if (_is_internal(m.frames[j])) continue;
                printf("%s\n", m.frames[j]);
            }
            printf("===========\n\n");
        }
    }
}

size_t memdebug_status (void) {
    return memdebug.maxmem;
}

size_t memdebug_leaks (void) {
    return memdebug.currmem;
}

// Internals
void _ptr_add (void *ptr, size_t size) {
    CHECK_FLAG();

    if (memdebug.nslot + 1 >= memdebug.aslot) {
        size_t old_size = sizeof(memslot) * memdebug.nslot;
        size_t new_size = sizeof(memslot) * SLOT_MIN;
        memslot *new_slot = (memslot *) realloc(memdebug.slot, old_size+new_size);
        if (!new_slot) {
            BUILD_ERROR("Unable to reallocate internal slots");
            memdebug_report(current_error, NULL, 0, NULL);
            abort();
        }
        memdebug.slot = new_slot;
        memdebug.aslot += SLOT_MIN;
    }

    memslot slot = {
        .deleted = false,
        .ptr = ptr,
        .size = size,
        .nrealloc = 0,
        .nframe2 = 0,
        .frames = NULL
    };
    slot.frames = _ptr_stacktrace(&slot.nframe);

    memdebug.slot[memdebug.nslot] = slot;
    ++memdebug.nslot;

    ++memdebug.nalloc;
    memdebug.currmem += size;
    if (memdebug.currmem > memdebug.maxmem)
        memdebug.maxmem = memdebug.currmem;
}

void _ptr_replace (void *old_ptr, void *new_ptr, size_t new_size) {
    CHECK_FLAG();

    uint32_t index = _ptr_lookup(old_ptr);

    if (index == SLOT_NOTFOUND) {
        BUILD_ERROR("Unable to find old pointer to realloc");
        memdebug_report(current_error, NULL, 0, NULL);
    }

    memslot slot = memdebug.slot[index];
    if (slot.deleted) {
        BUILD_ERROR("Pointer already freed");
        BUILD_STACK(n, stack);
        memdebug_report(current_error, stack, n, &slot);
    }
    size_t old_size = memdebug.slot[index].size;
    slot.ptr = new_ptr;
    slot.size = new_size;
    if (slot.frames) free((void *)slot.frames);
    slot.frames = _ptr_stacktrace(&slot.nframe);
    ++slot.nrealloc;

    memdebug.slot[index] = slot;
    ++memdebug.nrealloc;
    memdebug.currmem += (new_size - old_size);
    if (memdebug.currmem > memdebug.maxmem)
        memdebug.maxmem = memdebug.currmem;
}

void _ptr_remove (void *ptr) {
    CHECK_FLAG();

    uint32_t index = _ptr_lookup(ptr);

    if (index == SLOT_NOTFOUND) {
        BUILD_ERROR("Unable to find old pointer to realloc");
        memdebug_report(current_error, NULL, 0, NULL);
    }

    memslot slot = memdebug.slot[index];
    if (slot.deleted) {
        BUILD_ERROR("Pointer already freed");
        BUILD_STACK(n, stack);
        memdebug_report(current_error, stack, n, &slot);
    }

    size_t old_size = memdebug.slot[index].size;
    memdebug.slot[index].deleted = true;
    memdebug.slot[index].frames2 = _ptr_stacktrace(&memdebug.slot[index].nframe2);

    ++memdebug.nfree;
    memdebug.currmem -= old_size;
}

uint32_t _ptr_lookup (void *ptr) {
    for (uint32_t i=0; i<memdebug.nslot; ++i) {
        if (memdebug.slot[i].ptr == ptr) return i;
    }
    return SLOT_NOTFOUND;
}

char **_ptr_stacktrace (size_t *nframes) {
    #if _WIN32
    // http://www.codeproject.com/Articles/11132/Walking-the-callstack
    // https://spin.atomicobject.com/2013/01/13/exceptions-stack-traces-c/
    #else
    void *callstack[STACK_DEPTH];
    int n = backtrace(callstack, STACK_DEPTH);
    char **strs = backtrace_symbols(callstack, n);
    *nframes = (size_t)n;
    return strs;
    #endif
}

// Default callback
bool _is_internal(const char *s) {
    static const char *reserved[] = {"??? ", "libdyld.dylib ", "memdebug_", "_ptr_", NULL};

    const char **r = reserved;
    while (*r) {
        if (strstr(s, *r)) return true;
        ++r;
    }
    return false;
}

#undef STACK_DEPTH
#undef SLOT_MIN
#undef SLOT_NOTFOUND
#undef BUILD_ERROR
#undef BUILD_STACK
#undef CHECK_FLAG

#endif



