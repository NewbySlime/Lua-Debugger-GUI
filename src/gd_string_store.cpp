#include "gd_string_store.h"


void gd_string_store::append(const char* data){
  this->data += data;
}

void gd_string_store::append(const char* data, std::size_t length){
  this->data += std::string(data, length).c_str();
}