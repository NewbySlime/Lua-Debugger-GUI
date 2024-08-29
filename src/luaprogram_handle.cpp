#include "error_trigger.h"
#include "logger.h"
#include "luaprogram_handle.h"
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

  ADD_SIGNAL(MethodInfo(SIGNAL_LUA_ON_FILE_LOADED), ProperyInfo(Variant::STRING, "file_path"));
  ADD_SIGNAL(MethodInfo(SIGNAL_LUA_ON_FILE_FOCUS_CHANGED), PropertyInfo(Variant::STRING, "file_path"));
}


LuaProgramHandle::LuaProgramHandle(){
  _runtime_handler = NULL;
  _lua_lib_data = NULL;

#if (_WIN64) || (_WIN32)
  _event_stopped = CreateEvent(NULL, TRUE, FALSE, NULL);
  _event_resumed = CreateEvent(NULL, TRUE, FALSE, NULL);
  _event_paused = CreateEvent(NULL, TRUE, FALSE, NULL);
#endif
}

LuaProgramHandle::~LuaProgramHandle(){
  _unload_runtime_handler();

#if (_WIN64) || (_WIN32)
  CloseHandle(_event_stopped);
  CloseHandle(_event_resumed);
  CloseHandle(_event_paused);
#endif

  _lua_lib_data = NULL;
}


int LuaProgramHandle::_load_runtime_handler(const std::string& file_path){
  int _err_code = LUA_OK;

  _unload_runtime_handler();
  if(!_lua_lib_data){
    GameUtils::Logger::print_err_static("[LuaProgramHandle] Lua Debugging API not yet loaded.");
    goto on_error_label;
  }

  _runtime_handler = _lua_lib_data->get_function_data()->rhcf(NULL, false, true);

  _execution_flow = _runtime_handler->get_execution_flow_interface();
  _execution_flow->register_event_pausing(_event_paused);
  _execution_flow->register_event_resuming(_event_resumed);

  _err_code = _runtime_handler->load_file(file_path.c_str());
  if(_err_code != LUA_OK){
    string_store _str;
    _runtime_handler->get_last_error_object()->to_string(&_str);

    GameUtils::Logger::print_err_static(gd_format_str("[LuaProgramHandle][Lua] Err {0}, {1}", Variant(_err_code), String(_str.data.c_str())));
    goto on_error_label;
  }

  _current_file_path = file_path;

  emit_signal(SIGNAL_LUA_ON_FILE_LOADED, String(file_path.c_str()));
  return LUA_OK;


  on_error_label:{
    _unload_runtime_handler();
  return _err_code;}
}

void LuaProgramHandle::_unload_runtime_handler(){
  if(_lua_lib_data != NULL && _runtime_handler)
    _lua_lib_data->get_function_data()->rhdf(_runtime_handler);

  _runtime_handler = NULL;
  _execution_flow = NULL;
}


void LuaProgramHandle::_run_execution_cb(){
  int _err_code = _runtime_handler->run_current_file();
  if(_err_code != LUA_OK){
    string_store _str;
    _runtime_handler->get_last_error_object()->to_string(&_str);

    GameUtils::Logger::print_err_static(gd_format_str("[LuaProgramHandle][Lua] Err {0}, {1}", Variant(_err_code), String(_str.data.c_str())));
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

  _lua_lib = get_node<LibLuaHandle>("/root/GlobalLuaLibHandle");
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
  if(WaitForSingleObject(_event_stopped, 0)){
    ResetEvent(_event_stopped);
    emit_signal(SIGNAL_LUA_ON_STOPPING);
  }

  if(WaitForSingleObject(_event_resumed, 0)){
    ResetEvent(_event_resumed);
    emit_signal(SIGNAL_LUA_ON_RESUMING);
  }

  if(WaitForSingleObject(_event_paused, 0)){
    ResetEvent(_event_paused);
    emit_signal(SIGNAL_LUA_ON_PAUSING);
  }
#endif
}


int LuaProgramHandle::load_file(const std::string& file_path){
  return _load_runtime_handler(file_path);
}


void LuaProgramHandle::start_lua(){
  if(!_runtime_handler){
    // TODO prompt file select

    int _err_code = _load_runtime_handler(".lua");
    if(_err_code != LUA_OK)
      return;
  }
  else if(_runtime_handler->is_currently_executing())
    return;

  _runtime_handler->run_execution([](void* cbdata){
    ((LuaProgramHandle*)cbdata)->_run_execution_cb();
  }, this);

  emit_signal(SIGNAL_LUA_ON_STARTING);
}

void LuaProgramHandle::stop_lua(){
  if(!_runtime_handler || !_runtime_handler->is_currently_executing())
    return;

  _runtime_handler->stop_execution();

  emit_signal(SIGNAL_LUA_ON_STOPPING);
}

void LuaProgramHandle::restart_lua(){
  if(!_runtime_handler)
    return;

  stop_lua();
  start_lua();

  emit_signal(SIGNAL_LUA_ON_RESTARTING);
}


bool LuaProgramHandle::is_running(){
  return _runtime_handler && _runtime_handler->is_currently_executing();
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


lua::I_runtime_handler* LuaProgramHandle::get_runtime_handler(){
  return _runtime_handler;
}