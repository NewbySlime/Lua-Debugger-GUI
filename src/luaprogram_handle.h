#ifndef LUAPROGRAM_HANDLE_HEADER
#define LUAPROGRAM_HANDLE_HEADER

#include "liblua_handle.h"

#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/scene_tree_timer.hpp"

#include "Lua-CPPAPI/Src/lualibrary_iohandler.h"
#include "Lua-CPPAPI/Src/luathread_control.h"



class LuaProgramHandle: public godot::Node{
  GDCLASS(LuaProgramHandle, godot::Node)

  public:
    static const char* s_thread_starting;
    static const char* s_starting;
    static const char* s_stopping;
    static const char* s_pausing;
    static const char* s_resuming;
    static const char* s_restarting;

    // Param:
    //  - STRING: file_path
    static const char* s_file_loaded;
    // Param:
    //  - STRING: file_path
    static const char* s_file_focus_changed;

  private:
    typedef void(LuaProgramHandle::*on_stop_callback)();

    LibLuaHandle* _lua_lib;
    std::shared_ptr<LibLuaStore> _lua_lib_data;

    lua::I_runtime_handler* _runtime_handler = NULL;
    lua::global::I_print_override* _print_override = NULL;

    // Only valid when running
    lua::I_thread_handle_reference* _thread_handle = NULL;
    // Only valid when running
    lua::debug::I_variable_watcher* _variable_watcher = NULL;

    int _execution_code;
    std::string _execution_err_msg;

#if (_WIN64) || (_WIN32)
    HANDLE _event_stopped;
    HANDLE _event_paused;
    HANDLE _event_resumed;

    HANDLE _event_read;

    bool _print_reader_keep_reading = true;
    HANDLE _print_reader_thread = NULL;

    HANDLE _output_reader_thread = NULL;
    HANDLE _output_pipe = NULL;
    HANDLE _output_pipe_input = NULL;
    CRITICAL_SECTION _output_mutex;

    HANDLE _input_pipe = NULL;
    HANDLE _input_pipe_output = NULL;

    CRITICAL_SECTION* _obj_mutex_ptr;
    CRITICAL_SECTION _obj_mutex;
#endif

    godot::String _output_reading_buffer;

    godot::Ref<godot::SceneTreeTimer> _stop_warn_timer = NULL;
    // TODO expose to editor
    float _stop_warn_time = 3;

    std::string _current_file_path;

    on_stop_callback _on_stopping_cb = NULL;

    bool _blocking_on_start = true;
    bool _initialized = false;

    void _lock_object() const;
    void _unlock_object() const;

    int _load_runtime_handler(const std::string& file_path);
    // will not handle stopping the handler
    void _unload_runtime_handler();

    void _init_check();

    void _try_stop(on_stop_callback cb = NULL);
    void _create_stop_timer();
    void _stop_stop_timer();

    void _on_stop_warn_timer_timeout();
    void _on_stopped();

    void _on_stopped_restart();

#if (_WIN64) || (_WIN32)
    static DWORD _output_reader_thread_ep(LPVOID data);
    static DWORD _print_reader_thread_ep(LPVOID data);
#endif

  protected:
    static void _bind_methods();
  
  public:
    LuaProgramHandle();
    ~LuaProgramHandle();

    void _ready() override;
    void _process(double delta) override;
    
    void start_lua(const std::string& file_path);
    // async function, wait for s_stopping if needed
    void stop_lua();
    void restart_lua();

    bool is_running() const;
    bool is_loaded() const;

    void resume_lua();
    void pause_lua();
    void step_lua(lua::debug::I_execution_flow::step_type step);

    bool get_blocking_on_start() const;
    void set_blocking_on_start(bool blocking);

    // only updated when paused
    std::string get_current_running_file() const;
    // only updated when paused
    std::string get_current_function() const;
    // only updated when paused
    int get_current_running_line() const;

    void append_input(godot::String str);

    // Use this when accessing API objects, as the objects might be freed when the program is stopping. 
    void lock_object() const;
    void unlock_object() const;

    lua::I_runtime_handler* get_runtime_handler() const;
    lua::global::I_print_override* get_print_override() const;

    // Returns NULL if not yet running.
    lua::I_thread_handle_reference* get_main_thread() const;
    // Returns NULL if not yet running.
    lua::debug::I_execution_flow* get_execution_flow() const;
    // Returns NULL if not yet running.
    lua::debug::I_variable_watcher* get_variable_watcher() const;
};

#endif