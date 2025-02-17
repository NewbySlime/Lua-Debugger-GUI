#include "defines.h"
#include "error_trigger.h"
#include "gd_string_store.h"
#include "logger.h"
#include "luaprogram_handle.h"
#include "node_utils.h"
#include "strutil.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/class_db.hpp"

#include "Lua-CPPAPI/Src/error_util.h"
#include "Lua-CPPAPI/Src/luaapi_debug.h"
#include "Lua-CPPAPI/Src/luaapi_runtime.h"
#include "Lua-CPPAPI/Src/luaapi_thread.h"
#include "Lua-CPPAPI/Src/luaruntime_handler.h"
#include "Lua-CPPAPI/Src/luavariant_arr.h"

#define MAXIMUM_PIPE_READING_LENGTH 512


using namespace error::util;
using namespace godot;
using namespace lua;
using namespace lua::api;
using namespace lua::debug;
using namespace lua::library;


const char* LuaProgramHandle::s_thread_starting = "thread_starting";
const char* LuaProgramHandle::s_starting = "starting";
const char* LuaProgramHandle::s_stopping = "stopping";
const char* LuaProgramHandle::s_pausing = "pausing";
const char* LuaProgramHandle::s_resuming = "resuming";
const char* LuaProgramHandle::s_restarting = "restarting";

const char* LuaProgramHandle::s_file_loaded = "file_loaded";
const char* LuaProgramHandle::s_file_focus_changed = "file_focus_changed";


void LuaProgramHandle::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_stop_warn_timer_timeout"), &LuaProgramHandle::_on_stop_warn_timer_timeout);

  ClassDB::bind_method(D_METHOD("append_input", "data"), &LuaProgramHandle::append_input);

  ADD_SIGNAL(MethodInfo(s_thread_starting));
  ADD_SIGNAL(MethodInfo(s_starting));
  ADD_SIGNAL(MethodInfo(s_stopping));
  ADD_SIGNAL(MethodInfo(s_pausing));
  ADD_SIGNAL(MethodInfo(s_resuming));
  ADD_SIGNAL(MethodInfo(s_restarting));

  ADD_SIGNAL(MethodInfo(s_file_loaded, PropertyInfo(Variant::STRING, "file_path")));
  ADD_SIGNAL(MethodInfo(s_file_focus_changed, PropertyInfo(Variant::STRING, "file_path")));
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

  InitializeCriticalSection(&_output_mutex);
#endif
}

LuaProgramHandle::~LuaProgramHandle(){
  if(is_running())
    _thread_handle->get_interface()->stop_running();

  _unload_runtime_handler();

#if (_WIN64) || (_WIN32)
  CloseHandle(_event_stopped);
  CloseHandle(_event_resumed);
  CloseHandle(_event_paused);

  CloseHandle(_event_read);

  DeleteCriticalSection(&_output_mutex);

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
  bool _success = true;

  _unload_runtime_handler();

  _lock_object();
  if(!_lua_lib_data){
    String _err_msg = "[LuaProgramHandle] Lua Debugging API not yet loaded.";
    GameUtils::Logger::print_err_static(_err_msg);

    goto on_error_label;
  }

{ // enclosure for using gotos
  _current_file_path = file_path;

  const LibLuaHandle::function_data* _func_data = _lua_lib_data->get_function_data();
  const compilation_context* _cc = _func_data->get_cc();
  
  _runtime_handler = _cc->api_runtime->create_runtime_handler(NULL, true);
  _err_code = _runtime_handler->load_file(file_path.c_str());

  const core _lc = _runtime_handler->get_lua_core_copy();
  _print_override = _cc->api_runtime->create_print_override(_lc.istate);

#if (_WIN64) || (_WIN32)
  _print_override->register_event_read(_event_read);

#define _LOAD_RUNTIME_HANDLER_PRINT_WINDOWS_LAST_ERROR(condition) \
  if(condition){ \
    String _err_msg = gd_format_str("[LuaProgramHandle] Cannot create Pipe for IO: %s", get_windows_error_message(GetLastError()).c_str()); \
    GameUtils::Logger::print_err_static(_err_msg); \
     \
    goto on_error_label; \
  }

  _success = CreatePipe(&_output_pipe, &_output_pipe_input, NULL, 0);
  _LOAD_RUNTIME_HANDLER_PRINT_WINDOWS_LAST_ERROR(!_success)

  _success = CreatePipe(&_input_pipe_output, &_input_pipe, NULL, 0);
  _LOAD_RUNTIME_HANDLER_PRINT_WINDOWS_LAST_ERROR(!_success)

  _output_reader_thread = CreateThread(
    NULL,
    0,
    _output_reader_thread_ep,
    this,
    0,
    NULL
  );

  _print_reader_thread = CreateThread(
    NULL,
    0,
    _print_reader_thread_ep,
    this,
    0,
    NULL
  );

  file_handler_api_constructor_data _fconstruct_data;
    _fconstruct_data.lua_core = &_lc;
    _fconstruct_data.use_pipe = true;
    
    _fconstruct_data.is_output = true;
    _fconstruct_data.pipe_handle = _output_pipe_input;
  I_file_handler* _output_fhandle = _func_data->create_file_handler(&_fconstruct_data);
  I_file_handler* _error_fhandle = _func_data->create_file_handler(&_fconstruct_data);

    _fconstruct_data.is_output = false;
    _fconstruct_data.pipe_handle = _input_pipe_output;
  I_file_handler* _input_fhandle = _func_data->create_file_handler(&_fconstruct_data);

  I_io_handler::constructor_param _ioconstruct_param;
    _ioconstruct_param.stdout_file = _output_fhandle;
    _ioconstruct_param.stderr_file = _error_fhandle;
    _ioconstruct_param.stdin_file = _input_fhandle;
  io_handler_api_constructor_data _ioconstruct_data;
    _ioconstruct_data.lua_core = &_lc;
    _ioconstruct_data.param = &_ioconstruct_param;

  I_io_handler* _io_handle = _func_data->create_io_handler(&_ioconstruct_data);
  _runtime_handler->get_library_loader_interface()->load_library("io", _io_handle);
#endif

  if(_err_code != LUA_OK){
    string_store _str;
    _runtime_handler->get_last_error_object()->to_string(&_str);

    String _err_msg = format_str("[LuaProgramHandle][Lua] Err %d, %s", _err_code, _str.data.c_str()).c_str();
    GameUtils::Logger::print_err_static(_err_msg);

    goto on_error_label;
  }

  _current_file_path = file_path;

  emit_signal(s_file_loaded, String(file_path.c_str()));
} // enclosure closing

  _unlock_object();
  return LUA_OK;


  on_error_label:{
    _unlock_object();
    _unload_runtime_handler();
  return _err_code;}
}

void LuaProgramHandle::_unload_runtime_handler(){
  _lock_object();

  if(_lua_lib_data != NULL){
    const LibLuaHandle::function_data* _func_data = _lua_lib_data->get_function_data();
    const compilation_context* _cc = _func_data->get_cc();

    if(_print_override)
      _cc->api_runtime->delete_print_override(_print_override);

    // last, due to the object that instantiate lua_State
    if(_runtime_handler)
      _cc->api_runtime->delete_runtime_handler(_runtime_handler);

#if (_WIN64) || (_WIN32)
    if(_input_pipe){
      CloseHandle(_input_pipe);
      CloseHandle(_input_pipe_output);
    }

    if(_output_pipe){
      _output_pipe = NULL;
      WriteFile(_output_pipe_input, "\0", 1, NULL, NULL);

      CloseHandle(_output_pipe);
      CloseHandle(_output_pipe_input);
      
      WaitForSingleObject(_output_reader_thread, INFINITE);
      CloseHandle(_output_reader_thread);
      _output_reader_thread = NULL;
    }

    if(_print_reader_thread){
      _print_reader_keep_reading = false;
      SetEvent(_event_read);

      WaitForSingleObject(_print_reader_thread, INFINITE);
      CloseHandle(_print_reader_thread);
    }
#endif
  }

  _runtime_handler = NULL;
  _variable_watcher = NULL;
  _print_override = NULL;

#if (_WIN64) || (_WIN32)
  _input_pipe = NULL;
  _input_pipe_output = NULL;
  _output_pipe = NULL;
  _output_pipe_input = NULL;
  _print_reader_thread = NULL;

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


void LuaProgramHandle::_try_stop(on_stop_callback cb){
  if(!is_running())
    return;

  _create_stop_timer();

  resume_lua();
  _thread_handle->get_interface()->signal_stop();

  _on_stopped_cb = cb;
}

void LuaProgramHandle::_create_stop_timer(){
  _stop_warn_timer = get_tree()->create_timer(_stop_warn_time);
  _stop_warn_timer->connect("timeout", Callable(this, "_on_stop_warn_timer_timeout"));
}

void LuaProgramHandle::_stop_stop_timer(){
  if(!_stop_warn_timer.is_valid())
    return;

  _stop_warn_timer->disconnect("timeout", Callable(this, "_on_stop_warn_timer_timeout"));
  _stop_warn_timer.unref();
}


void LuaProgramHandle::_on_stop_warn_timer_timeout(){
  GameUtils::Logger::print_warn_static("Cannot stop runtime, might be suspended (ex. IO request).");
  _create_stop_timer();
}

void LuaProgramHandle::_on_stopped(){
  _stop_stop_timer();

  // reset events
  ResetEvent(_event_paused);
  ResetEvent(_event_resumed);
  ResetEvent(_event_stopped);

  emit_signal(s_stopping);

  _unload_runtime_handler();

  if(_on_stopped_cb){
    std::invoke(_on_stopped_cb, this);
    _on_stopped_cb = NULL;
  }
}


void LuaProgramHandle::_on_stopped_restart(){
  start_lua(_current_file_path);
  emit_signal(s_restarting);
}


DWORD LuaProgramHandle::_output_reader_thread_ep(LPVOID data){
  LuaProgramHandle* _this = (LuaProgramHandle*)data;

  // +1 for null-terminating character
  char _read_buffer[MAXIMUM_PIPE_READING_LENGTH+1];
  while(_this->_output_pipe){
    DWORD _bytes_read = 0; ReadFile(_this->_output_pipe, _read_buffer, MAXIMUM_PIPE_READING_LENGTH, &_bytes_read, NULL);
    _read_buffer[_bytes_read] = '\0';
    
    EnterCriticalSection(&_this->_output_mutex);
    _this->_output_reading_buffer += _read_buffer;
    LeaveCriticalSection(&_this->_output_mutex);
  }

  return 0;
}

DWORD LuaProgramHandle::_print_reader_thread_ep(LPVOID data){
  LuaProgramHandle* _this = (LuaProgramHandle*)data;
  _this->_print_reader_keep_reading = true;

  while(_this->_print_reader_keep_reading){
    WaitForSingleObject(_this->_event_read, INFINITE);
    ResetEvent(_this->_event_read);
    if(!_this->_print_reader_keep_reading)
      break;

    EnterCriticalSection(&_this->_output_mutex);
    gd_string_store _str;
    _this->_print_override->read_all(&_str);

    _this->_output_reading_buffer += _str.data;
    LeaveCriticalSection(&_this->_output_mutex);
  }

  return 0;
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
      String _msg = String("[ERR] ") + _execution_err_msg.c_str() + "\n";

      GameUtils::Logger::print_err_static(_msg);
    }

    // to allow some callback to start a lua code
    _unlock_object();
    _on_stopped();
    _lock_object();
  }

  if(WaitForSingleObject(_event_resumed, 0) == WAIT_OBJECT_0){
    ResetEvent(_event_resumed);
    emit_signal(s_resuming);
  }

  if(WaitForSingleObject(_event_paused, 0) == WAIT_OBJECT_0){
    ResetEvent(_event_paused);
    emit_signal(s_pausing);
  }

  EnterCriticalSection(&_output_mutex);
  if(_output_reading_buffer.length() > 0){
    GameUtils::Logger::print_log_static(_output_reading_buffer);
    _output_reading_buffer = "";
  }
  LeaveCriticalSection(&_output_mutex);
#endif

  _unlock_object();
}


void LuaProgramHandle::start_lua(const std::string& file_path){
  if(is_running())
    return;

  _load_runtime_handler(file_path);
  if(!_runtime_handler)
    return;

  struct _execution_data{
    LuaProgramHandle* _this;
    const compilation_context* _cc;
#if (_WIN64) || (_WIN32)
    HANDLE thread_initiated_event;
    HANDLE thread_initiated_finish_event;
    HANDLE finished_initiating_event;
#endif
  };

#if (_WIN64) || (_WIN32)
  HANDLE _started_allow_event = CreateEvent(NULL, false, false, NULL);
  HANDLE _started_finish_event = CreateEvent(NULL, false, false, NULL);
  HANDLE _wait_event = CreateMutex(NULL, false, NULL);
#endif

  _execution_data _data;
    _data._this = this;
    _data._cc = _lua_lib_data->get_function_data()->get_cc();
#if (_WIN64) || (_WIN32)
    _data.thread_initiated_event = _started_allow_event;
    _data.thread_initiated_finish_event = _started_finish_event;
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

    _d._this->_unlock_object();
#if (_WIN64) || (_WIN32)
    SetEvent(_d.thread_initiated_event);
    WaitForSingleObject(_d.thread_initiated_finish_event, INFINITE);
#endif
    _d._this->_lock_object();
    
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
  WaitForSingleObject(_started_allow_event, INFINITE);
  emit_signal(s_thread_starting);
  SetEvent(_started_finish_event);
#endif

#if (_WIN64) || (_WIN32)
  WaitForSingleObject(_wait_event, INFINITE);
  CloseHandle(_wait_event);
#endif

  emit_signal(s_starting);
}

void LuaProgramHandle::stop_lua(){
  if(!is_running())
    return;

  _try_stop();
}

void LuaProgramHandle::restart_lua(){
  if(!is_running())
    return;

  _try_stop(&LuaProgramHandle::_on_stopped_restart);
}


bool LuaProgramHandle::is_running() const{
  if(!is_loaded())
    return false;

  // _thread_handle is automatically set to NULL if the thread is stopped
  return _thread_handle;
}

bool LuaProgramHandle::is_loaded() const{
  return _runtime_handler;
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


void LuaProgramHandle::append_input(godot::String str){
  if(!_input_pipe)
    return;

  WriteFile(_input_pipe, GDSTR_AS_PRIMITIVE(str), str.length(), NULL, NULL);
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