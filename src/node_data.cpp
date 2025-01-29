#include "node_data.h"


using namespace godot;


void NodeData::_bind_methods(){
  ClassDB::bind_method(D_METHOD("get_user_data_dict"), &NodeData::get_user_data_dict);
  ClassDB::bind_method(D_METHOD("set_user_data_dict", "user_data"), &NodeData::set_user_data_dict);

  ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "user_data"), "set_user_data_dict", "get_user_data_dict");
}


Dictionary NodeData::get_user_data_dict() const{
  return _user_data;
}

void NodeData::set_user_data_dict(const Dictionary& user_data){
  _user_data = user_data;
}


Variant NodeData::get_user_data(const Variant& key) const{
  if(!_user_data.has(key))
    return Variant();

  return _user_data[key];
}

void NodeData::set_user_data(const Variant& key, const Variant& value){
  _user_data[key] = value;
}