#include "defines.h"
#include "dllutil.h"
#include "error_trigger.h"
#include "logger.h"
#include "option_value_control.h"
#include "popup_variable_setter.h"
#include "strutil.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/option_button.hpp"
#include "godot_cpp/classes/scene_tree.hpp"


using namespace dynamic_library::util;
using namespace ErrorTrigger;
using namespace godot;


const char* PopupVariableSetter::s_applied = "applied";
const char* PopupVariableSetter::s_cancelled = "cancelled";
const char* PopupVariableSetter::s_mode_type_changed = "mode_type_changed";

const char* PopupVariableSetter::key_string_data = "string_data";
const char* PopupVariableSetter::key_number_data = "number_data";
const char* PopupVariableSetter::key_boolean_data = "boolean_data";

const char* PopupVariableSetter::key_enum_button = "__enum_button";
const char* PopupVariableSetter::key_accept_button = "__accept_button";
const char* PopupVariableSetter::key_cancel_button = "__cancel_button";

typedef void (PopupVariableSetter::*key_cb_type)(const Variant& value);
std::map<String, key_cb_type>* _key_cb = NULL;

static void _code_deinitiate();
destructor_helper _dh(_code_deinitiate);


void PopupVariableSetter::_code_initiate(){
  if(!_key_cb){
    _key_cb = new std::map<String, key_cb_type>{
      {PopupVariableSetter::key_string_data, &PopupVariableSetter::_on_value_set_string_data},
      {PopupVariableSetter::key_number_data, &PopupVariableSetter::_on_value_set_number_data},
      {PopupVariableSetter::key_boolean_data, &PopupVariableSetter::_on_value_set_bool_data}
    };
  }
}

static void _code_deinitiate(){
  if(_key_cb){
    delete _key_cb;
    _key_cb = NULL;
  }
}


void PopupVariableSetter::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_value_set", "key", "value"), &PopupVariableSetter::_on_value_set);
  ClassDB::bind_method(D_METHOD("_on_accept_button_pressed"), &PopupVariableSetter::_on_accept_button_pressed);
  ClassDB::bind_method(D_METHOD("_on_cancel_button_pressed"), &PopupVariableSetter::_on_cancel_button_pressed);
  ClassDB::bind_method(D_METHOD("_on_enum_button_selected", "idx"), &PopupVariableSetter::_on_enum_button_selected);

  ClassDB::bind_method(D_METHOD("set_option_list_path", "path"), &PopupVariableSetter::set_option_list_path);
  ClassDB::bind_method(D_METHOD("get_option_list_path"), &PopupVariableSetter::get_option_list_path);

  ClassDB::bind_method(D_METHOD("set_mode_node_list", "node_list"), &PopupVariableSetter::set_mode_node_list);
  ClassDB::bind_method(D_METHOD("get_mode_node_list"), &PopupVariableSetter::get_mode_node_list);

  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "option_list"), "set_option_list_path", "get_option_list_path");
  ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "mode_node_list"), "set_mode_node_list", "get_mode_node_list");

  ADD_SIGNAL(MethodInfo(PopupVariableSetter::s_applied));
  ADD_SIGNAL(MethodInfo(PopupVariableSetter::s_cancelled));
  ADD_SIGNAL(MethodInfo(PopupVariableSetter::s_mode_type_changed, PropertyInfo(Variant::INT, "mode")));
}


void PopupVariableSetter::_on_value_set(const String& key, const Variant& value){
  auto _iter = _key_cb->find(key);
  if(_iter == _key_cb->end())
    return;

  (this->*_iter->second)(value);
}

void PopupVariableSetter::_on_value_set_string_data(const Variant& data){
  String _str_data = data;
  _data_init.string_data = GDSTR_TO_STDSTR(_str_data);
}

void PopupVariableSetter::_on_value_set_number_data(const Variant& data){
  _data_init.number_data = data;
}

void PopupVariableSetter::_on_value_set_bool_data(const Variant& data){
  _data_init.number_data = data;
}


void PopupVariableSetter::_on_accept_button_pressed(){
  _data_output = _data_init;
  _data_output.setter_mode = _current_mode;
  
  emit_signal(PopupVariableSetter::s_applied);
}

void PopupVariableSetter::_on_cancel_button_pressed(){
  emit_signal(PopupVariableSetter::s_cancelled);
}


void PopupVariableSetter::_on_enum_button_selected(int idx){
  if(!_enable_enum_button_signal)
    return;

  set_mode_type((SetterMode)_enum_button->get_selected_id());
}


void PopupVariableSetter::_try_parse_mode_node_list(){
  _clear_mode_node_map();
  
  Array _key_list = _mode_node_list.keys();
  for(int i = 0; i < _key_list.size(); i++){
    Variant _key = _key_list[i];
    Variant _value = _mode_node_list[_key];

    if(_key.get_type() != Variant::INT){
      GameUtils::Logger::print_err_static(gd_format_str("[PopupVariableSetter] ModeNodeList Key {0} is not an Int.", _key));
      continue;
    }

    if(_value.get_type() != Variant::ARRAY){
      GameUtils::Logger::print_err_static(gd_format_str("[PopupVariableSetter] ModeNodeList Value (key: {0}) is not an Array.", _key));
      continue;
    }

    // check if _mode_node_map already has the data of the object
    int _key_int = _key;
    _mode_node_data* _data;
    auto _mode_node_map_iter = _mode_node_map.find(_key);
    if(_mode_node_map_iter == _mode_node_map.end()){
      _data = new _mode_node_data();
      _mode_node_map[_key_int] = _data;
    }
    else
      _data = _mode_node_map_iter->second;

    Array _value_arr = _value;
    for(int i_value = 0; i_value < _value_arr.size(); i_value++){
      Variant _key_var = _value_arr[i_value];
      if(_key_var.get_type() != Variant::STRING){
        GameUtils::Logger::print_err_static(gd_format_str("[PopupVariableSetter] ModeNodeList Array (key: {0}, idx: {1}) is not a String.", _key, i_value));
        continue;
      }

      String _key = _key_var;
      Node* _target_node = _option_list->get_value_control_node(_key);
      if(!_target_node){
        GameUtils::Logger::print_err_static(gd_format_str("[PopupVariableSetter] ModeNodeList Array (key: {0}, idx: {1}) is not a valid key.", _key, i_value));
        continue;
      }

      uint64_t _node_id = _target_node->get_instance_id();
      auto node_data_map_iter = _data->node_data_map.find(_node_id);
      if(node_data_map_iter != _data->node_data_map.end())
        continue;

      _mode_node_data::_node_data* _node_data = new _mode_node_data::_node_data();
        _node_data->set_visible_funcs = Callable(_target_node, "set_visible");
      
      _data->node_data_map[_node_id] = _node_data;
    }
  }
}


void PopupVariableSetter::_reset_enum_button_config(){
  _enum_button->add_item("String", setter_mode_string);
  _enum_button->add_item("Number", setter_mode_number);
  _enum_button->add_item("Boolean", setter_mode_bool);
}


void PopupVariableSetter::_clear_mode_node_map(){
  for(auto _pair: _mode_node_map){
    for(auto _node_pair: _pair.second->node_data_map)
      delete _node_pair.second;

    delete _pair.second;
  }

  _mode_node_map.clear();
}


PopupVariableSetter::~PopupVariableSetter(){
  _clear_mode_node_map();
}


void PopupVariableSetter::_ready(){
  _code_initiate();

  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

{ // enclosure for using goto
  _option_list = get_node<OptionListMenu>(_option_list_path);
  if(!_option_list){
    GameUtils::Logger::print_err_static("[PopupVariableSetter] Cannot get OptionListMenu.");
    _quit_code = ERR_UNCONFIGURED;

    goto on_error_label;
  }

#define CHECK_OPTION_CONTROL_FETCH(key_name, log_name, target_var, target_gd_type) \
{ \
  OptionValueControl* _opc = _option_list->get_value_control_node(key_name); \
  if(!_opc){ \
    GameUtils::Logger::print_err_static("[PopupVariableSetter] Cannot get " log_name " for Setter Mode."); \
    _quit_code = ERR_UNCONFIGURED; \
     \
    goto on_error_label; \
  } \
   \
  Node* _tmp_node = _opc->get_value_control_node(); \
  if(!_tmp_node->is_class(target_gd_type::get_class_static())){ \
    GameUtils::Logger::print_err_static("[PopupVariableSetter] " log_name " is not a type of OptionButton."); \
    _quit_code = ERR_UNCONFIGURED; \
     \
    goto on_error_label; \
  } \
   \
  target_var = (target_gd_type*)_tmp_node; \
}

  CHECK_OPTION_CONTROL_FETCH(key_enum_button, "Enum Button", _enum_button, OptionButton)
  CHECK_OPTION_CONTROL_FETCH(key_accept_button, "Accept Button", _accept_button, Button)
  CHECK_OPTION_CONTROL_FETCH(key_cancel_button, "Cancel Button", _cancel_button, Button)

  _reset_enum_button_config();

  _try_parse_mode_node_list();

  _option_list->connect(OptionListMenu::s_value_set, Callable(this, "_on_value_set"));
  _accept_button->connect("pressed", Callable(this, "_on_accept_button_pressed"));
  _cancel_button->connect("pressed", Callable(this, "_on_cancel_button_pressed"));
  _enum_button->connect("item_selected", Callable(this, "_on_enum_button_selected"));

  set_mode_type(setter_mode_string);

  // let it resize to child's minimum size
  set_size(Vector2(0,0));

} // enclosure closing

  return;

  on_error_label:{
    trigger_generic_error_message();
  
    get_tree()->quit(_quit_code);
  }
}


void PopupVariableSetter::set_mode_type(SetterMode mode){
  _current_mode = mode;

  auto _iter = _mode_node_map.find(mode);

  // iterate excluded mode
  for(auto _map_iter = _mode_node_map.begin(); _map_iter != _mode_node_map.end(); _map_iter++){
    if(_map_iter == _iter)
      continue;

    for(auto _node_pair: _map_iter->second->node_data_map){
      _node_pair.second->set_visible_funcs.call(false);
    }
  }

  // set enum button
  for(int i = 0; i < _enum_button->get_item_count(); i++){
    if(mode != _enum_button->get_item_id(i))
      continue;

    _enable_enum_button_signal = false;
    _enum_button->select(i);
    _enable_enum_button_signal = true;
    break;
  }

  // immediately return if nodes not found
  if(_iter == _mode_node_map.end())
    return;

  // iterate included mode
  for(auto _node_pair: _iter->second->node_data_map){
    _node_pair.second->set_visible_funcs.call(true);
  }
}

PopupVariableSetter::SetterMode PopupVariableSetter::get_mode_type() const{
  return _current_mode;
}


void PopupVariableSetter::update_input_data_ui(){
  _option_list->set_value_data(key_string_data, _data_init.string_data.c_str());
  _option_list->set_value_data(key_number_data, _data_init.number_data);
  _option_list->set_value_data(key_boolean_data, _data_init.bool_data);
}


PopupVariableSetter::VariableData& PopupVariableSetter::get_input_data(){
  return _data_init;
}

const PopupVariableSetter::VariableData& PopupVariableSetter::get_output_data() const{
  return _data_output;
}


void PopupVariableSetter::set_option_list_path(const NodePath& path){
  _option_list_path = path;
}

NodePath PopupVariableSetter::get_option_list_path() const{
  return _option_list_path;
}


void PopupVariableSetter::set_mode_node_list(const Dictionary& node_list){
  _mode_node_list = node_list;
}

Dictionary PopupVariableSetter::get_mode_node_list() const{
  return _mode_node_list;
}