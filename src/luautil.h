#ifndef LUAUTIL_HEADER
#define LUAUTIL_HEADER

#include "Lua-CPPAPI/Src/luavariant.h"


namespace lua::util{
  bool is_reference_variant(const lua::I_variant* var);
  const void* get_reference_pointer(const lua::I_variant* var);
}

#endif