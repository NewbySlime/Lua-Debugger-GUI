#include "group_invoker.h"
#include "logger.h"
#include "strutil.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"


using namespace godot;


void GroupInvoker::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_node_removed", "node"), &GroupInvoker::_on_node_removed);

  ClassDB::bind_method(D_METHOD("set_group_node_data", "data"), &GroupInvoker::set_group_node_data);
  ClassDB::bind_method(D_METHOD("get_group_node_data"), &GroupInvoker::get_group_node_data);

  ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "group_node_data"), "set_group_node_data", "get_group_node_data");
}


void GroupInvoker::_on_node_removed(Node* node){
  auto _iter = _node_group_lookup.find(node->get_instance_id());
  if(_iter == _node_group_lookup.end())
    return;

  auto _nl_iter = _iter->second->node_list.find(node);
  if(_nl_iter != _iter->second->node_list.end())
    _iter->second->node_list.erase(_nl_iter);

  _node_group_lookup.erase(_iter);
}


void GroupInvoker::_clear_group_data_map(){
  for(auto _pair: _group_data_map)
    delete _pair.second;

  _node_group_lookup.clear();
  _group_data_map.clear();
}


GroupInvoker::~GroupInvoker(){
  _clear_group_data_map();
}


void GroupInvoker::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  SceneTree* _stree = get_tree();
  _stree->connect("node_removed", Callable(this, "_on_node_removed"));

  _clear_group_data_map();

  Array _key_list = _group_node_data.keys();
  for(int i = 0; i < _key_list.size(); i++){
    Variant _key = _key_list[i];
    String _key_str = _key;
    auto _iter = _group_data_map.find(_key_str);
    if(_iter != _group_data_map.end()){
      GameUtils::Logger::print_warn_static(gd_format_str("[GroupInvoker] Group '{0}' already exists, is there a dupe?", _key_str));
    }

    Variant _value = _group_node_data[_key];
    if(_value.get_type() != Variant::ARRAY){
      GameUtils::Logger::print_err_static(gd_format_str("[GroupInvoker] Group '{0}' value is not a type of array.", _key_str));
      continue;
    }
      
    _group_data* _data;
    if(_iter == _group_data_map.end()){
      _data = new _group_data();
      _group_data_map[_key_str] = _data;
    }
    else
      _data = _iter->second;
    
    
    Array _node_list = _value;
    for(int i_nl = 0; i_nl < _node_list.size(); i_nl++){
      NodePath _node_path = _node_list[i_nl];
      Node* _node = get_node<Node>(_node_path);
      if(!_node){
        GameUtils::Logger::print_err_static(gd_format_str("[GroupInvoker] Group '{0}' of idx {1} is a not a valid NodePath.", _key_str, i_nl));
        continue;
      }

      auto _nl_iter = _data->node_list.find(_node);
      if(_nl_iter != _data->node_list.end()){
        GameUtils::Logger::print_err_static(gd_format_str("[GroupInvoker] Group '{0}' has dupe NodePath: {1}", _key_str, _node_path));
        continue;
      }

      _data->node_list.insert(_node);
      _node_group_lookup[_node->get_instance_id()] = _data;
    }
  }
}


void GroupInvoker::invoke(const String& group_key, const String& func_name, const Array& parameter){
  auto _iter = _group_data_map.find(group_key);
  if(_iter == _group_data_map.end()){
    GameUtils::Logger::print_err_static(gd_format_str("[GroupInvoker] Cannot find group named '{0}'.", group_key));
    return;
  }

  for(Node* _node: _iter->second->node_list)
    _node->callv(func_name, parameter);
}


void GroupInvoker::set_group_node_data(const Dictionary& data){
  _group_node_data = data;
}

Dictionary GroupInvoker::get_group_node_data() const{
  return _group_node_data;
}