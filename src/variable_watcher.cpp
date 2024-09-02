#include "directory_util.h"
#include "error_trigger.h"
#include "logger.h"
#include "variable_watcher.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/class_db.hpp"


using namespace godot;
using namespace lua;
using namespace lua::debug;


void VariableWatcher::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_lua_on_pausing"), &VariableWatcher::_lua_on_pausing);
  ClassDB::bind_method(D_METHOD("_lua_on_resuming"), &VariableWatcher::_lua_on_resuming);
  ClassDB::bind_method(D_METHOD("_lua_on_stopping"), &VariableWatcher::_lua_on_stopping);

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


void VariableWatcher::_lua_on_pausing(){
  _update_variable_tree();
}

void VariableWatcher::_lua_on_resuming(){
  _clear_variable_tree();
}

void VariableWatcher::_lua_on_stopping(){
  _clear_variable_tree();
}


void VariableWatcher::_update_tree_item(TreeItem* parent_item, I_variable_watcher* watcher){
  for(int i = 0; i < watcher->get_variable_count(); i++){
    auto _iter = _filter_key.find(watcher->get_variable_name(i));
    if(_iter != _filter_key.end())
      continue;

    TreeItem* _var_title = _variable_tree->create_item(parent_item);
    I_variant* _variant = watcher->get_variable(i);

    { // Closing scope not to fill memory as it might recurse
      std::string _format_val_str = format_str("%s: %s", watcher->get_variable_name(i), _lua_lib_data->get_function_data()->gtnf(watcher->get_variable_type(i)));
      _var_title->set_text(0, _format_val_str.c_str());
    }
    
    switch(_variant->get_type()){
      break; case lua::table_var::get_static_lua_type():{
        _update_tree_item(_var_title, dynamic_cast<I_table_var*>(_variant));
      }

      break; case lua::nil_var::get_static_lua_type():
        // do nothing

      break; default:{
        string_store _str; _variant->to_string(&_str);
        TreeItem* _var_data = _variable_tree->create_item(_var_title);
        _var_data->set_text(0, _str.data.c_str());
      }
    }
  }
}

void VariableWatcher::_update_tree_item(TreeItem* parent_item, I_table_var* var){
  const I_variant** _keys_list = var->get_keys();
  
  int _idx = 0;
  while(_keys_list[_idx]){
    const I_variant* _key_data = _keys_list[_idx];
    string_store _key_str; _key_data->to_string(&_key_str);
    auto _iter = _filter_key.find(_key_str.data);
    if(_iter == _filter_key.end()){
      TreeItem* _var_title = _variable_tree->create_item(parent_item);
      I_variant* _value_data = var->get_value(_key_data);

      { // Closing scope not to fill memory as it might recurse
        std::string _format_val_str = format_str("%s: %s", _key_str.data.c_str(), _lua_lib_data->get_function_data()->gtnf(_value_data->get_type()));
        _var_title->set_text(0, _format_val_str.c_str());
      }

      switch(_value_data->get_type()){
        break; case lua::table_var::get_static_lua_type():{
          _update_tree_item(_var_title, dynamic_cast<I_table_var*>(_value_data));
        }

        break; default:{
          string_store _value_str; _value_data->to_string(&_value_str);
          TreeItem* _var_data = _variable_tree->create_item(_var_title);
          _var_data->set_text(0, _value_str.data.c_str());
        }
      }
    }

    _idx++;
  }
}


void VariableWatcher::_update_variable_tree(){
  _clear_variable_tree();

  I_variable_watcher* _variable_watcher = _lua_program_handle->get_variable_watcher();

  I_execution_flow* _execution_flow = _lua_program_handle->get_runtime_handler()->get_execution_flow_interface();
  
  std::string _file_name = DirectoryUtil::strip_path(_lua_program_handle->get_current_running_file());
  TreeItem* _file_item = _variable_tree->create_item();
  _file_item->set_text(0, _file_name.c_str());


  // udpate local variables
  _variable_watcher->fetch_current_function_variables();

  std::string _func_name = _lua_program_handle->get_current_function();
  TreeItem* _func_item = _variable_tree->create_item(_file_item);
  _func_item->set_text(0, _func_name.c_str());

  _update_tree_item(_func_item, _variable_watcher);


  // update global variables
  _variable_watcher->fetch_global_table_data();

  TreeItem* _global_item = _variable_tree->create_item(_file_item);
  _global_item->set_text(0, "Global");

  _update_tree_item(_global_item, _variable_watcher);
}

void VariableWatcher::_clear_variable_tree(){
  if(_variable_tree)
    _variable_tree->clear();
}


void VariableWatcher::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

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

  _variable_tree = get_node<Tree>(_variable_tree_path);
  if(!_variable_tree){
    GameUtils::Logger::print_err_static("[VariableWatcher] Cannot get Tree for Variable Inspector.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _lua_lib_data = _lua_lib->get_library_store();

  _lua_program_handle->connect(SIGNAL_LUA_ON_PAUSING, Callable(this, "_lua_on_pausing"));
  _lua_program_handle->connect(SIGNAL_LUA_ON_RESUMING, Callable(this, "_lua_on_resuming"));
  _lua_program_handle->connect(SIGNAL_LUA_ON_STOPPING, Callable(this, "_lua_on_stopping"));

  _clear_variable_tree();

  return;


  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  return;}
}


NodePath VariableWatcher::get_variable_tree_path() const{
  return _variable_tree_path;
}

void VariableWatcher::set_variable_tree_path(NodePath path){
  _variable_tree_path = path;
}