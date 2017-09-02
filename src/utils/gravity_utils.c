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
#include <string.h>
#include <assert.h>
#include <unistd.h>
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
#endif

#include "gravity_utils.h"
#include "gravity_memory.h"

#define SWP(x,y) (x^=y, y^=x, x^=y)

// MARK: Timer -

nanotime_t nanotime (void) {
	nanotime_t value;

	#if defined(_WIN32)
	static LARGE_INTEGER	win_frequency;
	QueryPerformanceFrequency(&win_frequency);
	LARGE_INTEGER			t;

	if (!QueryPerformanceCounter(&t)) return 0;
	value = (t.QuadPart / win_frequency.QuadPart) * 1000000000;
	value += (t.QuadPart % win_frequency.QuadPart) * 1000000000 / win_frequency.QuadPart;

	#elif defined(__MACH__)
	mach_timebase_info_data_t	info;
	kern_return_t				r;
	nanotime_t					t;

	t = mach_absolute_time();
	r = mach_timebase_info(&info);
	if (r != 0) return 0;
	value = (t / info.denom) * info.numer;
	value += (t % info.denom) * info.numer / info.denom;

	#elif defined(__linux)
	struct timespec ts;
	int				r;

	r = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (r != 0) return 0;
	value = ts.tv_sec * (nanotime_t)1000000000 + ts.tv_nsec;

	#else
	struct timeval	tv;
	int				r;

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

// MARK: - Console Functions -

#ifdef WIN32
// getline is a POSIX function not available in C on Windows (only C++)
static ssize_t getline (char **lineptr, size_t *n, FILE *stream) {
	// to be implemented on Windows
	// Never use gets: it offers no protections against a buffer overflow vulnerability.
	// see http://stackoverflow.com/questions/3302255/c-scanf-vs-gets-vs-fgets
	// we should implement something like ggets here
	// http://web.archive.org/web/20080525133110/http://cbfalconer.home.att.net/download/

	return -1;
}
#endif

char *readline (char *prompt, int *length) {
	char	*line = NULL;
	size_t	size = 0;

	printf("%s", prompt);
	fflush(stdout);

	ssize_t nread = getline(&line, &size, stdin);
	if (nread == -1 || feof(stdin)) return NULL;

	*length = (int)nread;
	return line;
}

// MARK: - I/O Functions -

uint64_t file_size (const char *path) {
	#ifdef WIN32
	WIN32_FILE_ATTRIBUTE_DATA   fileInfo;
	if (GetFileAttributesExA(path, GetFileExInfoStandard, (void*)&fileInfo) == 0) return -1;
	return (uint64_t)(((__int64)fileInfo.nFileSizeHigh) << 32 ) + fileInfo.nFileSizeLow;
	#else
	struct stat sb;
	if (stat(path, &sb) > 0) return -1;
	return (uint64_t)sb.st_size;
	#endif
}

const char *file_read(const char *path, size_t *len) {
	int		fd = 0;
	off_t	fsize = 0;
	size_t	fsize2 = 0;
	char	*buffer = NULL;

	fsize = (size_t) file_size(path);
	if (fsize < 0) goto abort_read;

	fd = open(path, O_RDONLY);
	if (fd < 0) goto abort_read;

	buffer = (char *)mem_alloc((size_t)fsize + 1);
	if (buffer == NULL) goto abort_read;
	buffer[fsize] = 0;

	fsize2 = read(fd, buffer, (size_t)fsize);
	if (fsize2 == -1) goto abort_read;

	if (len) *len = fsize2;
	close(fd);
	return (const char *)buffer;

abort_read:
	if (buffer) mem_free((void *)buffer);
	if (fd >= 0) close(fd);
	return NULL;
}

bool file_exists (const char *path) {
	#ifdef WIN32
	BOOL isDirectory;
	DWORD attributes = GetFileAttributesA(path);

	// special directory case to drive the network path check
	if (attributes == INVALID_FILE_ATTRIBUTES)
		isDirectory = (GetLastError() == ERROR_BAD_NETPATH);
	else
		isDirectory = (FILE_ATTRIBUTE_DIRECTORY & attributes);

	if (isDirectory) {
		if (PathIsNetworkPathA(path)) return true;
		if (PathIsUNCA(path)) return true;
	}

	if (PathFileExistsA(path) == 1) return true;
	#else
	if (access(path, F_OK)==0) return true;
	#endif

	return false;
}

const char *file_buildpath (const char *filename, const char *dirpath) {
//	#ifdef WIN32
//	PathCombineA(result, filename, dirpath);
//	#else
	size_t len1 = strlen(filename);
	size_t len2 = strlen(dirpath);
	size_t len = len1+len2+2;

	char *full_path = (char *)mem_alloc(len);
	if (!full_path) return NULL;

	if ((len2) && (dirpath[len2-1] != '/'))
		snprintf(full_path, len, "%s/%s", dirpath, filename);
	else
		snprintf(full_path, len, "%s%s", dirpath, filename);
//	#endif

	return (const char *)full_path;
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

	ssize_t nwrite = write(fd, buffer, len);
	close(fd);

	return (nwrite == len);
}

// MARK: - Directory Functions -

bool is_directory (const char *path) {
	#ifdef WIN32
	DWORD dwAttrs;

	dwAttrs = GetFileAttributesA(path);
	if (dwAttrs == INVALID_FILE_ATTRIBUTES) return false;
	if (dwAttrs & FILE_ATTRIBUTE_DIRECTORY) return true;
	#else
	struct stat buf;

	if (lstat(path, &buf) < 0) return false;
	if (S_ISDIR(buf.st_mode)) return true;
	#endif

	return false;
}

DIRREF directory_init (const char *dirpath) {
	#ifdef WIN32
	WIN32_FIND_DATA findData;
	WCHAR			path[MAX_PATH];
	WCHAR			dirpathW[MAX_PATH];
	HANDLE			hFind;
	(void)hFind;

	// convert dirpath to dirpathW
	MultiByteToWideChar(CP_UTF8, 0, dirpath, -1, dirpathW, MAX_PATH);

	// in this way I can be sure that the first file returned (and lost) is .
	PathCombineW(path, dirpathW, _T("*"));

	// if the path points to a symbolic link, the WIN32_FIND_DATA buffer contains
	// information about the symbolic link, not the target
	return FindFirstFileW(path, &findData);
	#else
	return opendir(dirpath);
	#endif
}

const char *directory_read (DIRREF ref) {
	if (ref == NULL) return NULL;

	while (1) {
		#ifdef WIN32
		WIN32_FIND_DATA findData;
		char 			*file_name;

		if (FindNextFile(ref, &findData) == 0) {
			FindClose(ref);
			return NULL;
		}
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
		if (findData.cFileName == NULL) continue;
		if (findData.cFileName[0] == '.') continue;
		return (const char*)findData.cFileName;
		#else
		struct dirent *d;
		if ((d = readdir(ref)) == NULL) {
			closedir(ref);
			return NULL;
		}
		if (d->d_name[0] == 0) continue;
		if (d->d_name[0] == '.') continue;
		return (const char *)d->d_name;
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

const char *string_dup (const char *s1) {
	size_t	len = (size_t)strlen(s1);
	char	*s = (char *)mem_alloc(len + 1);

	memcpy(s, s1, len);
	return s;
}

const char *string_ndup (const char *s1, size_t n) {
	char *s = (char *)mem_alloc(n + 1);
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
	unsigned char c = s[i];

	// determine bytes needed for character, based on RFC 3629
	if ((c > 0) && (c <= 127)) return 1;
	if ((c >= 194) && (c <= 223)) return 2;
	if ((c >= 224) && (c <= 239)) return 3;
	if ((c >= 240) && (c <= 244)) return 4;

	// means error
	return 0;
}

uint32_t utf8_nbytes (uint32_t n) {
	if (n <= 0x7f) return 1;		// 127
	if (n <= 0x7ff) return 2;		// 2047
	if (n <= 0xffff) return 3;		// 65535
	if (n <= 0x10ffff) return 4;	// 1114111

	return 0;
}

// from: https://github.com/munificent/wren/blob/master/src/vm/wren_utils.c
uint32_t utf8_encode(char *buffer, uint32_t value) {
	char *bytes = buffer;

	if (value <= 0x7f) {
		// single byte (i.e. fits in ASCII).
		*bytes = value & 0x7f;
		return 1;
	}

	if (value <= 0x7ff) {
		// two byte sequence: 110xxxxx 10xxxxxx.
		*bytes = 0xc0 | ((value & 0x7c0) >> 6);
		++bytes;
		*bytes = 0x80 | (value & 0x3f);
		return 2;
	}

	if (value <= 0xffff) {
		// three byte sequence: 1110xxxx 10xxxxxx 10xxxxxx.
		*bytes = 0xe0 | ((value & 0xf000) >> 12);
		++bytes;
		*bytes = 0x80 | ((value & 0xfc0) >> 6);
		++bytes;
		*bytes = 0x80 | (value & 0x3f);
		return 3;
	}

	if (value <= 0x10ffff) {
		// four byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx.
		*bytes = 0xf0 | ((value & 0x1c0000) >> 18);
		++bytes;
		*bytes = 0x80 | ((value & 0x3f000) >> 12);
		++bytes;
		*bytes = 0x80 | ((value & 0xfc0) >> 6);
		++bytes;
		*bytes = 0x80 | (value & 0x3f);
		return 4;
	}

	return 0;
}

uint32_t utf8_len (const char *s, uint32_t nbytes) {
	if (nbytes == 0) nbytes = (uint32_t)strlen(s);

	uint32_t pos = 1;
	uint32_t len = 0;

	while (pos <= nbytes) {
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
	#pragma unused(len)
	return (int64_t) strtoll(s, NULL, 16);
}

int64_t number_from_oct (const char *s, uint32_t len) {
	#pragma unused(len)
	return (int64_t) strtoll(s, NULL, 8);
}

int64_t number_from_bin (const char *s, uint32_t len) {
	int64_t value = 0;

	for (uint32_t i=0; i<len; ++i) {
		int c = s[i];
		value = (value << 1) + (c - '0');
	}
	return value;
}

