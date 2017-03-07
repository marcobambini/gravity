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
typedef unsigned __int64 nanotime_t;
#define DIRREF			HANDLE
#else
#include <dirent.h>
typedef uint64_t		nanotime_t;
#define DIRREF			DIR*
#endif

// TIMER
nanotime_t	nanotime (void);
double		microtime (nanotime_t tstart, nanotime_t tend);
double		millitime (nanotime_t tstart, nanotime_t tend);

// FILE
uint64_t	file_size (const char *path);
const char	*file_read (const char *path, size_t *len);
bool		file_exists (const char *path);
const char	*file_buildpath (const char *filename, const char *dirpath);
bool		file_write (const char *path, const char *buffer, size_t len);

// DIRECTORY
bool		is_directory (const char *path);
DIRREF		directory_init (const char *path);
const char	*directory_read (DIRREF ref);

// STRING
int			string_nocasencmp (const char *s1, const char *s2, size_t n);
int			string_casencmp (const char *s1, const char *s2, size_t n);
int			string_cmp (const char *s1, const char *s2);
const char	*string_dup (const char *s1);
const char	*string_ndup (const char *s1, size_t n);
char		*string_unescape (const char *s1, uint32_t *s1len, char *buffer);
void		string_reverse (char *p);
uint32_t	string_size (const char *p);

// UTF-8
uint32_t	utf8_charbytes (const char *s, uint32_t i);
uint32_t	utf8_len (const char *s, uint32_t nbytes);
void		utf8_reverse (char *p);

// MATH and NUMBERS
uint32_t	power_of2_ceil (uint32_t n);
int64_t		number_from_hex (const char *s, uint32_t len);
int64_t		number_from_oct (const char *s, uint32_t len);
int64_t		number_from_bin (const char *s, uint32_t len);

#endif
