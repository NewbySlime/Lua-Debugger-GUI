#include "defines.h"
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

#ifdef DEBUG_ENABLED
#define CPPLUA_LIBRARY_PATH ProjectSettings::get_singleton()->globalize_path("res://bin/" CPPLUA_LIBRARY_FNAME)
#else
#define CPPLUA_LIBRARY_PATH OS::get_singleton()->get_executable_path() + "/" + CPPLUA_LIBRARY_FNAME
#endif


using namespace godot;


void LibLuaHandle::_bind_methods(){

}


LibLuaHandle::LibLuaHandle(){
  // don't load library here, load when on _ready()
  _library_handle = NULL;
  _func_data = NULL;
}

LibLuaHandle::~LibLuaHandle(){
  _unload_library();
}


void LibLuaHandle::_load_library(){
  if(_func_data)
    _unload_library();

  int _quit_code = 0;
  std::string _library_path;{
    String __gd_str = String(CPPLUA_LIBRARY_PATH);
    _library_path.append(GDSTR_AS_PRIMITIVE(__gd_str), __gd_str.length());
  }

#if (_WIN64) || (_WIN32)
{ // limiting scope
  _library_handle = LoadLibraryA(_library_path.c_str());
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

    MessageBoxA(
      NULL,
      _err_msg_cstr.c_str(),
      "Debugger Error",
      MB_OK | MB_ICONERROR
    );
    
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

  _func_data = new function_data();
  __CHECK_MODULE_FUNCTION(_func_data->vsdlf, var_set_def_logger_func, CPPLUA_VARIANT_SET_DEFAULT_LOGGER_STR)
  __CHECK_MODULE_FUNCTION(_func_data->dvf, del_var_func, CPPLUA_DELETE_VARIANT_STR)
  __CHECK_MODULE_FUNCTION(_func_data->gpocf, gpo_create_func, CPPLUA_CREATE_GLOBAL_PRINT_OVERRIDE_STR)
  __CHECK_MODULE_FUNCTION(_func_data->gpodf, gpo_delete_func, CPPLUA_DELETE_GLOBAL_PRINT_OVERRIDE_STR)
  __CHECK_MODULE_FUNCTION(_func_data->rhcf, rh_create_func, CPPLUA_CREATE_RUNTIME_HANDLER_STR)
  __CHECK_MODULE_FUNCTION(_func_data->rhdf, rh_delete_func, CPPLUA_DELETE_RUNTIME_HANDLER_STR)

  if(_function_not_found){
    MessageBoxA(
      NULL,
      "Error occurred! Cannot find needed functions for Lua Debugging API, check logs for details.",
      "Debugger Error",
      MB_OK | MB_ICONERROR
    );

    _quit_code = GetLastError();
    goto on_error_label;
  }
}
#endif

  GameUtils::Logger::print_log_static("[LibLuaHandle] Lua Debugging API loaded.");
  return;

  on_error_label:{
    get_tree()->quit(_quit_code);
  return;}
}

void LibLuaHandle::_unload_library(){
  if(!_func_data)
    return;

  FreeLibrary(_library_handle);
  _library_handle = NULL;

  delete _func_data;
  _func_data = NULL;
}


void LibLuaHandle::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  _load_library();
}


const LibLuaHandle::function_data* LibLuaHandle::get_library_function(){
  return _func_data;
}