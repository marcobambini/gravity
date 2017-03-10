#pragma once
#pragma comment(lib, "Shlwapi.lib")

#include <basetsd.h>
#include <io.h>
#include <stdio.h>

#define bzero(b, len) memset((b), 0, (len))

typedef SSIZE_T ssize_t;
typedef int mode_t;

// Fix for Visual Studio