#include "gdvariant_util.h"


using namespace godot;


PackedByteArray convert_to_variant(const void* data, size_t data_len){
  PackedByteArray _res;

  const uint8_t* _bdata = (const uint8_t*)data;
  for(int i = 0; i < data_len; i++)
    _res.append(_bdata[i]);

  return _res;
}


void parse_variant_data(const Variant& data, void* buffer, size_t buffer_len){
  PackedByteArray _byte_data = data;

  uint8_t* _bbuffer = (uint8_t*)buffer;
  for(int i = 0; i < _byte_data.size(); i++)
    _bbuffer[i] = _byte_data[i];
}