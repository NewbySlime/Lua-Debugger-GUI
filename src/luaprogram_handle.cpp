#include "console_window.h"
#include "error_trigger.h"
#include "logger.h"
#include "luaprogram_handle.h"
#include "node_utils.h"
#include "strutil.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/class_db.hpp"


using namespace godot;
using namespace lua;
using namespace lua::debug;


void LuaProgramHandle::_bind_methods(){
  ADD_SIGNAL(MethodInfo(SIGNAL_LUA_ON_STARTING));
  ADD_SIGNAL(MethodInfo(SIGNAL_LUA_ON_STOPPING));
  ADD_SIGNAL(MethodInfo(SIGNAL_LUA_ON_PAUSING));
  ADD_SIGNAL(MethodInfo(SIGNAL_LUA_ON_RESUMING));
  ADD_SIGNAL(MethodInfo(SIGNAL_LUA_ON_RESTARTING));

  ADD_SIGNAL(MethodInfo(SIGNAL_LUA_ON_OUTPUT_WRITTEN));

  ADD_SIGNAL(MethodInfo(SIGNAL_LUA_ON_FILE_LOADED, PropertyInfo(Variant::STRING, "file_path")));
  ADD_SIGNAL(MethodInfo(SIGNAL_LUA_ON_FILE_FOCUS_CHANGED, PropertyInfo(Variant::STRING, "file_path")));
}


LuaProgramHandle::LuaProgramHandle(){
  _runtime_handler = NULL;
  _lua_lib_data = NULL;

#if (_WIN64) || (_WIN32)
  _event_stopped = CreateEvent(NULL, TRUE, FALSE, NULL);
  _event_resumed = CreateEvent(NULL, TRUE, FALSE, NULL);
  _event_paused = CreateEvent(NULL, TRUE, FALSE, NULL);

  _event_read = CreateEvent(NULL, TRUE, FALSE, NULL);
#endif
}

LuaProgramHandle::~LuaProgramHandle(){
  _unload_runtime_handler();

#if (_WIN64) || (_WIN32)
  CloseHandle(_event_stopped);
  CloseHandle(_event_resumed);
  CloseHandle(_event_paused);

  CloseHandle(_event_read);
#endif

  _lua_lib_data = NULL;
}


int LuaProgramHandle::_load_runtime_handler(const std::string& file_path){
  int _err_code = LUA_OK;

  ConsoleWindow* _console_window = get_any_node<ConsoleWindow>(get_node<Node>("/root"), true);

  _unload_runtime_handler();
  if(!_lua_lib_data){
    std::string _err_msg = "[LuaProgramHandle] Lua Debugging API not yet loaded.";
    GameUtils::Logger::print_err_static(_err_msg.c_str());
    if(_console_window)
      _console_window->append_output_buffer("[ERR]" + _err_msg + "\n");

    goto on_error_label;
  }

  const LibLuaHandle::function_data* _func_data = _lua_lib_data->get_function_data();
  
  _runtime_handler = _func_data->rhcf(NULL, false, true);
  _variable_watcher = _func_data->vwcf(_runtime_handler->get_lua_state_interface());
  _print_override = _func_data->gpocf(_runtime_handler->get_lua_state_interface());

  _execution_flow = _runtime_handler->get_execution_flow_interface();


#if (_WIN64) || (_WIN32)
  _print_override->register_event_read(_event_read);

  _execution_flow->register_event_pausing(_event_paused);
  _execution_flow->register_event_resuming(_event_resumed);
#endif

  _err_code = _runtime_handler->load_file(file_path.c_str());
  if(_err_code != LUA_OK){
    string_store _str;
    _runtime_handler->get_last_error_object()->to_string(&_str);

    std::string _err_msg = format_str("[LuaProgramHandle][Lua] Err %d, %s", _err_code, _str.data.c_str());
    GameUtils::Logger::print_err_static(_err_msg.c_str());
    if(_console_window)
      _console_window->append_output_buffer("[ERR]" + _err_msg + "\n");

    goto on_error_label;
  }

  _current_file_path = file_path;
  _variable_watcher->update_global_table_ignore();

  emit_signal(SIGNAL_LUA_ON_FILE_LOADED, String(file_path.c_str()));
  return LUA_OK;


  on_error_label:{
    _unload_runtime_handler();
  return _err_code;}
}

void LuaProgramHandle::_unload_runtime_handler(){
  if(_lua_lib_data != NULL){
    const LibLuaHandle::function_data* _func_data = _lua_lib_data->get_function_data();
    
    if(_variable_watcher)
      _func_data->vwdf(_variable_watcher);
    if(_print_override)
      _func_data->gpodf(_print_override);

    // last, due to the object instantiate lua_State
    if(_runtime_handler)
      _func_data->rhdf(_runtime_handler);
  }

  _runtime_handler = NULL;
  _execution_flow = NULL;
  _variable_watcher = NULL;
  _print_override = NULL;

#if (_WIN64) || (_WIN32)
  // event reset just in case
  ResetEvent(_event_stopped);
  ResetEvent(_event_paused);
  ResetEvent(_event_resumed);
  
  ResetEvent(_event_read);
#endif
}


void LuaProgramHandle::_run_execution_cb(){
  _execution_code = LUA_OK;
  _execution_err_msg = "";

  _execution_code = _runtime_handler->run_current_file();
  if(_execution_code != LUA_OK){
    string_store _str;
    _runtime_handler->get_last_error_object()->to_string(&_str);

    _execution_err_msg = format_str("[LuaProgramHandle][Lua] Err %d, %s", _execution_code, _str.data.c_str());
    GameUtils::Logger::print_err_static(_execution_err_msg.c_str());
  }

#if (_WIN64) || (_WIN32)
  SetEvent(_event_stopped);
#endif
}


void LuaProgramHandle::_init_check(){
  _initialized = true;

  _lua_lib_data = _lua_lib->get_library_store();
  if(!_lua_lib_data)
    _initialized = false;
}


void LuaProgramHandle::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

  _lua_lib = get_node<LibLuaHandle>("/root/GlobalLibLuaHandle");
  if(!_lua_lib){
    GameUtils::Logger::print_err_static("[LuaProgramHandle] Cannot get LibLuaHandle for library information.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  return;


  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  return;}
}

void LuaProgramHandle::_process(double delta){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  if(!_initialized)
    _init_check();

#if (_WIN64) || (_WIN32)
  if(WaitForSingleObject(_event_stopped, 0) == WAIT_OBJECT_0){
    if(_execution_code != LUA_OK){
      ConsoleWindow* _console_window = get_any_node<ConsoleWindow>(get_node<Node>("/root"), true);
      _console_window->append_output_buffer("[ERR]" + _execution_err_msg + "\n");
    }

    ResetEvent(_event_stopped);
    emit_signal(SIGNAL_LUA_ON_STOPPING);
  }

  if(WaitForSingleObject(_event_resumed, 0) == WAIT_OBJECT_0){
    ResetEvent(_event_resumed);
    emit_signal(SIGNAL_LUA_ON_RESUMING);
  }

  if(WaitForSingleObject(_event_paused, 0) == WAIT_OBJECT_0){
    ResetEvent(_event_paused);
    emit_signal(SIGNAL_LUA_ON_PAUSING);
  }

  if(WaitForSingleObject(_event_read, 0) == WAIT_OBJECT_0){
    ResetEvent(_event_read);
    emit_signal(SIGNAL_LUA_ON_OUTPUT_WRITTEN);
  }
#endif
}


int LuaProgramHandle::load_file(const std::string& file_path){
  return _load_runtime_handler(file_path);
}

int LuaProgramHandle::reload_file(){
  return load_file(_current_file_path);
}


void LuaProgramHandle::start_lua(){  
  if(!_runtime_handler || _runtime_handler->is_currently_executing())
    return;

  if(_blocking_on_start)
    _execution_flow->block_execution();
  else
    _execution_flow->resume_execution();

  _runtime_handler->run_execution([](void* cbdata){
    ((LuaProgramHandle*)cbdata)->_run_execution_cb();
  }, this);

  emit_signal(SIGNAL_LUA_ON_STARTING);
}

void LuaProgramHandle::stop_lua(){
  if(!_runtime_handler || !_runtime_handler->is_currently_executing())
    return;

  _runtime_handler->stop_execution();
  
  // reset events
  ResetEvent(_event_paused);
  ResetEvent(_event_resumed);
  ResetEvent(_event_stopped);

  emit_signal(SIGNAL_LUA_ON_STOPPING);
}

void LuaProgramHandle::restart_lua(){
  if(!_runtime_handler)
    return;

  stop_lua();

  reload_file();
  start_lua();

  emit_signal(SIGNAL_LUA_ON_RESTARTING);
}


bool LuaProgramHandle::is_running(){
  return _runtime_handler && _runtime_handler->is_currently_executing();
}


void LuaProgramHandle::resume_lua(){
  if(!_runtime_handler || !_runtime_handler->is_currently_executing())
    return;

  _execution_flow->resume_execution();
}

void LuaProgramHandle::pause_lua(){
  if(!_runtime_handler || !_runtime_handler->is_currently_executing())
    return;

  _execution_flow->block_execution();
}

void LuaProgramHandle::step_lua(lua::debug::I_execution_flow::step_type step){
  if(!_runtime_handler || !_runtime_handler->is_currently_executing())
    return;

  _execution_flow->step_execution(step);
}


bool LuaProgramHandle::get_blocking_on_start() const{
  return _blocking_on_start;
}

void LuaProgramHandle::set_blocking_on_start(bool blocking){
  _blocking_on_start = blocking;
}


std::string LuaProgramHandle::get_current_running_file() const{
  const char* _file_path = _execution_flow->get_current_file_path();
  return _file_path? _file_path: "";
}

std::string LuaProgramHandle::get_current_function() const{
  const char* _fname = _execution_flow->get_function_name();
  _fname = _fname? _fname: "";

  // Since the first layer is always a main function, return a predefined function name
  return _execution_flow->get_function_layer() > 1? _fname: "(lua-main)";
}

int LuaProgramHandle::get_current_running_line() const{
  return _execution_flow->get_current_line();
}


lua::I_runtime_handler* LuaProgramHandle::get_runtime_handler(){
  return _runtime_handler;
}

lua::debug::I_variable_watcher* LuaProgramHandle::get_variable_watcher(){
  return _variable_watcher;
}

lua::global::I_print_override* LuaProgramHandle::get_print_override(){
  return _print_override;
}