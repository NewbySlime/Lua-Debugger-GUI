#ifndef LIBLUA_HANDLE_HEADER
#define LIBLUA_HANDLE_HEADER

#include "godot_cpp/classes/node.hpp"

#include "Lua-CPPAPI/Src/lua_variant.h"
#include "Lua-CPPAPI/Src/luaglobal_print_override.h"
#include "Lua-CPPAPI/Src/luaruntime_handler.h"

#if (_WIN64) || (_WIN32)
#include "Windows.h"
#endif


class LibLuaHandle: public godot::Node{
  GDCLASS(LibLuaHandle, godot::Node)

  public:
    struct function_data{
      public:
        // Set logger for Variant in DLL compilation
        var_set_def_logger_func vsdlf;
        // Delete variant that is created by the DLL
        del_var_func dvf;

        // Create print_override
        gpo_create_func gpocf;
        // Delete print_override
        gpo_delete_func gpodf;

        // Create runtime_handler
        rh_create_func rhcf;
        // Delete runtime_handler
        rh_delete_func rhdf;
    };

  private:
#if (_WIN64) || (_WIN32)
    HMODULE _library_handle = NULL;
#endif

    function_data* _func_data = NULL;

    void _load_library();
    void _unload_library();

  protected:
    static void _bind_methods();

  public:
    LibLuaHandle();
    ~LibLuaHandle();

    void _ready() override;

    const function_data* get_library_function();
};

#endif