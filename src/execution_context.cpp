#include "error_trigger.h"
#include "execution_context.h"
#include "logger.h"
#include "strutil.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/variant/color.hpp"
#include "godot_cpp/core/class_db.hpp"

using namespace godot;
using namespace lua::debug;


void ExecutionContext::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_lua_starting"), &ExecutionContext::_on_lua_starting);
  ClassDB::bind_method(D_METHOD("_on_lua_stopping"), &ExecutionContext::_on_lua_stopping);
  ClassDB::bind_method(D_METHOD("_on_lua_pausing"), &ExecutionContext::_on_lua_pausing);
  ClassDB::bind_method(D_METHOD("_on_lua_resuming"), &ExecutionContext::_on_lua_resuming);

  ClassDB::bind_method(D_METHOD("get_execution_info_path"), &ExecutionContext::get_execution_info_path);
  ClassDB::bind_method(D_METHOD("set_execution_info_path", "path"), &ExecutionContext::set_execution_info_path);
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "execution_info_path"), "set_execution_info_path", "get_execution_info_path");
}


ExecutionContext::ExecutionContext(){

}

ExecutionContext::~ExecutionContext(){

}


void ExecutionContext::_on_lua_starting(){
  _toggle_gray(true);
}

void ExecutionContext::_on_lua_stopping(){
  _toggle_gray(false);
  _clear_execution_info();
}

void ExecutionContext::_on_lua_pausing(){
  _toggle_gray(false);
  _update_execution_info();
}

void ExecutionContext::_on_lua_resuming(){
  _toggle_gray(true);
}


void ExecutionContext::_toggle_gray(bool flag){
  set_modulate(flag? Color("#a2a2a2"): Color("#ffffff"));
}


void ExecutionContext::_update_execution_info(){
  const char* _info_format = "Function %s | Line %d";

  I_execution_flow* _execution_flow = _program_handle->get_runtime_handler()->get_execution_flow_interface();

  // only fetch the current function's name when the layer is above 1 (on lua-main)
  std::string _fname = _program_handle->get_current_function();
  int _line_code = _program_handle->get_current_running_line();
  
  _execution_info->set_text(format_str(_info_format, _fname.c_str(), _line_code).c_str());
}

void ExecutionContext::_clear_execution_info(){
  _execution_info->set_text("-");
}


void ExecutionContext::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

  _program_handle = get_node<LuaProgramHandle>("/root/GlobalLuaProgramHandle");
  if(!_program_handle){
    GameUtils::Logger::print_err_static("[ExecutionContext] Cannot get LuaProgramHandle node.");

    _quit_code = ERR_UNAVAILABLE;
    goto on_error_label;
  }

  _execution_info = get_node<Label>(_execution_info_path);
  if(!_execution_info){
    GameUtils::Logger::print_err_static("[ExecutionContext] Cannot get Execution Info label.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _clear_execution_info();

  _program_handle->connect(SIGNAL_LUA_ON_STARTING, Callable(this, "_on_lua_starting"));
  _program_handle->connect(SIGNAL_LUA_ON_STOPPING, Callable(this, "_on_lua_stopping"));
  _program_handle->connect(SIGNAL_LUA_ON_PAUSING, Callable(this, "_on_lua_pausing"));
  _program_handle->connect(SIGNAL_LUA_ON_RESUMING, Callable(this, "_on_lua_resuming"));

  return;


  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  return;}
}


NodePath ExecutionContext::get_execution_info_path() const{
  return _execution_info_path;
}

void ExecutionContext::set_execution_info_path(NodePath path){
  _execution_info_path = path;
}