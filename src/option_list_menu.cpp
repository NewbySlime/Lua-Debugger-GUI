#include "common_event.h"
#include "logger.h"
#include "option_list_menu.h"
#include "option_value_control.h"
#include "strutil.h"

#include "godot_cpp/classes/engine.hpp"


using namespace godot;


const char* OptionListMenu::s_value_set = "value_set";


void OptionListMenu::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_option_changed", "obj", "value"), &OptionListMenu::_on_option_changed);

  ADD_SIGNAL(MethodInfo(SIGNAL_ON_READY, PropertyInfo(Variant::OBJECT, "node")));
  ADD_SIGNAL(MethodInfo(s_value_set, PropertyInfo(Variant::STRING, "key"), PropertyInfo(Variant::NIL, "value")));
}


void OptionListMenu::_on_option_changed(const String& key, const Variant& value){
  emit_signal(s_value_set, key, value);
}


void OptionListMenu::_update_option_nodes(Node* parent){
  for(int i = 0; i < parent->get_child_count(); i++){
    Node* _child_node = parent->get_child(i);
    _update_option_nodes(_child_node);
    
    if(!_child_node->is_class(OptionValueControl::get_class_static()))
      continue;

    _child_node->connect(OptionValueControl::s_value_set, Callable(this, "_on_option_changed"));

    OptionValueControl* _option_control = (OptionValueControl*)_child_node;
    String _option_key = _option_control->get_option_key();
    _option_lists[_option_key] = _option_control;
  }
}


void OptionListMenu::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  _update_option_nodes(this);

  emit_signal(SIGNAL_ON_READY, this);
}


void OptionListMenu::set_value_data(const String& key, const Variant& value){
  auto _iter = _option_lists.find(key);
  if(_iter == _option_lists.end()){
    GameUtils::Logger::print_warn_static(gd_format_str("[OptionListMenu] Cannot set value of option of key '{0}'.", key));
    return;
  }

  _iter->second->set_option_value(value);
}

Variant OptionListMenu::get_value_data(const String& key) const{
  auto _iter = _option_lists.find(key);
  if(_iter == _option_lists.end()){
    GameUtils::Logger::print_warn_static(gd_format_str("[OptionListMenu] Cannot get value of option of key '{0}'.", key));
    return Variant();
  }

  return _iter->second->get_option_value();
}