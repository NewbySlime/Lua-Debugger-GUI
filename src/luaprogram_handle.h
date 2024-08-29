#ifndef LUAPROGRAM_HANDLE_HEADER
#define LUAPROGRAM_HANDLE_HEADER

#include "liblua_handle.h"

#include "godot_cpp/classes/node.hpp"


#define SIGNAL_LUA_ON_STARTING "on_starting"
#define SIGNAL_LUA_ON_STOPPING "on_stopping"
#define SIGNAL_LUA_ON_PAUSING "on_pausing"
#define SIGNAL_LUA_ON_RESUMING "on_resuming"
#define SIGNAL_LUA_ON_RESTARTING "on_restarting"

#define SIGNAL_LUA_ON_FILE_LOADED "file_loaded"
#define SIGNAL_LUA_ON_FILE_FOCUS_CHANGED "file_focus_changed"


class LuaProgramHandle: public godot::Node{
  GDCLASS(LuaProgramHandle, godot::Node)

  private:
    LibLuaHandle* _lua_lib;
    std::shared_ptr<LibLuaStore> _lua_lib_data;

    lua::I_runtime_handler* _runtime_handler = NULL;
    lua::debug::I_execution_flow* _execution_flow = NULL;

#if (_WIN64) || (_WIN32)
    HANDLE _event_stopped;
    HANDLE _event_paused;
    HANDLE _event_resumed;
#endif

    std::string _current_file_path;

    bool _initialized = false;


    int _load_runtime_handler(const std::string& file_path);
    void _unload_runtime_handler();

    void _run_execution_cb();

    void _init_check();

  protected:
    static void _bind_methods();

  
  public:
    LuaProgramHandle();
    ~LuaProgramHandle();

    void _ready() override;
    void _process(double delta) override;

    int load_file(const std::string& file_path);
    
    void start_lua();
    void stop_lua();
    void restart_lua();

    bool is_running();

    void pause_lua();
    void step_lua(lua::debug::I_execution_flow::step_type step);

    // only updated when paused
    std::string get_current_running_file();
    // only updated when paused
    int get_current_running_line();

    lua::I_runtime_handler* get_runtime_handler();
};

#endif