#ifndef __GRAVITY_COMMON__
#define __GRAVITY_COMMON__

//DLL export/import support for Windows
#if !defined(GRAVITY_API) && defined(_WIN32) && defined(BUILD_GRAVITY_API)
  #define GRAVITY_API __declspec(dllexport)
#else
  #define GRAVITY_API
#endif

#endif
