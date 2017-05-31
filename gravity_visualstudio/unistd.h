#pragma once
#pragma comment(lib, "Shlwapi.lib")

#include <basetsd.h>
#include <io.h>
#include <stdio.h>

#define bzero(b, len) memset((b), 0, (len))

typedef SSIZE_T ssize_t;
typedef int mode_t;

#define open _open
#define close _close
#define read _read
#define write _write

#define snprintf _snprintf
#define __func__ __FUNCTION__

// Fix for Visual Studio