#include "error_trigger.h"
#include "global_variables.h"
#include "logger.h"
#include "popup_context_menu.h"
#include "popup_variable_setter.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/popup_menu.hpp"
#include "godot_cpp/classes/scene_tree.hpp"

using namespace ErrorTrigger;
using namespace gdutils;
using namespace godot;


const char* GlobalVariables::singleton_path = "/root/GlobalUserVariables";

const char* GlobalVariables::s_global_value_set = "value_set";

const char* GlobalVariables::key_popup_variable_setter_path = "global_popup_variable_setter_path";
const char* GlobalVariables::key_context_menu_path = "global_context_menu_path";


void GlobalVariables::_bind_methods(){
  ClassDB::bind_method(D_METHOD("set_global_data", "data"), &GlobalVariables::set_global_data);
  ClassDB::bind_method(D_METHOD("get_global_data"), &GlobalVariables::get_global_data);

  ADD_SIGNAL(MethodInfo(GlobalVariables::s_global_value_set, PropertyInfo(Variant::STRING, "key"), PropertyInfo(Variant::NIL, "value")));

  ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "global_data"), "set_global_data", "get_global_data");
}


void GlobalVariables::_check_variable_data(bool as_warning){
  bool _failed = false;
  void(*log_func)(const String&) = as_warning? GameUtils::Logger::print_warn_static: GameUtils::Logger::print_err_static; 

{ // enclosure for scoping
  NodePath _test_path = get_global_value(key_popup_variable_setter_path);
  PopupVariableSetter* _test = get_node<PopupVariableSetter>(_test_path);
  if(!_test){
    log_func("[GlobalVariables] Global PopupVariableSetter is not valid object or not yet assigned.");
    _failed = true;
  }
} // enclsoure closing

{ // enclosure for scoping
  NodePath _test_path = get_global_value(key_context_menu_path);
  PopupContextMenu* _test = get_node<PopupContextMenu>(_test_path);
  if(!_test){
    log_func("[GlobalVariables] Global PopupContextMenu is not a valid object or not yet assigned.");
    _failed = true;
  }  
} // enclosure closing

  if(_failed && !as_warning){
    trigger_generic_error_message();
    get_tree()->quit(ERR_UNCONFIGURED);
  }
}


void GlobalVariables::_ready(){
  Engine* _engine = Engine::get_singleton();
  _check_variable_data(_engine->is_editor_hint());

  _is_ready = true;
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

  if(!_is_ready)
    return;

  Engine* _engine = Engine::get_singleton();
  _check_variable_data(true);
}

Dictionary GlobalVariables::get_global_data() const{
  return _variable_data;
}