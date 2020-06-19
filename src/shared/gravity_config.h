//
//  gravity_config.h
//  gravity
//
//  Created by Scott Ramsay on 18/6/20.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_CONFIG__
#define __GRAVITY_CONFIG__

#ifdef _MSC_VER

#pragma comment(lib, "shlwapi")
#include <basetsd.h>

#if (!defined(HAVE_BZERO) || !defined(bzero))
#define bzero(b, len) memset((b), 0, (len))
#endif

#if (!defined(HAVE_SNPRINTF) || !defined(snprintf))
#define snprintf    _snprintf
#endif

typedef SSIZE_T     ssize_t;
typedef int         mode_t;

#define open        _open
#define close       _close
#define read        _read
#define write       _write
#define __func__    __FUNCTION__

#else
#include <unistd.h>
#endif

#endif // __GRAVITY_CONFIG__
