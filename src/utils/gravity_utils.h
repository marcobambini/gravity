//
//  gravity_utils.h
//  gravity
//
//  Created by Marco Bambini on 29/08/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_UTILS__
#define __GRAVITY_UTILS__

#include <stdint.h>
#include <stdbool.h>

#include "gravity_common.h"

#if defined(_WIN32)
#include <windows.h>
typedef unsigned __int64    nanotime_t;
#define DIRREF              HANDLE
#else
#include <dirent.h>
typedef uint64_t            nanotime_t;
#define DIRREF              DIR*
#endif

// TIMER
GRAVITY_API nanotime_t  nanotime (void);
GRAVITY_API double      microtime (nanotime_t tstart, nanotime_t tend);
GRAVITY_API double      millitime (nanotime_t tstart, nanotime_t tend);

// FILE
GRAVITY_API char       *file_buildpath (const char *filename, const char *dirpath);
GRAVITY_API bool        file_delete (const char *path);
GRAVITY_API bool        file_exists (const char *path);
GRAVITY_API char       *file_name_frompath (const char *path);
GRAVITY_API char       *file_read (const char *path, size_t *len);
GRAVITY_API int64_t     file_size (const char *path);
GRAVITY_API bool        file_write (const char *path, const char *buffer, size_t len);

// DIRECTORY
GRAVITY_API bool        directory_create (const char *path);
GRAVITY_API DIRREF      directory_init (const char *path);
// On Windows, you are expected to provied a win32buffer buffer of at least MAX_PATH in length
GRAVITY_API char       *directory_read (DIRREF ref, char *win32buffer);
GRAVITY_API char       *directory_read_extend (DIRREF ref, char *win32buffer);
GRAVITY_API bool        is_directory (const char *path);

// STRING
GRAVITY_API int         string_casencmp (const char *s1, const char *s2, size_t n);
GRAVITY_API int         string_cmp (const char *s1, const char *s2);
GRAVITY_API char       *string_dup (const char *s1);
GRAVITY_API char       *string_ndup (const char *s1, size_t n);
GRAVITY_API int         string_nocasencmp (const char *s1, const char *s2, size_t n);
GRAVITY_API uint32_t    string_size (const char *p);
GRAVITY_API char       *string_strnstr(const char *s, const char *find, size_t slen);
GRAVITY_API char       *string_replace(const char *str, const char *from, const char *to, size_t *rlen);
GRAVITY_API void        string_reverse (char *p);

// UTF-8
GRAVITY_API uint32_t    utf8_charbytes (const char *s, uint32_t i);
GRAVITY_API uint32_t    utf8_encode(char *buffer, uint32_t value);
GRAVITY_API uint32_t    utf8_len (const char *s, uint32_t nbytes);
GRAVITY_API uint32_t    utf8_nbytes (uint32_t n);
GRAVITY_API bool        utf8_reverse (char *p);

// MATH and NUMBERS
GRAVITY_API int64_t     number_from_bin (const char *s, uint32_t len);
GRAVITY_API int64_t     number_from_hex (const char *s, uint32_t len);
GRAVITY_API int64_t     number_from_oct (const char *s, uint32_t len);
GRAVITY_API uint32_t    power_of2_ceil (uint32_t n);

#endif
