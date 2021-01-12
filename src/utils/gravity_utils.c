//
//  gravity_utils.c
//  gravity
//
//  Created by Marco Bambini on 29/08/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

#if defined(__linux)
#include <sys/time.h>
#endif
#if defined(__MACH__)
#include <mach/mach_time.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#include <Shlwapi.h>
#include <tchar.h>
#include <io.h>
#endif
#if defined(__EMSCRIPTEN__)
#include <sys/time.h>
#endif

#include "gravity_utils.h"
#include "gravity_macros.h"
#include "gravity_memory.h"
#include "gravity_config.h"

#define SWP(x,y) (x^=y, y^=x, x^=y)

// MARK: Timer -

nanotime_t nanotime (void) {
    nanotime_t value;
    
    #if defined(_WIN32)
    static LARGE_INTEGER    win_frequency;
    QueryPerformanceFrequency(&win_frequency);
    LARGE_INTEGER            t;
    
    if (!QueryPerformanceCounter(&t)) return 0;
    value = (t.QuadPart / win_frequency.QuadPart) * 1000000000;
    value += (t.QuadPart % win_frequency.QuadPart) * 1000000000 / win_frequency.QuadPart;
    
    #elif defined(__MACH__)
    mach_timebase_info_data_t    info;
    kern_return_t                r;
    nanotime_t                    t;
    
    t = mach_absolute_time();
    r = mach_timebase_info(&info);
    if (r != 0) return 0;
    value = (t / info.denom) * info.numer;
    value += (t % info.denom) * info.numer / info.denom;
    
    #elif defined(__linux)
    struct timespec ts;
    int                r;
    
    r = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (r != 0) return 0;
    value = ts.tv_sec * (nanotime_t)1000000000 + ts.tv_nsec;
    
    #else
    struct timeval    tv;
    int                r;
    
    r = gettimeofday(&tv, 0);
    if (r != 0) return 0;
    value = tv.tv_sec * (nanotime_t)1000000000 + tv.tv_usec * 1000;
    #endif
    
    return value;
}

double microtime (nanotime_t tstart, nanotime_t tend) {
    nanotime_t t = tend - tstart;
    return ((double)t / 1000.0f);
}

double millitime (nanotime_t tstart, nanotime_t tend) {
    nanotime_t t = tend - tstart;
    return ((double)t / 1000000.0f);
}

// MARK: - I/O Functions -

int64_t file_size (const char *path) {
    #ifdef WIN32
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, (void*)&fileInfo) == 0) return -1;
    return (int64_t)(((__int64)fileInfo.nFileSizeHigh) << 32 ) + fileInfo.nFileSizeLow;
    #else
    struct stat sb;
    if (stat(path, &sb) < 0) return -1;
    return (int64_t)sb.st_size;
    #endif
}

bool file_exists (const char *path) {
    #ifdef WIN32
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) return true;
    #else
    if (access(path, F_OK) == 0) return true;
    #endif
    
    return false;
}

bool file_delete (const char *path) {
    #ifdef WIN32
    return DeleteFileA(path);
    #else
    if (unlink(path) == 0) return true;
    #endif
    
    return false;
}

char *file_read(const char *path, size_t *len) {
    int     fd = 0;
    off_t   fsize = 0;
    size_t  fsize2 = 0;
    char    *buffer = NULL;
    
    fsize = (off_t) file_size(path);
    if (fsize < 0) goto abort_read;
    
    fd = open(path, O_RDONLY);
    if (fd < 0) goto abort_read;
    
    buffer = (char *)mem_alloc(NULL, (size_t)fsize + 1);
    if (buffer == NULL) goto abort_read;
    buffer[fsize] = 0;
    
    fsize2 = read(fd, buffer, (size_t)fsize);
    if (fsize2 != fsize) goto abort_read;
    
    if (len) *len = fsize2;
    close(fd);
    return (char *)buffer;
    
abort_read:
    if (buffer) mem_free((void *)buffer);
    if (fd >= 0) close(fd);
    return NULL;
}

bool file_write (const char *path, const char *buffer, size_t len) {
    // RW for owner, R for group, R for others
    #ifdef _WIN32
    mode_t mode = _S_IWRITE;
    #else
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    #endif
    
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0) return false;
    
    ssize_t nwrite = (ssize_t)write(fd, buffer, len);
    close(fd);
    
    return (nwrite == len);
}

char *file_buildpath (const char *filename, const char *dirpath) {
    size_t len1 = (filename) ? strlen(filename) : 0;
    size_t len2 = (dirpath) ? strlen(dirpath) : 0;
    size_t len = len1 + len2 + 4;
    
    char *full_path = (char *)mem_alloc(NULL, len);
    if (!full_path) return NULL;
    
    #ifdef WIN32
    PathCombineA(full_path, filename, dirpath);
    #else
    // check if PATH_SEPARATOR exists in dirpath
    if ((len2) && (dirpath[len2-1] != PATH_SEPARATOR))
        snprintf(full_path, len, "%s/%s", dirpath, filename);
    else
        snprintf(full_path, len, "%s%s", dirpath, filename);
    #endif
    
    return full_path;
}

char *file_name_frompath (const char *path) {
    if (!path || (path[0] == 0)) return NULL;
    
    // must be sure to have a read-write memory address
	char *buffer = string_dup(path);
    if (!buffer) return false;
    
    char *name = NULL;
    size_t len = strlen(buffer);
    for (size_t i=len-1; i>0; --i) {
        if (buffer[i] == PATH_SEPARATOR) {
            buffer[i] = 0;
			name = string_dup(&buffer[i + 1]);
            break;
        }
    }
    return name;
}

// MARK: - Directory Functions -

bool is_directory (const char *path) {
    #ifdef WIN32
    DWORD dwAttrs = GetFileAttributesA(path);
    if (dwAttrs == INVALID_FILE_ATTRIBUTES) return false;
    if (dwAttrs & FILE_ATTRIBUTE_DIRECTORY) return true;
    #else
    struct stat buf;
    
    if (lstat(path, &buf) < 0) return false;
    if (S_ISDIR(buf.st_mode)) return true;
    #endif
    
    return false;
}

bool directory_create (const char *path) {
    #ifdef WIN32
    CreateDirectoryA(path, NULL);
    #else
    mode_t saved = umask(0);
    mkdir(path, 0775);
    umask(saved);
    #endif
    
    return file_exists(path);
}

DIRREF directory_init (const char *dirpath) {
	#ifdef WIN32
	WIN32_FIND_DATAW findData;
	WCHAR   path[MAX_PATH];
	WCHAR   dirpathW[MAX_PATH];
	
	// convert dirpath to dirpathW
	MultiByteToWideChar(CP_UTF8, 0, dirpath, -1, dirpathW, MAX_PATH);
	
	// in this way I can be sure that the first file returned (and lost) is .
	PathCombineW(path, dirpathW, L"*");
	
	// if the path points to a symbolic link, the WIN32_FIND_DATA buffer contains
	// information about the symbolic link, not the target
	return FindFirstFileW(path, &findData);
	#else
	return opendir(dirpath);
	#endif
}

char *directory_read (DIRREF ref, char *win32buffer) {
	if (ref == NULL) return NULL;
	
	while (1) {
		#ifdef WIN32
		WIN32_FIND_DATAA findData;
		
		if (FindNextFileA(ref, &findData) == 0) {
			FindClose(ref);
			return NULL;
		}
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
		if (findData.cFileName[0] == '\0') continue;
		if (findData.cFileName[0] == '.') continue;
		// cFileName from WIN32_FIND_DATAA is a fixed size array, and findData is local
		// This line of code is under the assumption that `win32buffer` is at least MAX_PATH in size!
		return !win32buffer ? NULL : memcpy(win32buffer, findData.cFileName, sizeof(findData.cFileName));
		#else
        UNUSED_PARAM(win32buffer);
		struct dirent *d;
		if ((d = readdir(ref)) == NULL) {
			closedir(ref);
			return NULL;
		}
		if (d->d_name[0] == '\0') continue;
		if (d->d_name[0] == '.') continue;
		return (char *)d->d_name;
		#endif
	}
	return NULL;
}

char *directory_read_extend (DIRREF ref, char *win32buffer) {
    if (ref == NULL) return NULL;
    
    while (1) {
        #ifdef WIN32
        WIN32_FIND_DATAA findData;
        
        if (FindNextFileA(ref, &findData) == 0) {
            FindClose(ref);
            return NULL;
        }
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (findData.cFileName[0] == '\0') continue;
        if (findData.cFileName[0] == '.') continue;
        // cFileName from WIN32_FIND_DATAA is a fixed size array, and findData is local
        // This line of code is under the assumption that `win32buffer` is at least MAX_PATH in size!
        return !win32buffer ? NULL : memcpy(win32buffer, findData.cFileName, sizeof(findData.cFileName));
        #else
        UNUSED_PARAM(win32buffer);
        struct dirent *d;
        if ((d = readdir(ref)) == NULL) {
            closedir(ref);
            return NULL;
        }
        if (d->d_name[0] == '\0') continue;
        if (strcmp(d->d_name, ".") == 0) continue;
        if (strcmp(d->d_name, "..") == 0) continue;
        return (char *)d->d_name;
        #endif
    }
    return NULL;
}

// MARK: - String Functions -

int string_nocasencmp(const char *s1, const char *s2, size_t n) {
    while(n > 0 && tolower((unsigned char)*s1) == tolower((unsigned char)*s2)) {
        if(*s1 == '\0') return 0;
        s1++;
        s2++;
        n--;
    }
    
    if(n == 0) return 0;
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int string_casencmp(const char *s1, const char *s2, size_t n) {
    while(n > 0 && ((unsigned char)*s1) == ((unsigned char)*s2)) {
        if(*s1 == '\0') return 0;
        s1++;
        s2++;
        n--;
    }
    
    if(n == 0) return 0;
    return ((unsigned char)*s1) - ((unsigned char)*s2);
}

int string_cmp (const char *s1, const char *s2) {
    if (!s1) return 1;
    return strcmp(s1, s2);
}

char *string_dup (const char *s1) {
    size_t len = (size_t)strlen(s1);
    char*s = (char *)mem_alloc(NULL, len + 1);
    if (!s) return NULL;
    memcpy(s, s1, len);
    return s;
}

char *string_ndup (const char *s1, size_t n) {
    char *s = (char *)mem_alloc(NULL, n + 1);
    if (!s) return NULL;
    memcpy(s, s1, n);
    return s;
}

// From: http://stackoverflow.com/questions/198199/how-do-you-reverse-a-string-in-place-in-c-or-c
void string_reverse (char *p) {
    char *q = p;
    while(q && *q) ++q; /* find eos */
    for(--q; p < q; ++p, --q) SWP(*p, *q);
}

uint32_t string_size (const char *p) {
    if (!p) return 0;
    return (uint32_t)strlen(p);
}

// From: https://opensource.apple.com/source/Libc/Libc-339/string/FreeBSD/strnstr.c
char *string_strnstr(const char *s, const char *find, size_t slen) {
    char c, sc;
    size_t len;
    
    if ((c = *find++) != '\0') {
        len = strlen(find);
        do {
            do {
                if ((sc = *s++) == '\0' || slen-- < 1)
                    return (NULL);
            } while (sc != c);
            if (len > slen)
                return (NULL);
        } while (strncmp(s, find, len) != 0);
        s--;
    }
    return ((char *)s);
}

// From: https://creativeandcritical.net/str-replace-c
char *string_replace(const char *str, const char *from, const char *to, size_t *rlen) {
    // cache related settings
    size_t cache_sz_inc = 16;
    const size_t cache_sz_inc_factor = 3;
    const size_t cache_sz_inc_max = 1048576;
    
    char *pret, *ret = NULL;
    const char *pstr2, *pstr = str;
    size_t i, count = 0;
    uintptr_t *pos_cache_tmp, *pos_cache = NULL;
    size_t cache_sz = 0;
    size_t cpylen, orglen, retlen = 0, tolen = 0, fromlen = strlen(from);
    if (rlen) *rlen = 0;
    
    // find all matches and cache their positions
    while ((pstr2 = strstr(pstr, from)) != NULL) {
        ++count;
        
        // Increase the cache size when necessary
        if (cache_sz < count) {
            cache_sz += cache_sz_inc;
            pos_cache_tmp = mem_realloc(NULL, pos_cache, sizeof(*pos_cache) * cache_sz);
            if (pos_cache_tmp == NULL) {
                goto end_repl_str;
            } else {
                pos_cache = pos_cache_tmp;
            }
            
            cache_sz_inc *= cache_sz_inc_factor;
            if (cache_sz_inc > cache_sz_inc_max) {
                cache_sz_inc = cache_sz_inc_max;
            }
        }
        
        pos_cache[count-1] = pstr2 - str;
        pstr = pstr2 + fromlen;
    }
    
    orglen = pstr - str + strlen(pstr);
    
    // allocate memory for the post-replacement string
    if (count > 0) {
        tolen = strlen(to);
        retlen = orglen + (tolen - fromlen) * count;
    } else {
        retlen = orglen;
    }
    ret = mem_alloc(NULL, retlen + 1);
    if (ret == NULL) {
        goto end_repl_str;
    }
    
    if (count == 0) {
        // if no matches, then just duplicate the string
        strcpy(ret, str);
    } else {
        //therwise, duplicate the string while performing the replacements using the position cache
        pret = ret;
        memcpy(pret, str, pos_cache[0]);
        pret += pos_cache[0];
        for (i = 0; i < count; ++i) {
            memcpy(pret, to, tolen);
            pret += tolen;
            pstr = str + pos_cache[i] + fromlen;
            cpylen = (i == count-1 ? orglen : pos_cache[i+1]) - pos_cache[i] - fromlen;
            memcpy(pret, pstr, cpylen);
            pret += cpylen;
        }
        ret[retlen] = '\0';
    }
    
end_repl_str:
    // free the cache and return the post-replacement string which will be NULL in the event of an error
    mem_free(pos_cache);
    if (rlen && ret) *rlen = retlen;
    return ret;
}

// MARK: - UTF-8 Functions -

/*
    Based on: https://github.com/Stepets/utf8.lua/blob/master/utf8.lua
    ABNF from RFC 3629

    UTF8-octets = *( UTF8-char )
    UTF8-char   = UTF8-1 / UTF8-2 / UTF8-3 / UTF8-4
    UTF8-1      = %x00-7F
    UTF8-2      = %xC2-DF UTF8-tail
    UTF8-3      = %xE0 %xA0-BF UTF8-tail / %xE1-EC 2( UTF8-tail ) /
                  %xED %x80-9F UTF8-tail / %xEE-EF 2( UTF8-tail )
    UTF8-4      = %xF0 %x90-BF 2( UTF8-tail ) / %xF1-F3 3( UTF8-tail ) /
                  %xF4 %x80-8F 2( UTF8-tail )
    UTF8-tail   = %x80-BF
 
 */

inline uint32_t utf8_charbytes (const char *s, uint32_t i) {
    unsigned char c = (unsigned char)s[i];
    
    // determine bytes needed for character, based on RFC 3629
    if ((c > 0) && (c <= 127)) return 1;
    if ((c >= 194) && (c <= 223)) return 2;
    if ((c >= 224) && (c <= 239)) return 3;
    if ((c >= 240) && (c <= 244)) return 4;
    
    // means error
    return 0;
}

uint32_t utf8_nbytes (uint32_t n) {
    if (n <= 0x7f) return 1;        // 127
    if (n <= 0x7ff) return 2;       // 2047
    if (n <= 0xffff) return 3;      // 65535
    if (n <= 0x10ffff) return 4;    // 1114111

    return 0;
}

// from: https://github.com/munificent/wren/blob/master/src/vm/wren_utils.c
uint32_t utf8_encode(char *buffer, uint32_t value) {
    char *bytes = buffer;
    
    if (value <= 0x7f) {
        // single byte (i.e. fits in ASCII).
        *bytes = (char)(value & 0x7f);
        return 1;
    }
    
    if (value <= 0x7ff) {
        // two byte sequence: 110xxxxx 10xxxxxx.
        *bytes = (char)(0xc0 | ((value & 0x7c0) >> 6));
        ++bytes;
        *bytes = (char)(0x80 | (value & 0x3f));
        return 2;
    }
    
    if (value <= 0xffff) {
        // three byte sequence: 1110xxxx 10xxxxxx 10xxxxxx.
        *bytes = (char)(0xe0 | ((value & 0xf000) >> 12));
        ++bytes;
        *bytes = (char)(0x80 | ((value & 0xfc0) >> 6));
        ++bytes;
        *bytes = (char)(0x80 | (value & 0x3f));
        return 3;
    }
    
    if (value <= 0x10ffff) {
        // four byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx.
        *bytes = (char)(0xf0 | ((value & 0x1c0000) >> 18));
        ++bytes;
        *bytes = (char)(0x80 | ((value & 0x3f000) >> 12));
        ++bytes;
        *bytes = (char)(0x80 | ((value & 0xfc0) >> 6));
        ++bytes;
        *bytes = (char)(0x80 | (value & 0x3f));
        return 4;
    }
    
    return 0;
}

uint32_t utf8_len (const char *s, uint32_t nbytes) {
    if (nbytes == 0) nbytes = (uint32_t)strlen(s);
    
    uint32_t pos = 0;
    uint32_t len = 0;
    
    while (pos < nbytes) {
        ++len;
        uint32_t n = utf8_charbytes(s, pos);
        if (n == 0) return 0; // means error
        pos += n;
    }
    
    return len;
}

// From: http://stackoverflow.com/questions/198199/how-do-you-reverse-a-string-in-place-in-c-or-c
bool utf8_reverse (char *p) {
    char *q = p;
    string_reverse(p);
    
    // now fix bass-ackwards UTF chars.
    while(q && *q) ++q; // find eos
    while(p < --q)
        switch( (*q & 0xF0) >> 4 ) {
            case 0xF: /* U+010000-U+10FFFF: four bytes. */
                if (q-p < 4) return false;
                SWP(*(q-0), *(q-3));
                SWP(*(q-1), *(q-2));
                q -= 3;
                break;
            case 0xE: /* U+000800-U+00FFFF: three bytes. */
                if (q-p < 3) return false;
                SWP(*(q-0), *(q-2));
                q -= 2;
                break;
            case 0xC: /* fall-through */
            case 0xD: /* U+000080-U+0007FF: two bytes. */
                if (q-p < 1) return false;
                SWP(*(q-0), *(q-1));
                q--;
                break;
        }
    return true;
}

// MARK: - Math -

// From: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2Float
// WARNING: this function returns 0 if n is greater than 2^31
uint32_t power_of2_ceil (uint32_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    
    return n;
}

int64_t number_from_hex (const char *s, uint32_t len) {
    // LLONG_MIN  = -9223372036854775808
    // LLONG_MAX  = +9223372036854775807
    // HEX(9223372036854775808) = 346DC5D638865
    
    // sanity check on len in order to workaround an address sanitizer error
    if (len > 24) return 0;
    return (int64_t) strtoll(s, NULL, 16);
}

int64_t number_from_oct (const char *s, uint32_t len) {
    // LLONG_MIN  = -9223372036854775808
    // LLONG_MAX  = +9223372036854775807
    // OCT(9223372036854775808) = 32155613530704145
    
    // sanity check on len in order to workaround an address sanitizer error
    if (len > 24) return 0;
    return (int64_t) strtoll(s, NULL, 8);
}

int64_t number_from_bin (const char *s, uint32_t len) {
    // LLONG_MIN  = -9223372036854775808
    // LLONG_MAX  = +9223372036854775807
    // BIN(9223372036854775808) = 11010001101101110001011101011000111000100001100101
    
    // sanity check on len
    if (len > 64) return 0;
    int64_t value = 0;
    for (uint32_t i=0; i<len; ++i) {
        int c = s[i];
        value = (value << 1) + (c - '0');
    }
    return value;
}

