#include "global_variables.h"

using namespace gdutils;
using namespace godot;


const char* GlobalVariables::singleton_path = "/root/GlobalUserVariables";

const char* GlobalVariables::s_global_value_set = "value_set";


void GlobalVariables::_bind_methods(){
  ClassDB::bind_method(D_METHOD("set_global_data", "data"), &GlobalVariables::set_global_data);
  ClassDB::bind_method(D_METHOD("get_global_data"), &GlobalVariables::get_global_data);

  ADD_SIGNAL(MethodInfo(GlobalVariables::s_global_value_set, PropertyInfo(Variant::STRING, "key"), PropertyInfo(Variant::NIL, "value")));

  ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "global_data"), "set_global_data", "get_global_data");
}


void GlobalVariables::set_global_value(const String& key, const Variant& value){
  _variable_data[key] = value;
  emit_signal(GlobalVariables::s_global_value_set, key, value);
}

Variant GlobalVariables::get_global_value(const String& key) const{
  if(!_variable_data.has(key))
    return Variant();

  return _variable_data[key];
}

bool GlobalVariables::has_global_value(const String& key) const{
  return _variable_data.has(key);
}


const VariantTypeParser& GlobalVariables::get_type_parser() const{
  return _type_parser;
}


void GlobalVariables::set_global_data(const Dictionary& data){
  _variable_data = data;
}

Dictionary GlobalVariables::get_global_data() const{
  return _variable_data;
}