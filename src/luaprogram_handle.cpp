#include "console_window.h"
#include "error_trigger.h"
#include "logger.h"
#include "luaprogram_handle.h"
#include "node_utils.h"
#include "strutil.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/class_db.hpp"

#include "Lua-CPPAPI/Src/luaapi_debug.h"
#include "Lua-CPPAPI/Src/luaapi_runtime.h"
#include "Lua-CPPAPI/Src/luaapi_thread.h"
#include "Lua-CPPAPI/Src/luaruntime_handler.h"
#include "Lua-CPPAPI/Src/luavariant_arr.h"


using namespace godot;
using namespace lua;
using namespace lua::api;
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
  _obj_mutex_ptr = &_obj_mutex;
  InitializeCriticalSection(_obj_mutex_ptr);

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

  DeleteCriticalSection(_obj_mutex_ptr);
#endif

  _lua_lib_data = NULL;
}


void LuaProgramHandle::_lock_object() const{
#if (_WIN64) || (_WIN32)
  EnterCriticalSection(_obj_mutex_ptr);
#endif
}

void LuaProgramHandle::_unlock_object() const{
#if (_WIN64) || (_WIN32)
  LeaveCriticalSection(_obj_mutex_ptr);
#endif
}


int LuaProgramHandle::_load_runtime_handler(const std::string& file_path){
  int _err_code = LUA_OK;

  ConsoleWindow* _console_window = get_any_node<ConsoleWindow>(get_node<Node>("/root"), true);

  _unload_runtime_handler();

  _lock_object();
  if(!_lua_lib_data){
    std::string _err_msg = "[LuaProgramHandle] Lua Debugging API not yet loaded.";
    GameUtils::Logger::print_err_static(_err_msg.c_str());
    if(_console_window)
      _console_window->append_output_buffer("[ERR]" + _err_msg + "\n");

    goto on_error_label;
  }

{ // enclosure for using gotos
  const LibLuaHandle::function_data* _func_data = _lua_lib_data->get_function_data();
  const compilation_context* _cc = _func_data->get_cc();
  
  _runtime_handler =  _cc->api_runtime->create_runtime_handler(NULL, true);
  _err_code = _runtime_handler->load_file(file_path.c_str());

  const core _lc = _runtime_handler->get_lua_core_copy();
  _print_override = _cc->api_runtime->create_print_override(_lc.istate);

#if (_WIN64) || (_WIN32)
  _print_override->register_event_read(_event_read);
#endif

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

  emit_signal(SIGNAL_LUA_ON_FILE_LOADED, String(file_path.c_str()));
} // enclosure closing

  _unlock_object();
  return LUA_OK;


  on_error_label:{
    _unlock_object();
    _unload_runtime_handler();
  return _err_code;}
}

void LuaProgramHandle::_unload_runtime_handler(){
  if(is_running())
    stop_lua();

  _lock_object();

  if(_lua_lib_data != NULL){
    const LibLuaHandle::function_data* _func_data = _lua_lib_data->get_function_data();
    const compilation_context* _cc = _func_data->get_cc();

    if(_print_override)
      _cc->api_runtime->delete_print_override(_print_override);

    // last, due to the object that instantiate lua_State
    if(_runtime_handler)
      _cc->api_runtime->delete_runtime_handler(_runtime_handler);
  }

  _runtime_handler = NULL;
  _variable_watcher = NULL;
  _print_override = NULL;

#if (_WIN64) || (_WIN32)
  // event reset just in case
  ResetEvent(_event_stopped);
  ResetEvent(_event_paused);
  ResetEvent(_event_resumed);
  
  ResetEvent(_event_read);
#endif

  _unlock_object();
}


void LuaProgramHandle::_init_check(){
  _lock_object();
  _initialized = true;

  _lua_lib_data = _lua_lib->get_library_store();
  if(!_lua_lib_data)
    _initialized = false;

  _unlock_object();
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

  _lock_object();
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

  _unlock_object();
}


int LuaProgramHandle::load_file(const std::string& file_path){
  return _load_runtime_handler(file_path);
}

int LuaProgramHandle::reload_file(){
  return load_file(_current_file_path);
}


void LuaProgramHandle::start_lua(){  
  if(is_running())
    return;

  struct _execution_data{
    LuaProgramHandle* _this;
    const compilation_context* _cc;
#if (_WIN64) || (_WIN32)
    HANDLE finished_initiating_event;
#endif
  };

#if (_WIN64) || (_WIN32)
  HANDLE _wait_event = CreateMutex(NULL, false, NULL);
#endif

  _execution_data _data;
    _data._this = this;
    _data._cc = _lua_lib_data->get_function_data()->get_cc();
#if (_WIN64) || (_WIN32)
    _data.finished_initiating_event = _wait_event;
#endif

  I_thread_control* _tcontrol = _runtime_handler->get_thread_control_interface();
  _tcontrol->run_execution([](void* data){
    _execution_data _d = *(_execution_data*)data;

    // starting
    _d._this->_lock_object();
    I_thread_control* _thread_control = _d._this->_runtime_handler->get_thread_control_interface();
    I_thread_handle* _thread_handle = _d._cc->api_thread->get_thread_handle();

    _d._this->_thread_handle = _thread_control->get_thread_handle(GetCurrentThreadId());
    _d._this->_variable_watcher = _d._cc->api_debug->create_variable_watcher(_thread_handle->get_lua_state_interface());

    I_execution_flow* _execution_flow = _thread_handle->get_execution_flow_interface();
    
    if(_d._this->_blocking_on_start)
      _execution_flow->block_execution();

#if (_WIN64) || (_WIN32)
    _execution_flow->register_event_resuming(_d._this->_event_resumed);
    _execution_flow->register_event_pausing(_d._this->_event_paused);
#endif

    const core _lc = _d._this->_runtime_handler->get_lua_core_copy();
    const I_function_var* _main_function = _d._this->_runtime_handler->get_main_function();

    _d._this->_unlock_object();
#if (_WIN64) || (_WIN32)
    SetEvent(_d.finished_initiating_event);
#endif

    vararr _args, _res;
    int _err_code = _main_function->run_function(&_lc, &_args, &_res);
    if(_err_code != LUA_OK){
      if(_res.get_var_count() > 0){
        const I_variant* _tmp_var = _res.get_var(0);
        string_store _str; _tmp_var->to_string(&_str);

        _d._this->_execution_err_msg = _str.data;
      }
      else
        _d._this->_execution_err_msg = "Error Message Not Found.";
    }

    _d._this->_execution_code = _err_code;

    // stopping
    _d._this->_lock_object();
    _d._cc->api_debug->delete_variable_watcher(_d._this->_variable_watcher);
    _d._this->_variable_watcher = NULL;
    _thread_control->free_thread_handle(_d._this->_thread_handle);
    _d._this->_thread_handle = NULL;

#if (_WIN64) || (_WIN32)
    SetEvent(_d._this->_event_stopped);
#endif
    _d._this->_unlock_object();
  }, &_data);

#if (_WIN64) || (_WIN32)
  WaitForSingleObject(_wait_event, INFINITE);
  CloseHandle(_wait_event);
#endif

  emit_signal(SIGNAL_LUA_ON_STARTING);
}

void LuaProgramHandle::stop_lua(){
  if(!is_running())
    return;

  _thread_handle->get_interface()->stop_running();
  
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


bool LuaProgramHandle::is_running() const{
  // _thread_handle is automatically set to NULL if the thread is stopped
  return _runtime_handler && _thread_handle;
}


void LuaProgramHandle::resume_lua(){
  _lock_object();
  if(!is_running())
    goto skip_to_return;

{ // enclosure for using gotos
  I_execution_flow* _execution_flow = get_execution_flow(); 
  _execution_flow->resume_execution();
} // enclosure closing

  skip_to_return:{}
  _unlock_object();
}

void LuaProgramHandle::pause_lua(){
  _lock_object();
  if(!is_running())
    goto skip_to_return;

{ // enclosure for using gotos
  I_execution_flow* _execution_flow = get_execution_flow();
  _execution_flow->block_execution();
} // enclosure closing

  skip_to_return:{}
  _unlock_object();
}

void LuaProgramHandle::step_lua(lua::debug::I_execution_flow::step_type step){
  _lock_object();
  if(!is_running())
    goto skip_to_return;

{ // enclosure for using gotos
  I_execution_flow* _execution_flow = get_execution_flow();
  _execution_flow->step_execution(step);
} // enclosure closing

  skip_to_return:{}
  _unlock_object();
}


bool LuaProgramHandle::get_blocking_on_start() const{
  return _blocking_on_start;
}

void LuaProgramHandle::set_blocking_on_start(bool blocking){
  _blocking_on_start = blocking;
}


std::string LuaProgramHandle::get_current_running_file() const{
  std::string _file_path = "";

  _lock_object();
  if(!is_running())
    goto skip_to_return;

{ // enclosure for using gotos
  I_execution_flow* _execution_flow = get_execution_flow();
  const char* _file_path_cstr = _execution_flow->get_current_file_path();
  _file_path = _file_path_cstr? _file_path_cstr: "";
} // enclosure closing

  skip_to_return:{}
  _unlock_object();

  return _file_path;
}

std::string LuaProgramHandle::get_current_function() const{
  std::string _fname = "";

  _lock_object();
  if(!is_running())
    goto skip_to_return;

{ // enclosure for using gotos
  I_execution_flow* _execution_flow = get_execution_flow();
  const char* _fname_cstr = _execution_flow->get_function_name();
  _fname_cstr = _fname_cstr? _fname_cstr: "";

  // Since the first layer is always a main function, return a predefined function name
  _fname =  _execution_flow->get_function_layer() > 1? _fname_cstr: "(lua-main)";
} // enclosure closing

  skip_to_return:{}
  _unlock_object();

  return _fname;
}

int LuaProgramHandle::get_current_running_line() const{
  int _current_line = -1;

  _lock_object();
  if(!is_running())
    goto skip_to_return;

{ // enclosure for using gotos
  I_execution_flow* _execution_flow = get_execution_flow();
  _current_line = _execution_flow->get_current_line();
} // enclosure closing

  skip_to_return:{}
  _unlock_object();

  return _current_line;
}


void LuaProgramHandle::lock_object() const{
  _lock_object();
}

void LuaProgramHandle::unlock_object() const{
  _unlock_object();
}


lua::I_runtime_handler* LuaProgramHandle::get_runtime_handler() const{
  return _runtime_handler;
}

lua::global::I_print_override* LuaProgramHandle::get_print_override() const{
  return _print_override;
}


lua::I_thread_handle_reference* LuaProgramHandle::get_main_thread() const{
  return _thread_handle;
}

lua::debug::I_execution_flow* LuaProgramHandle::get_execution_flow() const{
  if(!is_running())
    return NULL;

  return _thread_handle->get_interface()->get_execution_flow_interface();
}

lua::debug::I_variable_watcher* LuaProgramHandle::get_variable_watcher() const{
  return _variable_watcher;
}