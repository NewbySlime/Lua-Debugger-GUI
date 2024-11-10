#include "defines.h"
#include "error_trigger.h"
#include "liblua_handle.h"
#include "logger.h"
#include "strutil.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/os.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/classes/project_settings.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/utility_functions.hpp"
#include "godot_cpp/classes/scroll_container.hpp"


#if (_WIN64) || (_WIN32)
#pragma comment(lib, "user32.lib")

#define CPPLUA_LIBRARY_FNAME "CPPAPI.dll"
#endif

#define CPPLUA_LIBRARY_PATH ProjectSettings::get_singleton()->globalize_path("res://bin/" CPPLUA_LIBRARY_FNAME)


using namespace godot;


void LibLuaHandle::_bind_methods(){

}


LibLuaHandle::LibLuaHandle(){
  // don't load library here, load when on _ready()
}

LibLuaHandle::~LibLuaHandle(){
  _unload_library();
}


void LibLuaHandle::_load_library(){
  if(!_lib_store)
    _unload_library();

  int _quit_code = 0;
  std::string _library_path;{
    String __gd_str = String(CPPLUA_LIBRARY_PATH);
    _library_path.append(GDSTR_AS_PRIMITIVE(__gd_str), __gd_str.length());
  }

  function_data* _func_data = new function_data();

#if (_WIN64) || (_WIN32)
  HMODULE _library_handle = NULL;

{ // Start Of Scope Limiter
  _library_handle = LoadLibraryA(_library_path.c_str());

  // Try to load in the same working directory
  if(!_library_handle)
    _library_handle = LoadLibraryA(CPPLUA_LIBRARY_FNAME);

  if(!_library_handle){
    DWORD _error_code = GetLastError();

    LPSTR _buffer_str = NULL;
    DWORD_PTR _args[] = {(DWORD_PTR)_library_path.c_str()};
    DWORD _write_len = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY,
      NULL,
      _error_code,
      0,
      (LPSTR)&_buffer_str,
      0,
      (va_list*)_args
    );

    String _err_msg = gd_format_str("[LibLuaHandle] Error occurred! Code: {0} Message: {1}", Variant((uint32_t)_error_code), String(_buffer_str));
    LocalFree(_buffer_str);

    GameUtils::Logger::print_err_static(_err_msg);
    std::string _err_msg_cstr(GDSTR_AS_PRIMITIVE(_err_msg), _err_msg.length());

    ErrorTrigger::trigger_error_message(_err_msg_cstr.c_str());

    _quit_code = _error_code;
    goto on_error_label;
  }


  bool _function_not_found = false;
  const char* _fname_not_found_format = "[LibLuaHandle] Cannot find function {0} in Lua Debugging API Library.";

#define __CHECK_MODULE_FUNCTION(fd_address, fd_type, fname) \
  fd_address = (fd_type)GetProcAddress(_library_handle, fname); \
  if(!fd_address){ \
    _function_not_found = true; \
    GameUtils::Logger::print_err_static(gd_format_str(_fname_not_found_format, String(fname))); \
  }

  __CHECK_MODULE_FUNCTION(_func_data->get_cc, get_api_compilation_context, CPPLUA_GET_API_COMPILATION_CONTEXT_STR)

  if(_function_not_found){
    ErrorTrigger::trigger_error_message("Error occured! Cannot find needed functions for Lua Debugging API, check logs for details.");

    _quit_code = GetLastError();
    goto on_error_label;
  }

  _lib_store = std::make_shared<LibLuaStore>(_library_handle, _func_data);
} // End Of Scope Limiter
#endif

  delete _func_data;

  GameUtils::Logger::print_log_static("[LibLuaHandle] Lua Debugging API loaded.");
  return;

  on_error_label:{
#if (_WIN64) || (_WIN32)
    if(_library_handle)
      FreeLibrary(_library_handle);
#endif

    delete _func_data;

    get_tree()->quit(_quit_code);
  return;}
}

void LibLuaHandle::_unload_library(){
  _lib_store = NULL;
}


void LibLuaHandle::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  _load_library();
}


std::shared_ptr<LibLuaStore> LibLuaHandle::get_library_store(){
  return _lib_store;
}




#if (_WIN64) || (_WIN32)
LibLuaStore::LibLuaStore(HMODULE library_handle, LibLuaHandle::function_data* fdata){
  _lib_handle = library_handle;
  _function_data = new LibLuaHandle::function_data(*fdata);
}

LibLuaStore::~LibLuaStore(){
  FreeLibrary(_lib_handle);
  delete _function_data;
}


HMODULE LibLuaStore::get_library_handle(){
  return _lib_handle;
}

const LibLuaHandle::function_data* LibLuaStore::get_function_data(){
  return _function_data;
}
#endif