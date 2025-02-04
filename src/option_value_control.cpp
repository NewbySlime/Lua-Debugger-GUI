#include "common_event.h"
#include "defines.h"
#include "error_trigger.h"
#include "logger.h"
#include "option_value_control.h"
#include "strutil.h"

#include "godot_cpp/classes/base_button.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/line_edit.hpp"
#include "godot_cpp/classes/option_button.hpp"
#include "godot_cpp/classes/range.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/classes/text_edit.hpp"
#include "godot_cpp/variant/utility_functions.hpp"

#include "map"
#include "string"

using namespace GameUtils;
using namespace godot;


const char* OptionValueControl::s_value_set = "value_set";


enum base_class_code{
  base_class_Nil,
  base_class_Range,
  base_class_OptionButton,
  base_class_Button,
  base_class_LineEdit,
  base_class_TextEdit
};

struct option_parser_data{
  public:
    option_parser_data(const String& class_name, base_class_code class_code){
      base_class = class_name;
      this->class_code = class_code;
    }

    String base_class;
    base_class_code class_code;
};


// returns base_class_Nil if not found
static base_class_code _get_node_base_class(godot::Node* obj){
  option_parser_data _parser_list[] = {
    option_parser_data(Range::get_class_static(), base_class_Range),
    option_parser_data(OptionButton::get_class_static(), base_class_OptionButton),
    option_parser_data(BaseButton::get_class_static(), base_class_Button),
    option_parser_data(LineEdit::get_class_static(), base_class_LineEdit),
    option_parser_data(TextEdit::get_class_static(), base_class_TextEdit)
  };

  size_t _parser_list_len = sizeof(_parser_list)/sizeof(option_parser_data); 

  for(const option_parser_data& data: _parser_list){
    if(obj->is_class(data.base_class))
      return data.class_code;
  }

  return base_class_Nil;
}



void OptionValueControl::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_changed_range", "num"), &OptionValueControl::_on_changed_range);
  ClassDB::bind_method(D_METHOD("_on_changed_option_button", "idx"), &OptionValueControl::_on_changed_option_button);
  ClassDB::bind_method(D_METHOD("_on_changed_button", "toggle"), &OptionValueControl::_on_changed_button);
  ClassDB::bind_method(D_METHOD("_on_changed_line_edit", "new_text"), &OptionValueControl::_on_changed_line_edit);
  ClassDB::bind_method(D_METHOD("_on_changed_text_edit"), &OptionValueControl::_on_changed_text_edit);

  ClassDB::bind_method(D_METHOD("set_option_key", "key"), &OptionValueControl::set_option_key);
  ClassDB::bind_method(D_METHOD("get_option_key"), &OptionValueControl::get_option_key);

  ADD_PROPERTY(PropertyInfo(Variant::STRING, "option_key"), "set_option_key", "get_option_key");

  ADD_SIGNAL(MethodInfo(s_value_set, PropertyInfo(Variant::STRING, "key"), PropertyInfo(Variant::NIL, "value")));
}


void OptionValueControl::_on_changed_range(float num){
  emit_signal(s_value_set, _option_key, num);
}

void OptionValueControl::_on_changed_button(bool toggle){
  emit_signal(s_value_set, _option_key, toggle);
}

void OptionValueControl::_on_changed_option_button(int idx){
  emit_signal(s_value_set, _option_key, _get_option_button_value());
}

void OptionValueControl::_on_changed_line_edit(const String& new_text){
  emit_signal(s_value_set, _option_key, new_text);
}

void OptionValueControl::_on_changed_text_edit(){
  if(!_option_control_node || _option_control_node->get_class() != TextEdit::get_class_static())
    return;

  TextEdit* _opt_node = (TextEdit*)_option_control_node;
  emit_signal(s_value_set, _option_key, _opt_node->get_text());
}


void OptionValueControl::_set_option_button_value(const Variant& value){
  OptionButton* _optnode = (OptionButton*)_option_control_node;
  
  Dictionary _data = value;

  Variant _choices_data_var = _data["choices"];
  Dictionary _choices_data = _choices_data_var;
  
  Array _choices_data_key_list = _choices_data.keys();
  if(_choices_data_key_list.size() > 0)
    _optnode->clear();

  for(int i = 0; i < _choices_data_key_list.size(); i++){
    Variant _key = _choices_data_key_list[i];
    Variant _value = _choices_data[_key];
    _optnode->add_item(_value, _key);
  }

  Variant _picked_data_var = _data["picked"];
  if(_picked_data_var.get_type() != Variant::INT)
    return;

  int _picked_data = _data["picked"];
  for(int i = 0; i < _optnode->get_item_count(); i++){
    if(_optnode->get_item_id(i) == _picked_data){
      _optnode->select(i);
      return;
    }
  }
}

Variant OptionValueControl::_get_option_button_value() const{
  OptionButton* _optnode = (OptionButton*)_option_control_node;
  
  Dictionary _result_data;

  Dictionary _choices_data;
  for(int i = 0; i < _optnode->get_item_count(); i++)
    _choices_data[_optnode->get_item_id(i)] = _optnode->get_item_text(i);

  _result_data["choices"] = _choices_data;
  _result_data["picked"] = _optnode->get_selected_id();

  return _result_data;
}


Node* OptionValueControl::_find_control_node(Node* parent){
  for(int i = 0; i < parent->get_child_count(); i++){
    Node* _child_node = parent->get_child(i);
    base_class_code _class_code = _get_node_base_class(_child_node);
    if(_class_code != base_class_Nil)
      return _child_node;

    Node* _found_node = _find_control_node(_child_node);
    if(_found_node)
      return _found_node;
  }

  return NULL;
}


void OptionValueControl::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

{ // enclosure for using gotos
  _option_control_node = _find_control_node(this);
  if(!_option_control_node){
    GameUtils::Logger::print_err_static("[OptionValueControl] Cannot get Option control Node.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error;
  }

  int _option_control_class_code = _get_node_base_class(_option_control_node);
  switch(_option_control_class_code){
    break; case base_class_Range:{
      _option_control_node->connect("value_changed", Callable(this, "_on_changed_range"));
    }

    break; case base_class_Button:{
      _option_control_node->connect("toggled", Callable(this, "_on_changed_button"));
    }

    break; case base_class_OptionButton:{
      _option_control_node->connect("item_selected", Callable(this, "_on_changed_option_button"));
    }

    break; case base_class_LineEdit:{
      _option_control_node->connect("text_submitted", Callable(this, "_on_changed_line_edit"));
    }

    break; case base_class_TextEdit:{
      _option_control_node->connect("text_changed", Callable(this, "_on_changed_text_edit"));
    }

    break; case base_class_Nil:{
      GameUtils::Logger::print_err_static("[OptionValueControl] Fetched Option control Node is not a supported type.");

      _quit_code = ERR_UNCONFIGURED;
      goto on_error;
    }
  }
} // enclosure closing

  return;


  on_error:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  return;}
}


NodePath OptionValueControl::get_value_control_path(Node* relative_node) const{
  if(!_option_control_node)
    return NodePath();

  if(relative_node)
    return relative_node->get_path_to(_option_control_node);

  return _option_control_node->get_path();
}

Node* OptionValueControl::get_value_control_node() const{
  return _option_control_node;
}


void OptionValueControl::set_option_key(const String& key){
  _option_key = key;
}

String OptionValueControl::get_option_key() const{
  return _option_key;
}


void OptionValueControl::set_option_value(const Variant& value){
  // impossible to fail until try to set, as the initialization already handled in _ready
  int _class_code = _get_node_base_class(_option_control_node);
  switch(_class_code){
    break; case base_class_Range:{
      Range* _optnode = (Range*)_option_control_node;
      _optnode->set_value(value);
    }

    break; case base_class_OptionButton:{
      _set_option_button_value(value);
    }

    break; case base_class_Button:{
      BaseButton* _optnode = (BaseButton*)_option_control_node;
      _optnode->set_pressed(value);
    }

    break; case base_class_LineEdit:{
      LineEdit* _optnode = (LineEdit*)_option_control_node;
      _optnode->set_text(value);
    }

    break; case base_class_TextEdit:{
      TextEdit* _optnode = (TextEdit*)_option_control_node;
      _optnode->set_text(value);
    }
  }
}

Variant OptionValueControl::get_option_value() const{
  Variant _result;

  // impossible to fail until try to get, as the initialization already handled in _ready
  int _class_code = _get_node_base_class(_option_control_node);
  switch(_class_code){
    break; case base_class_Range:{
      Range* _optnode = (Range*)_option_control_node;
      _result = _optnode->get_value();
    }

    break; case base_class_OptionButton:{
      _result = _get_option_button_value();
    }

    break; case base_class_Button:{
      BaseButton* _optnode = (BaseButton*)_option_control_node;
      _result = _optnode->is_pressed();
    }

    break; case base_class_LineEdit:{
      LineEdit* _optnode = (LineEdit*)_option_control_node;
      _result = _optnode->get_text();
    }

    break; case base_class_TextEdit:{
      TextEdit* _optnode = (TextEdit*)_option_control_node;
      _result = _optnode->get_text();
    }
  }

  return _result;
}