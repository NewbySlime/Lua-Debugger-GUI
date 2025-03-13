#include "custom_variant.h"
#include "directory_util.h"
#include "defines.h"
#include "error_trigger.h"
#include "gdstring_store.h"
#include "gdvariant_util.h"
#include "logger.h"
#include "signal_ownership.h"
#include "strutil.h"
#include "variable_watcher.h"
#include "variable_storage.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/class_db.hpp"

#include "Lua-CPPAPI/Src/luaapi_value.h"
#include "Lua-CPPAPI/Src/luaapi_variant_util.h"
#include "Lua-CPPAPI/Src/luaapi_stack.h"

#include "algorithm"


using namespace gdutils;
using namespace godot;
using namespace lua;
using namespace lua::api;
using namespace lua::debug;


static const char* placeholder_group_name = "placeholder_node";


void VariableWatcher::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_global_variable_changed", "key", "value"), &VariableWatcher::_on_global_variable_changed);
  ClassDB::bind_method(D_METHOD("_on_option_control_changed", "key", "value"), &VariableWatcher::_on_option_control_changed);

  ClassDB::bind_method(D_METHOD("_lua_on_thread_starting"), &VariableWatcher::_lua_on_thread_starting);
  ClassDB::bind_method(D_METHOD("_lua_on_pausing"), &VariableWatcher::_lua_on_pausing);
  ClassDB::bind_method(D_METHOD("_lua_on_resuming"), &VariableWatcher::_lua_on_resuming);
  ClassDB::bind_method(D_METHOD("_lua_on_stopping"), &VariableWatcher::_lua_on_stopping);

  ClassDB::bind_method(D_METHOD("get_variable_storage_path"), &VariableWatcher::get_variable_storage_path);
  ClassDB::bind_method(D_METHOD("set_variable_storage_path", "path"), &VariableWatcher::set_variable_storage_path);

  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "variable_storage_path"), "set_variable_storage_path", "get_variable_storage_path");
}


VariableWatcher::~VariableWatcher(){
  _clear_item_state(&_local_item_state_tree);
  _clear_item_state(&_global_item_state_tree);
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
  _update_placeholder_state();
}

void VariableWatcher::_lua_on_pausing(){
  _update_variable_tree();
  // local item state will not be cleared as it will retain data about each functions.
  // _clear_item_state(&_local_item_state_tree);
  _clear_item_state(&_global_item_state_tree);
}

void VariableWatcher::_lua_on_resuming(){
  _update_item_state_tree();
  _clear_variable_tree();
}

void VariableWatcher::_lua_on_stopping(){
  _clear_item_state(&_local_item_state_tree);
  _clear_item_state(&_global_item_state_tree);
  _clear_variable_tree();
  _update_placeholder_state();
}


void VariableWatcher::_update_variable_tree(){
  _clear_variable_tree();

  _lua_program_handle->lock_object();
  if(!_lua_program_handle->is_running())
    goto skip_to_return;

{ // enclosure for using gotos
  core _lc = _lua_program_handle->get_runtime_handler()->get_lua_core_copy();

  I_thread_handle_reference* _main_thread_ref = _lua_program_handle->get_main_thread();
  core _main_lc;
    _main_lc.istate = _main_thread_ref->get_interface()->get_lua_state_interface();
    _main_lc.context = _lua_lib_data->get_function_data()->get_cc();

  I_execution_flow* _execution_flow = _lua_program_handle->get_execution_flow();
  
  std::string _file_name = DirectoryUtil::strip_path(_lua_program_handle->get_current_running_file());
  TreeItem* _file_item = _variable_tree->create_item();
  _file_item->set_text(0, _file_name.c_str());

  // update local variables
  _local_item = _create_tree_item(_file_item);
  _variable_tree_item_metadata* _lmetadata = _vartree_map[_local_item->get_instance_id()];
  _lmetadata->already_revealed = true;
  
  string_var _func_name = _lua_program_handle->get_current_function();
  local_table_var _local_data(&_main_lc, 0);
  _set_tree_item_key(_local_item, &_func_name);
  _set_tree_item_value(_local_item, &_local_data);

  auto _state_iter = _local_item_state_tree.branches.find(_func_name);
  _item_state* _local_function_state = _state_iter != _local_item_state_tree.branches.end()? _state_iter->second: NULL;
  
  _update_tree_item(_local_item, _local_function_state);
  
  // update global variables
  _global_item = _create_tree_item(_file_item);
  _variable_tree_item_metadata* _gmetadata = _vartree_map[_global_item->get_instance_id()];
  _gmetadata->already_revealed = true;
  _global_item_state_tree.is_revealed = true;
  
  string_var _global_name = "Global";
  _lc.context->api_value->pushglobaltable(_lc.istate);
  I_variant* _global_table_var = _lc.context->api_varutil->to_variant(_lc.istate, -1);
  _lc.context->api_stack->pop(_lc.istate, 1);
  _set_tree_item_key(_global_item, &_global_name);
  _set_tree_item_value(_global_item, _global_table_var);  
  
  _update_tree_item(_global_item, &_global_item_state_tree);
  
  cpplua_delete_variant(_global_table_var);
} // enclosure closing

  skip_to_return:{}
  _lua_program_handle->unlock_object();
}

bool VariableWatcher::_check_ignored_variable(_variable_tree_item_metadata* metadata, const I_variant* key){
  if(!_lua_program_handle->is_running())
    return false;

  I_variable_watcher* _vw = _lua_program_handle->get_variable_watcher();

  // check ignored variables
  return
    metadata->this_item == _global_item &&
    _ignore_internal_variables &&
    _vw->is_internal_variables(key)
  ;
}


void VariableWatcher::_update_item_state_tree(){
{ // enclosure for using gotos
  auto _local_iter = _vartree_map.find(_local_item->get_instance_id());
  if(_local_iter == _vartree_map.end() || !_local_iter->second->this_key)
    goto skip_local_function_checking;

  _item_state* _local_function_state;
  auto _state_iter = _local_item_state_tree.branches.find(_local_iter->second->this_key);
  if(_state_iter != _local_item_state_tree.branches.end())
    _local_function_state = _state_iter->second;
  else{
    _local_function_state = new _item_state();
      _local_function_state->is_revealed = true;
    _local_item_state_tree.branches[_local_iter->second->this_key] = _local_function_state;
  }

  _clear_item_state(_local_function_state);
  _store_item_state(_local_item, _local_function_state);
} // enclosure closing
  skip_local_function_checking:{}
  
  _clear_item_state(&_global_item_state_tree);
  _store_item_state(_global_item, &_global_item_state_tree);
}


void VariableWatcher::_update_placeholder_state(){
  if(!_ginvoker)
    return;

  _ginvoker->invoke(placeholder_group_name, "set_visible", !_lua_program_handle->is_running());
}


void VariableWatcher::_add_custom_context(_variable_tree_item_metadata* metadata, PopupContextMenu::MenuData& data){
  PopupContextMenu::MenuData::Part _tmp_part;

  bool _cannot_be_stored = false;
  bool _value_can_be_ref = false;
  if(metadata->this_value){
    switch(metadata->this_value->get_type()){
      break;
      case I_table_var::get_static_lua_type():
      case I_function_var::get_static_lua_type():
        _value_can_be_ref = true;

      break; case I_local_table_var::get_static_lua_type():
        _cannot_be_stored = true;
    }
  }
  
  // add menu for adding to storage
  if(!_cannot_be_stored && metadata->this_value){
    // create separator only when there's any item
    if(data.part_list.size() > 0){
        _tmp_part.item_type = PopupContextMenu::MenuData::type_separator;
        _tmp_part.label = "";
        _tmp_part.id = -1;
      data.part_list.insert(data.part_list.end(), _tmp_part);
    }

      _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
      _tmp_part.label = "Add To Storage (copy)";
      _tmp_part.id = context_menu_add_to_storage_copy;
    data.part_list.insert(data.part_list.end(), _tmp_part);

    if(_value_can_be_ref){
        _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
        _tmp_part.label = "Add To Storage (ref)";
        _tmp_part.id = context_menu_add_to_storage_reference;
      data.part_list.insert(data.part_list.end(), _tmp_part);
    }
  }
}

void VariableWatcher::_check_custom_context(int id){
  switch(id){
    break; case context_menu_add_to_storage_copy:{
      auto _iter = _vartree_map.find(_last_selected_id);
      if(_iter == _vartree_map.end())
        break;

      _vstorage->add_to_storage(_iter->second->this_value, _iter->second->this_key);
    }

    break; case context_menu_add_to_storage_reference:{
      auto _iter = _vartree_map.find(_last_selected_id);
      if(_iter == _vartree_map.end())
        break;

      _vstorage->add_to_storage(_iter->second->this_value, _iter->second->this_key, VariableStorage::sf_store_as_reference);
    }
  }
}


void VariableWatcher::_get_reference_query_function(ReferenceQueryMenu::ReferenceQueryFunction* func){
  *func = _vstorage->get_reference_query_function();
}


void VariableWatcher::_bind_object(OptionControl* obj){
  obj->connect(OptionControl::s_value_set, Callable(this, "_on_option_control_changed"));
}


void VariableWatcher::_ready(){
  LuaVariableTree::_ready();

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

  _vstorage = get_node<VariableStorage>(_vstorage_node_path);
  if(!_vstorage){
    GameUtils::Logger::print_err_static("[VariableWatcher] Cannot get Variable Storage.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _ginvoker = find_any_node<GroupInvoker>(this);
  if(!_ginvoker)
    GameUtils::Logger::print_warn_static("[VariableWatcher] Cannot get Group Invoker.");

  LuaVariableTree::_bind_object(_vstorage);

  // just in case
  Variant _opt_control_path = _gvariables->get_global_value(OptionControl::gvar_object_node_path);
  _gvar_changed_option_control_path(_opt_control_path);

  _gvariables->connect(GlobalVariables::s_global_value_set, Callable(this, "_on_global_variable_changed"));

  _lua_lib_data = _lua_lib->get_library_store();

  _lua_program_handle->connect(LuaProgramHandle::s_thread_starting, Callable(this, "_lua_on_thread_starting"));
  _lua_program_handle->connect(LuaProgramHandle::s_pausing, Callable(this, "_lua_on_pausing"));
  _lua_program_handle->connect(LuaProgramHandle::s_resuming, Callable(this, "_lua_on_resuming"));
  _lua_program_handle->connect(LuaProgramHandle::s_stopping, Callable(this, "_lua_on_stopping"));
} // enclosure closing

  return;


  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  }
}


void VariableWatcher::set_ignore_internal_variables(bool flag){
  _ignore_internal_variables = flag;
  _update_variable_tree();
}

bool VariableWatcher::get_ignore_internal_variables() const{
  return _ignore_internal_variables;
}


NodePath VariableWatcher::get_variable_storage_path() const{
  return _vstorage_node_path;
}

void VariableWatcher::set_variable_storage_path(const NodePath& path){
  _vstorage_node_path = path;
}