#ifndef ERROR_UTIL_HEADER
#define ERROR_UTIL_HEADER

#include "string"

#if (_WIN64) || (_WIN32)
#include "Windows.h"
#endif


namespace error::util{
#if (_WIN64) || (_WIN32)
  std::string get_windows_error_message(DWORD error_code);
#endif
}

#endif