#ifndef LIBLUA_HANDLE_HEADER
#define LIBLUA_HANDLE_HEADER

#include "godot_cpp/classes/node.hpp"

#include "Lua-CPPAPI/Src/luadebug_variable_watcher.h"
#include "Lua-CPPAPI/Src/luaglobal_print_override.h"
#include "Lua-CPPAPI/Src/lualibrary_iohandler.h"
#include "Lua-CPPAPI/Src/luaruntime_handler.h"
#include "Lua-CPPAPI/Src/luavariant.h"

#include "memory"

#if (_WIN64) || (_WIN32)
#include "Windows.h"
#endif


class LibLuaStore;
class LibLuaHandle: public godot::Node{
  GDCLASS(LibLuaHandle, godot::Node)

  public:
    struct function_data{
      public:
        // Get compilation_context of the Library
        get_api_compilation_context get_cc;

        create_library_io_handler_func create_io_handler;
        delete_library_io_handler_func delete_io_handler;

        create_library_file_handler_func create_file_handler;
        delete_library_file_handler_func delete_file_handler;
    };

  private:
    std::shared_ptr<LibLuaStore> _lib_store;

    void _load_library();
    void _unload_library();

  protected:
    static void _bind_methods();

  public:
    LibLuaHandle();
    ~LibLuaHandle();

    void _ready() override;

    std::shared_ptr<LibLuaStore> get_library_store();
};


class LibLuaStore{
  private:
#if (_WIN64) || (_WIN32)
    HMODULE _lib_handle;
#endif

    LibLuaHandle::function_data* _function_data;


  public:
#if (_WIN64) || (_WIN32)
    LibLuaStore(HMODULE library_handle, LibLuaHandle::function_data* fdata);
#endif
  
    ~LibLuaStore();

#if (_WIN64) || (_WIN32)
    HMODULE get_library_handle();
#endif

    const LibLuaHandle::function_data* get_function_data();
};


#endif