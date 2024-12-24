#include "gdutils.h"
#include "strutil.h"

using namespace gdutils;
using namespace godot;


String gdutils::as_hex(const Color& col, bool include_alpha){
  uint32_t _col =
    ((uint32_t)(col.r*255) << 16) |
    ((uint32_t)(col.g*255) << 8) |
    ((uint32_t)(col.b*255))
  ;

  if(include_alpha){
    _col = (_col << 2) | (uint32_t)(col.a*255);
    return format_str("%08x", _col).c_str();
  }
  
  return format_str("%06x", _col).c_str();
}