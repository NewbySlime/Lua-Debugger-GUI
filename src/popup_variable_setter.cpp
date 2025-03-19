#include "defines.h"
#include "dllutil.h"
#include "error_trigger.h"
#include "gdutils.h"
#include "logger.h"
#include "node_utils.h"
#include "option_value_control.h"
#include "popup_variable_setter.h"
#include "strutil.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/option_button.hpp"
#include "godot_cpp/classes/scene_tree.hpp"

#include "vector"


using namespace dynamic_library::util;
using namespace ErrorTrigger;
using namespace gdutils;
using namespace godot;
using namespace lua;


const char* PopupVariableSetter::s_applied = "applied";
const char* PopupVariableSetter::s_cancelled = "cancelled";
const char* PopupVariableSetter::s_mode_type_changed = "mode_type_changed";

const char* PopupVariableSetter::key_variable_key_data = "variable_key_data";
const char* PopupVariableSetter::key_local_key_data = "local_key_data";
const char* PopupVariableSetter::key_use_reference_key_flag_data = "use_reference_key_flag_data";
const char* PopupVariableSetter::key_value_data = "value_data";

const char* PopupVariableSetter::key_string_data = "string_data";
const char* PopupVariableSetter::key_number_data = "number_data";
const char* PopupVariableSetter::key_boolean_data = "boolean_data";
const char* PopupVariableSetter::key_reference_list = "reference_list_data";

const char* PopupVariableSetter::key_type_enum_button = "__type_enum_button";
const char* PopupVariableSetter::key_accept_button = "__accept_button";
const char* PopupVariableSetter::key_cancel_button = "__cancel_button";

typedef void (PopupVariableSetter::*key_cb_type)(const Variant& value);
std::map<String, key_cb_type>* _key_cb = NULL;
std::map<String, uint32_t>* _setter_mode_enum_lookup = NULL;

static void _code_deinitiate();
static destructor_helper _dh(_code_deinitiate);


void PopupVariableSetter::_code_initiate(){
  if(!_key_cb){
    _key_cb = new std::map<String, key_cb_type>{
      {PopupVariableSetter::key_string_data, &PopupVariableSetter::_on_value_set_string_data},
      {PopupVariableSetter::key_number_data, &PopupVariableSetter::_on_value_set_number_data},
      {PopupVariableSetter::key_boolean_data, &PopupVariableSetter::_on_value_set_bool_data},
      {PopupVariableSetter::key_type_enum_button, &PopupVariableSetter::_on_value_set_type_enum_data},
      {PopupVariableSetter::key_use_reference_key_flag_data, &PopupVariableSetter::_on_value_set_use_reference_key_flag_data}
    };
  }

  if(!_setter_mode_enum_lookup){
    _setter_mode_enum_lookup = new std::map<String, uint32_t>{
      {PopupVariableSetter::key_string_data, PopupVariableSetter::setter_mode_string},
      {PopupVariableSetter::key_number_data, PopupVariableSetter::setter_mode_number},
      {PopupVariableSetter::key_boolean_data, PopupVariableSetter::setter_mode_bool},
      {PopupVariableSetter::key_reference_list, PopupVariableSetter::setter_mode_reference_list}
    };
  }
}

static void _code_deinitiate(){
  if(_key_cb){
    delete _key_cb;
    _key_cb = NULL;
  }

  if(_setter_mode_enum_lookup){
    delete _setter_mode_enum_lookup;
    _setter_mode_enum_lookup = NULL;
  }
}


void PopupVariableSetter::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_value_set", "key", "value"), &PopupVariableSetter::_on_value_set);
  ClassDB::bind_method(D_METHOD("_on_accept_button_pressed"), &PopupVariableSetter::_on_accept_button_pressed);
  ClassDB::bind_method(D_METHOD("_on_cancel_button_pressed"), &PopupVariableSetter::_on_cancel_button_pressed);
  ClassDB::bind_method(D_METHOD("_on_popup"), &PopupVariableSetter::_on_popup);
  ClassDB::bind_method(D_METHOD("_on_popup_hide"), &PopupVariableSetter::_on_popup_hide);

  ClassDB::bind_method(D_METHOD("set_option_list_path", "path"), &PopupVariableSetter::set_option_list_path);
  ClassDB::bind_method(D_METHOD("get_option_list_path"), &PopupVariableSetter::get_option_list_path);

  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "option_list"), "set_option_list_path", "get_option_list_path");

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
  _data_init.bool_data = data;
}

void PopupVariableSetter::_on_value_set_type_enum_data(const Variant& data){
  _type_enum_button_signal = true;

  Dictionary _data_dict = data;
  int _enum = _data_dict["picked"];
  set_mode_type(_enum);

  _type_enum_button_signal = false;
}

void PopupVariableSetter::_on_value_set_use_reference_key_flag_data(const Variant& data){
  _data_init.use_reference_key = data;
  _update_use_reference_key_flag();
}


void PopupVariableSetter::_on_accept_button_pressed(){
  _data_output = _data_init;
  _data_output.choosen_reference_id = _reference_query_menu->get_chosen_reference_id();
  _data_output.setter_mode = _current_mode;

  _applied = true;
  emit_signal(PopupVariableSetter::s_applied);
  
  hide();
}

void PopupVariableSetter::_on_cancel_button_pressed(){
  emit_signal(PopupVariableSetter::s_cancelled);
  
  hide();
}

void PopupVariableSetter::_on_popup(){
  _reference_query_menu->set_page(0);
  _reference_query_menu->choose_reference(0);

  _update_setter_ui();
  _applied = false;
}

void PopupVariableSetter::_on_popup_hide(){
  if(!_applied)
    emit_signal(PopupVariableSetter::s_cancelled);
}


void PopupVariableSetter::_reset_enum_button_config(){
  std::vector<std::pair<uint32_t, String>> _name_list = {
    {setter_mode_string, "String"},
    {setter_mode_number, "Number"},
    {setter_mode_bool, "Boolean"},
    {setter_mode_add_table, "Add New Table"},
    {setter_mode_reference_list, "From Reference"}
  };

  Dictionary _data;
  Dictionary _choice_dict;
  for(auto _pair: _name_list){
    // check custom
    auto _iter = _custom_mode_name.find(_pair.first);
    String _name = _iter != _custom_mode_name.end()? _iter->second: _pair.second;
    
    _choice_dict[_pair.first] = _name;
  }

  _data["choices"] = _choice_dict;
  _option_list->set_value_data(key_type_enum_button, _data);
}


void PopupVariableSetter::_update_setter_ui(){
  _reset_enum_button_config();

  if(_edit_flag & edit_add_key_edit){
    _ginvoker->invoke(key_local_key_data, "set_visible", (bool)(_edit_flag & edit_local_key));
    _ginvoker->invoke(key_variable_key_data, "set_visible", !(bool)(_edit_flag & edit_local_key));

    _data_output.use_reference_key = false;
    _update_use_reference_key_flag();
  }
  else{
    _ginvoker->invoke(key_local_key_data, "set_visible", false);
    _ginvoker->invoke(key_variable_key_data, "set_visible", false);
    _ginvoker->invoke(key_use_reference_key_flag_data, "set_visible", false);
  }

  _ginvoker->invoke(key_value_data, "set_visible", (bool)(_edit_flag & edit_add_value_edit));
  
  if(_edit_flag & edit_clear_on_popup)
    clear_input_data();

  if(_edit_flag & edit_add_value_edit){
    _option_list->set_value_data(key_string_data, _data_init.string_data.c_str());
    _option_list->set_value_data(key_number_data, _data_init.number_data);
    _option_list->set_value_data(key_boolean_data, _data_init.bool_data);
    _option_list->set_value_data(key_use_reference_key_flag_data, _data_init.use_reference_key);
  }

  _data_output = _data_init;

  // set setter mode
  set_mode_type(_current_mode);
}


void PopupVariableSetter::_update_use_reference_key_flag(){
  // Use Reference Key Flag
  bool _use_reference_key_visible = (_edit_flag & edit_add_key_edit) > 0 && (_edit_flag & edit_local_key) <= 0 && _current_mode == setter_mode_reference_list;
  _ginvoker->invoke(key_use_reference_key_flag_data, "set_visible", _use_reference_key_visible);

  // Check variable key property
  bool _variable_key_visible = (_edit_flag & edit_add_key_edit) > 0 && (_edit_flag & edit_local_key) <= 0 && !_data_output.use_reference_key;
  _ginvoker->invoke(key_variable_key_data, "set_visible", _variable_key_visible);
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

  CHECK_OPTION_CONTROL_FETCH(key_accept_button, "Accept Button", _accept_button, Button)
  CHECK_OPTION_CONTROL_FETCH(key_cancel_button, "Cancel Button", _cancel_button, Button)

  _ginvoker = find_any_node<GroupInvoker>(_option_list);
  if(!_ginvoker){
    GameUtils::Logger::print_err_static("[PopupVariableSetter] Cannot get GroupInvoker from OptionListMenu children.");
    _quit_code = ERR_UNCONFIGURED;

    goto on_error_label;
  }

  _reference_query_menu = get_any_node<ReferenceQueryMenu>(this, true);
  if(!_reference_query_menu){
    GameUtils::Logger::print_err_static("[PopupVariableSetter] Cannot get ReferenceQueryMenu in child.");
    _quit_code = ERR_UNCONFIGURED;

    goto on_error_label;
  }

  connect("about_to_popup", Callable(this, "_on_popup"));
  connect("popup_hide", Callable(this, "_on_popup_hide"));

  _option_list->connect(OptionListMenu::s_value_set, Callable(this, "_on_value_set"));
  _accept_button->connect("pressed", Callable(this, "_on_accept_button_pressed"));
  _cancel_button->connect("pressed", Callable(this, "_on_cancel_button_pressed"));

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


void PopupVariableSetter::set_mode_type(uint32_t mode){
  _current_mode = mode;
  
  for(auto _pair: *_setter_mode_enum_lookup)
    _ginvoker->invoke(_pair.first, "set_visible", _pair.second == mode);

  if(!_type_enum_button_signal){
    // set enum button
    Dictionary _data;
      _data["picked"] = mode;

    _option_list->set_value_data(key_type_enum_button, _data);
  }

  _update_use_reference_key_flag();
}

uint32_t PopupVariableSetter::get_mode_type() const{
  return _current_mode;
}


void PopupVariableSetter::update_input_data_ui(){
  _update_setter_ui();
}


PopupVariableSetter::VariableData& PopupVariableSetter::get_input_data(){
  return _data_init;
}

void PopupVariableSetter::clear_input_data(){
  _data_init = VariableData();
}


const PopupVariableSetter::VariableData& PopupVariableSetter::get_output_data() const{
  return _data_output;
}


void PopupVariableSetter::set_popup_data(const I_variant* var){
  _data_init = VariableData();
  if(!var)
    return;
  
  switch(var->get_type()){
    break; case I_number_var::get_static_lua_type():{
      const I_number_var* _nvar = dynamic_cast<const I_number_var*>(var);

      set_mode_type(setter_mode_number);
      _data_init.number_data = _nvar->get_number(); 
    }

    break; case I_string_var::get_static_lua_type():{
      const I_string_var* _svar = dynamic_cast<const I_string_var*>(var);

      set_mode_type(setter_mode_string);
      _data_init.string_data = _svar->get_string();
    }

    break; case I_bool_var::get_static_lua_type():{
      const I_bool_var* _bvar = dynamic_cast<const I_bool_var*>(var);

      set_mode_type(setter_mode_bool);
      _data_init.bool_data = _bvar->get_boolean();
    }

    break; default:{
      GameUtils::Logger::print_warn_static(gd_format_str("[PopupVariableSetter] Type '{0}' is not yet supported.", var->get_type()));
    }
  }
}


void PopupVariableSetter::set_custom_setter_mode_name(uint32_t mode, const String& name){
  _custom_mode_name[mode] = name;
}

void PopupVariableSetter::clear_custom_setter_mode_name(){
  _custom_mode_name.clear();
}


void PopupVariableSetter::set_reference_query_function_data(const ReferenceQueryMenu::ReferenceQueryFunction& func_data){
  _reference_query_menu->set_reference_query_function_data(func_data);
}

ReferenceQueryMenu::ReferenceQueryFunction PopupVariableSetter::get_reference_query_function_data() const{
  return _reference_query_menu->get_reference_query_function_data();
}


void PopupVariableSetter::set_edit_flag(uint32_t flag){
  _edit_flag = flag;
}

uint32_t PopupVariableSetter::get_edit_flag() const{
  return _edit_flag;
}


void PopupVariableSetter::set_local_key_choice(const PackedStringArray& key_list){
  _local_key_lookup.clear();
  Dictionary _choices;
  for(int i = 0; i < key_list.size(); i++){
    String _key_str = key_list[i];

    _local_key_lookup[i] = _key_str;
    _choices[i] = _key_str;
  }

  Dictionary _data;
    _data["picked"] = 0;
    _data["choices"] = _choices;

  _option_list->set_value_data(key_local_key_data, _data);
}

String PopupVariableSetter::get_local_key_applied() const{
  Dictionary _data = _option_list->get_value_data(key_local_key_data);
  int _idx = _data["picked"];

  auto _iter = _local_key_lookup.find(_idx);
  if(_iter == _local_key_lookup.end())
    return "";
  
  return _iter->second;
}


void PopupVariableSetter::set_variable_key(const String& key){
  _option_list->set_value_data(key_variable_key_data, key);
}

String PopupVariableSetter::get_variable_key() const{
  return _option_list->get_value_data(key_variable_key_data);
}


void PopupVariableSetter::set_option_list_path(const NodePath& path){
  _option_list_path = path;
}

NodePath PopupVariableSetter::get_option_list_path() const{
  return _option_list_path;
}