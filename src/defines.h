#ifndef DEFINES_HEADER
#define DEFINES_HEADER


#ifdef _WIN64
#include "windows.h"

#define __LOCK_MUTEX(mutex) WaitForSingleObject(mutex, INFINITE)
#define __RELEASE_MUTEX(mutex) ReleaseMutex(mutex)
#endif


#define GDSTR_AS_PRIMITIVE(str) (const char*)str.to_utf8_buffer().ptr()

#endif