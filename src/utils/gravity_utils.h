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
nanotime_t  nanotime (void);
double      microtime (nanotime_t tstart, nanotime_t tend);
double      millitime (nanotime_t tstart, nanotime_t tend);

// FILE
int64_t     file_size (const char *path);
bool        file_exists (const char *path);
bool        file_delete (const char *path);
char        *file_read (const char *path, size_t *len);
bool        file_write (const char *path, const char *buffer, size_t len);
char        *file_buildpath (const char *filename, const char *dirpath);
char        *file_name_frompath (const char *path);

// DIRECTORY
bool        is_directory (const char *path);
bool        directory_create (const char *path);
DIRREF      directory_init (const char *path);
// On Windows, you are expected to provied a win32buffer buffer of at least MAX_PATH in length
char        *directory_read (DIRREF ref, char *win32buffer);
char        *directory_read_extend (DIRREF ref, char *win32buffer);

// STRING
int         string_nocasencmp (const char *s1, const char *s2, size_t n);
int         string_casencmp (const char *s1, const char *s2, size_t n);
int         string_cmp (const char *s1, const char *s2);
char		*string_dup (const char *s1);
char		*string_ndup (const char *s1, size_t n);
void        string_reverse (char *p);
uint32_t    string_size (const char *p);
char        *string_strnstr(const char *s, const char *find, size_t slen);
char        *string_replace(const char *str, const char *from, const char *to, size_t *rlen);

// UTF-8
uint32_t    utf8_charbytes (const char *s, uint32_t i);
uint32_t    utf8_nbytes (uint32_t n);
uint32_t    utf8_encode(char *buffer, uint32_t value);
uint32_t    utf8_len (const char *s, uint32_t nbytes);
bool        utf8_reverse (char *p);

// MATH and NUMBERS
uint32_t    power_of2_ceil (uint32_t n);
int64_t     number_from_hex (const char *s, uint32_t len);
int64_t     number_from_oct (const char *s, uint32_t len);
int64_t     number_from_bin (const char *s, uint32_t len);

#endif
