#include "gdstring_store.h"


using namespace godot;


gd_string_store::gd_string_store(const char* cstr){
  data = cstr;
}

gd_string_store::gd_string_store(const String& gdstr){
  data = gdstr;
}


void gd_string_store::append(const char* data){
  this->data += data;
}

void gd_string_store::append(const char* data, std::size_t length){
  this->data += std::string(data, length).c_str();
}