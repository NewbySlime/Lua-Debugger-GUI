#ifndef GDVARIANT_UTIL_HEADER
#define GDVARIANT_UTIL_HEADER

#include "godot_cpp/variant/variant.hpp"

// WARNING, do not use C++ objects/classes. Structs shouldn't also have C++ objects. If needed to pass C++ objects, pass pointers as the parameter.

godot::PackedByteArray convert_to_variant(const void* data, size_t data_len);
template<typename T_data> godot::PackedByteArray convert_to_variant(const T_data* data){
  return convert_to_variant(data, sizeof(T_data));
}

void parse_variant_data(const godot::Variant& data, void* buffer, size_t buffer_len);
template<typename T_data> T_data parse_variant_data(const godot::Variant& data){
  T_data _res;
  parse_variant_data(data, &_res, sizeof(T_data));

  return _res;
}

#endif