#include "directory_util.h"
#include "error_trigger.h"
#include "gd_string_store.h"
#include "logger.h"
#include "strutil.h"
#include "variable_watcher.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/class_db.hpp"

#include "Lua-CPPAPI/Src/luaapi_value.h"
#include "Lua-CPPAPI/Src/luaapi_variant_util.h"
#include "Lua-CPPAPI/Src/luaapi_stack.h"


using namespace godot;
using namespace lua;
using namespace lua::api;
using namespace lua::debug;
using namespace lua::util;


void VariableWatcher::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_global_variable_changed", "key", "value"), &VariableWatcher::_on_global_variable_changed);
  ClassDB::bind_method(D_METHOD("_on_option_control_changed", "key", "value"), &VariableWatcher::_on_option_control_changed);

  ClassDB::bind_method(D_METHOD("_lua_on_thread_starting"), &VariableWatcher::_lua_on_thread_starting);
  ClassDB::bind_method(D_METHOD("_lua_on_pausing"), &VariableWatcher::_lua_on_pausing);
  ClassDB::bind_method(D_METHOD("_lua_on_resuming"), &VariableWatcher::_lua_on_resuming);
  ClassDB::bind_method(D_METHOD("_lua_on_stopping"), &VariableWatcher::_lua_on_stopping);

  ClassDB::bind_method(D_METHOD("_item_collapsed_safe", "item"), &VariableWatcher::_item_collapsed_safe);
  ClassDB::bind_method(D_METHOD("_item_collapsed", "item"), &VariableWatcher::_item_collapsed);
  ClassDB::bind_method(D_METHOD("_item_selected", "mouse_pos", "mouse_button_index"), &VariableWatcher::_item_selected);
  ClassDB::bind_method(D_METHOD("_item_activated"), &VariableWatcher::_item_activated);

  ClassDB::bind_method(D_METHOD("_on_setter_applied"), &VariableWatcher::_on_setter_applied);

  ClassDB::bind_method(D_METHOD("get_variable_tree_path"), &VariableWatcher::get_variable_tree_path);
  ClassDB::bind_method(D_METHOD("set_variable_tree_path", "path"), &VariableWatcher::set_variable_tree_path);

  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "variable_tree_path"), "set_variable_tree_path", "get_variable_tree_path");
}


VariableWatcher::VariableWatcher(){
  _variable_tree = NULL;
}

VariableWatcher::~VariableWatcher(){
  _clear_variable_tree();
}


void VariableWatcher::_on_global_variable_changed(const String& key, const Variant& value){
  typedef void (VariableWatcher::*key_cb_type)(const Variant& value);
  std::map<String, key_cb_type> _key_cb = {
    {OptionControl::gvar_object_node_path, &VariableWatcher::_gvar_changed_option_control_path}
  };

  auto _iter = _key_cb.find(key);
  if(_iter == _key_cb.end())
    return;
  
  (this->*_iter->second)(value);
}

void VariableWatcher::_gvar_changed_option_control_path(const Variant& value){
  if(value.get_type() != Variant::NODE_PATH)
    return;

  NodePath _opt_control_path = value;
  OptionControl* _opt_control = get_node<OptionControl>(_opt_control_path);
  if(!_opt_control){
    GameUtils::Logger::print_err_static(gd_format_str("[VariableWatcher] Path '{0}' is not a valid OptionControl object.", _opt_control_path));
    return;
  }

  _bind_object(_opt_control);
}


void VariableWatcher::_on_option_control_changed(const String& key, const Variant& value){
  typedef void (VariableWatcher::*key_cb_type)(const Variant& value);
  std::map<String, key_cb_type> _key_cb = {
    {"ignore_internal_data", &VariableWatcher::_option_changed_ignore_internal_data}
  };

  auto _iter = _key_cb.find(key);
  if(_iter == _key_cb.end())
    return;

  (this->*_iter->second)(value);
}

void VariableWatcher::_option_changed_ignore_internal_data(const Variant& value){
  set_ignore_internal_variables(value);
}


void VariableWatcher::_lua_on_thread_starting(){
  _lua_program_handle->get_variable_watcher()->ignore_internal_variables(_ignore_internal_variables);
}

void VariableWatcher::_lua_on_pausing(){
  _update_variable_tree();
}

void VariableWatcher::_lua_on_resuming(){
  _clear_variable_tree();
}

void VariableWatcher::_lua_on_stopping(){
  _clear_variable_tree();
}


void VariableWatcher::_item_collapsed_safe(TreeItem* item){
  _update_callable_list.insert(_update_callable_list.end(), Callable(this, "_item_collapsed").bind(item));
}

void VariableWatcher::_item_collapsed(TreeItem* item){
  if(!_lua_program_handle->is_running() || item->is_collapsed())
    return;

  // TODO add error

  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end() || _iter->second->already_revealed)
    return;

  if(_iter->second->this_value->get_type() != I_table_var::get_static_lua_type())
    return;

  _reveal_tree_item(item, dynamic_cast<I_table_var*>(_iter->second->this_value));
}

void VariableWatcher::_item_selected(const Vector2 mouse_pos, int mouse_idx){
  _variable_setter_do_popup();
}

void VariableWatcher::_item_activated(){
  _variable_setter_do_popup();
}


void VariableWatcher::_on_setter_applied(Node* node){
  if(!_lua_program_handle->is_running())
    return;

  // TODO add error
  TreeItem* _current_item = _variable_tree->get_selected();
  if(!_current_item)
    return;

  auto _iter = _vartree_map.find(_current_item->get_instance_id());
  if(_iter == _vartree_map.end())
    return;

  PopupVariableSetter::SetterMode _mode = _popup_variable_setter->get_mode_type();
  const PopupVariableSetter::VariableData& _output = _popup_variable_setter->get_output_data();

  I_variant* _value = NULL;
  switch(_mode){
    break; case PopupVariableSetter::setter_mode_string:{
      string_var _svar = _output.string_data.c_str();
      _value = cpplua_create_var_copy(&_svar);
    }

    break; case PopupVariableSetter::setter_mode_number:{
      number_var _nvar = _output.number_data;
      _value = cpplua_create_var_copy(&_nvar);
    }

    break; case PopupVariableSetter::setter_mode_bool:{
      bool_var _bvar = _output.bool_data;
      _value = cpplua_create_var_copy(&_bvar);
    }
  }

  if(!_value)
    return;

  bool _success = _iter->second->var_setter->set_value(_value, _iter->second->this_key);
  if(!_success){
    I_variable_watcher* _vwatch = _lua_program_handle->get_variable_watcher();
    const I_error_var* _err_var = _vwatch->get_last_error();
    gd_string_store _err_str = "(null)"; 
    gd_string_store _key_str;
    
    if(_err_var){
      _err_str.data = "";
      _err_var->to_string(&_err_str);
    }

    _iter->second->this_key->to_string(&_key_str);

    GameUtils::Logger::print_err_static(gd_format_str("Error occurred when setting '{0}' variable. Err: {1}", _key_str.data, _err_str.data));
  }

  I_variant* _key = cpplua_create_var_copy(_iter->second->this_key);
  _update_tree_item(_current_item, _key, _value);

  cpplua_delete_variant(_key);
  cpplua_delete_variant(_value);
}


void VariableWatcher::_variable_setter_do_popup(){
  if(!_lua_program_handle->is_running())
    return;

  TreeItem* _item = _variable_tree->get_selected();

  // TODO add error
  auto _iter = _vartree_map.find(_item->get_instance_id());
  if(_iter == _vartree_map.end())
    return;

  _popup_variable_setter->set_popup_data(_iter->second->this_value);
  _popup_variable_setter->bind_apply_callable(Callable(this, "_on_setter_applied"));
  _popup_variable_setter->popup();
}


void VariableWatcher::_update_tree_item(TreeItem* parent_item, I_variable_watcher* watcher, bool as_global){
  _delete_tree_item_child(parent_item);

  IVariableSetter* _vsetter;
  if(as_global)
    _vsetter = new GlobalVariableSetter(watcher);
  else
    _vsetter = new LocalVariableSetter(watcher);

  _setter_list.insert(_setter_list.end(), _vsetter);

  const core _lc = _lua_program_handle->get_runtime_handler()->get_lua_core_copy();
  for(int i = 0; i < watcher->get_variable_count(); i++){
    const I_variant* _key_var = watcher->get_variable_key(i);
    auto _iter = _filter_key.find(_key_var);
    if(_iter != _filter_key.end())
      continue;

    TreeItem* _var_title = _create_tree_item(parent_item);
    _variable_tree_item_metadata* _metadata = _vartree_map[_var_title->get_instance_id()];
    _metadata->var_setter = _vsetter;

    I_variant* _value_var = watcher->get_variable_value_mutable(i);
    _update_tree_item(_var_title, _key_var, _value_var);
  }
}

void VariableWatcher::_update_tree_item(TreeItem* parent_item, const I_variant* key_var, I_variant* var){
  _delete_tree_item_child(parent_item);

  const char* _prefix_string = "";
  const char* _suffix_string = "";

  switch(var->get_type()){
    break; case I_table_var::get_static_lua_type():{
      auto _vt_iter = _vartree_map.find(parent_item->get_instance_id());
      if(_vt_iter == _vartree_map.end() || _vt_iter->second->already_revealed)
        break;

      _variable_tree->create_item(parent_item);
      parent_item->set_collapsed(true);
    }

    break; case lua::string_var::get_static_lua_type():{
      _prefix_string = "\"";
      _suffix_string = "\"";
    }
  }

  string_store _key_str; key_var->to_string(&_key_str);
  string_store _value_str; var->to_string(&_value_str);
  std::string _format_val_str = format_str("%s: %s%s%s",
    _key_str.data.c_str(),
    _prefix_string,
    _value_str.data.c_str(),
    _suffix_string
  );

  parent_item->set_text(0, _format_val_str.c_str());

  auto _iter = _vartree_map.find(parent_item->get_instance_id());
  if(_iter != _vartree_map.end()){
    _clear_variable_metadata(_iter->second);

    _iter->second->this_key = cpplua_create_var_copy(key_var);
    _iter->second->this_value = cpplua_create_var_copy(var);
  }
}

void VariableWatcher::_reveal_tree_item(TreeItem* parent_item, I_table_var* var){
  _delete_tree_item_child(parent_item);
  
  IVariableSetter* _vsetter = new TableVariableSetter(var);
  _setter_list.insert(_setter_list.end(), _vsetter);

  auto _vt_iter = _vartree_map.find(parent_item->get_instance_id());
  if(_vt_iter == _vartree_map.end())
    return;

  auto _pt_iter = _parsed_table_list.find(var->get_table_pointer());
  if(_pt_iter != _parsed_table_list.end())
    return;

  _vt_iter->second->already_revealed = true;

  _parsed_table_list.insert(var->get_table_pointer());

  const core _lc = _lua_program_handle->get_runtime_handler()->get_lua_core_copy();
  const I_variant** _keys_list = var->get_keys();
  
  int _idx = 0;
  while(_keys_list[_idx]){
    const I_variant* _key_data = _keys_list[_idx];
    auto _iter = _filter_key.find(_key_data);
    if(_iter == _filter_key.end()){
      TreeItem* _var_title = _create_tree_item(parent_item);
      _variable_tree_item_metadata* _metadata = _vartree_map[_var_title->get_instance_id()];
      _metadata->var_setter = _vsetter;

      I_variant* _value_data = var->get_value(_key_data);
      _update_tree_item(_var_title, _key_data, _value_data);
      var->free_variant(_value_data);
    }

    _idx++;
  }
}


TreeItem* VariableWatcher::_create_tree_item(TreeItem* parent_item){
  TreeItem* _result = _variable_tree->create_item(parent_item);
  _vartree_map[_result->get_instance_id()] = _create_vartree_metadata();

  return _result;
}

VariableWatcher::_variable_tree_item_metadata* VariableWatcher::_create_vartree_metadata(){
  _variable_tree_item_metadata* _result = new _variable_tree_item_metadata();
  _result->this_key = NULL;
  _result->this_value = NULL;
  _result->var_setter = NULL;

  return _result;
}


void VariableWatcher::_update_variable_tree(){
  _clear_variable_tree();

  _lua_program_handle->lock_object();
  if(!_lua_program_handle->is_running())
    goto skip_to_return;


{ // enclosure for using gotos
  core _lc = _lua_program_handle->get_runtime_handler()->get_lua_core_copy();
  
  _parsed_table_list.clear();

  I_variable_watcher* _variable_watcher = _lua_program_handle->get_variable_watcher();
  I_execution_flow* _execution_flow = _lua_program_handle->get_execution_flow();
  
  std::string _file_name = DirectoryUtil::strip_path(_lua_program_handle->get_current_running_file());
  TreeItem* _file_item = _variable_tree->create_item();
  _file_item->set_text(0, _file_name.c_str());

  // update local variables
  _variable_watcher->update_local_variables();

  std::string _func_name = _lua_program_handle->get_current_function();
  TreeItem* _func_item = _variable_tree->create_item(_file_item);
  _func_item->set_text(0, _func_name.c_str());

  _update_tree_item(_func_item, _variable_watcher, false);

  // update global variables
  _lc.context->api_value->pushglobaltable(_lc.istate);
  I_variant* _global_table_var = _lc.context->api_varutil->to_variant(_lc.istate, -1);
  if(_global_table_var->get_type() == I_table_var::get_static_lua_type()){
    I_table_var* _global_table_tvar = dynamic_cast<I_table_var*>(_global_table_var);
    _parsed_table_list.insert(_global_table_tvar->get_table_pointer());
  }

  _lc.context->api_stack->pop(_lc.istate, 1);
  _lc.context->api_varutil->delete_variant(_global_table_var);

  _variable_watcher->update_global_variables();

  TreeItem* _global_item = _variable_tree->create_item(_file_item);
  _global_item->set_text(0, "Global");

  _update_tree_item(_global_item, _variable_watcher, true);
} // enclosure closing

  skip_to_return:{}
  _lua_program_handle->unlock_object();
}


void VariableWatcher::_clear_variable_tree(){
  if(_variable_tree)
    _variable_tree->clear();

  _clear_variable_metadata_map();
  _clear_vsetter_list();
}

void VariableWatcher::_clear_vsetter_list(){
  for(auto _vsetter: _setter_list)
    delete _vsetter;

  _setter_list.clear();
}

void VariableWatcher::_clear_variable_metadata_map(){
  for(auto _pair: _vartree_map)
    _delete_variable_metadata(_pair.second);

  _vartree_map.clear();
}

void VariableWatcher::_clear_variable_metadata(_variable_tree_item_metadata* metadata){
  if(metadata->this_key){
    cpplua_delete_variant(metadata->this_key);
    metadata->this_key = NULL;
  }

  if(metadata->this_value){
    cpplua_delete_variant(metadata->this_value);
    metadata->this_value = NULL;
  }
}

void VariableWatcher::_delete_variable_metadata(_variable_tree_item_metadata* metadata){
  _clear_variable_metadata(metadata);
  delete metadata;
}

void VariableWatcher::_delete_tree_item_child(TreeItem* item){
  while(item->get_child_count() > 0){
    TreeItem* _current_item = item->get_child(0);
    _delete_tree_item_child(_current_item);
    _delete_tree_item(_current_item);
  }
}

void VariableWatcher::_delete_tree_item(TreeItem* item){
  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter != _vartree_map.end()){
    _delete_variable_metadata(_iter->second);
    _vartree_map.erase(_iter);
  }

  TreeItem* _parent_item = item->get_parent();
  if(_parent_item){
    _parent_item->remove_child(item);
  }
}


void VariableWatcher::_bind_object(OptionControl* obj){
  obj->connect(OptionControl::s_value_set, Callable(this, "_on_option_control_changed"));
}


void VariableWatcher::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

{ // enclosure for using goto
  _lua_lib = get_node<LibLuaHandle>("/root/GlobalLibLuaHandle");
  if(!_lua_lib){
    GameUtils::Logger::print_err_static("[VariableWatcher] Cannot get Library Handle for Lua.");

    _quit_code = ERR_UNAVAILABLE;
    goto on_error_label;
  }

  _lua_program_handle = get_node<LuaProgramHandle>("/root/GlobalLuaProgramHandle");
  if(!_lua_program_handle){
    GameUtils::Logger::print_err_static("[VariableWatcher] Cannot get Program Handle for Lua.");

    _quit_code = ERR_UNAVAILABLE;
    goto on_error_label;
  }

  _gvariables = get_node<GlobalVariables>(GlobalVariables::singleton_path);
  if(!_gvariables){
    GameUtils::Logger::print_err_static("[VariableWatcher] Cannot get GlobalVariables.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  NodePath _popup_var_path = _gvariables->get_global_value(GlobalVariables::key_popup_variable_setter_path);
  _popup_variable_setter = _gvariables->get_node<PopupVariableSetter>(_popup_var_path);
  if(!_popup_variable_setter)
    return;

  _variable_tree = get_node<Tree>(_variable_tree_path);
  if(!_variable_tree){
    GameUtils::Logger::print_err_static("[VariableWatcher] Cannot get Tree for Variable Inspector.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _variable_tree->connect("item_collapsed", Callable(this, "_item_collapsed_safe"));
  _variable_tree->connect("item_mouse_selected", Callable(this, "_item_selected"));
  _variable_tree->connect("item_activated", Callable(this, "_item_activated"));

  // just in case
  Variant _opt_control_path = _gvariables->get_global_value(OptionControl::gvar_object_node_path);
  _gvar_changed_option_control_path(_opt_control_path);

  _gvariables->connect(GlobalVariables::s_global_value_set, Callable(this, "_on_global_variable_changed"));

  _lua_lib_data = _lua_lib->get_library_store();

  _lua_program_handle->connect(LuaProgramHandle::s_thread_starting, Callable(this, "_lua_on_thread_starting"));
  _lua_program_handle->connect(LuaProgramHandle::s_pausing, Callable(this, "_lua_on_pausing"));
  _lua_program_handle->connect(LuaProgramHandle::s_resuming, Callable(this, "_lua_on_resuming"));
  _lua_program_handle->connect(LuaProgramHandle::s_stopping, Callable(this, "_lua_on_stopping"));

  _clear_variable_tree();
} // enclosure closing

  return;


  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  return;}
}

void VariableWatcher::_process(double delta){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  if(_update_callable_list.size() > 0){
    for(Callable cb: _update_callable_list)
      cb.call();

    _update_callable_list.clear();
  }
}


void VariableWatcher::set_ignore_internal_variables(bool flag){
  _ignore_internal_variables = flag;
  if(_lua_program_handle->is_running()){
    I_variable_watcher* _vw = _lua_program_handle->get_variable_watcher();
    _vw->ignore_internal_variables(_ignore_internal_variables);
  }

  _update_variable_tree();
}

bool VariableWatcher::get_ignore_internal_variables() const{
  return _ignore_internal_variables;
}


NodePath VariableWatcher::get_variable_tree_path() const{
  return _variable_tree_path;
}

void VariableWatcher::set_variable_tree_path(NodePath path){
  _variable_tree_path = path;
}