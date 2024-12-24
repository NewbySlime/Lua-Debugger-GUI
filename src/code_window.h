#ifndef CODE_WINDOW_HEADER
#define CODE_WINDOW_HEADER

#include "code_context.h"
#include "code_context_menu.h"
#include "console_window.h"
#include "luaprogram_handle.h"

#include "godot_cpp/classes/packed_scene.hpp"
#include "godot_cpp/classes/tab_container.hpp"
#include "godot_cpp/variant/node_path.hpp"


class CodeWindow: public godot::TabContainer{
  GDCLASS(CodeWindow, godot::TabContainer)

  public:
    // Param:
    //  - STRING: file_path
    static const char* s_file_loaded;
    // Param:
    //  - STRING: file_path
    static const char* s_file_closed;
    static const char* s_focus_switched;
    // Param:
    //  - STRING: file_path
    //  - INT: line
    static const char* s_breakpoint_added;
    // Param:
    //  - STRING: file_path
    //  - INT: line
    static const char* s_breakpoint_removed;
    // Param:
    //  - STRING: file_path
    static const char* s_code_opened;
    // Param:
    //  - STRING: file_path
    //  - INT: error_code
    static const char* s_code_cannot_open;

  private:
    struct _path_node{
      public:
        std::string _path_name;

        std::map<std::string, _path_node*> _branches;

        // if NULL, then this is a directory
        CodeContext* _code_node = NULL;

        // if NULL, then it is the top most
        _path_node* _parent = NULL;
    };

    godot::String _code_context_scene_path;
    godot::Ref<godot::PackedScene> _code_context_scene;

    godot::NodePath _context_menu_path;
    CodeContextMenu* _context_menu_node;

    LuaProgramHandle* _program_handle;

    _path_node* _path_code_root;
    std::map<uint64_t, CodeContext*> _context_map;

    std::string _initial_prompt_path;

    bool _initialized = false;
    bool _context_menu_node_init = false;


    void _recursive_delete(_path_node* node);

    void _on_file_loaded(godot::String file_path);
    void _on_breakpoint_added(int line, uint64_t id);
    void _on_breakpoint_removed(int line, uint64_t id);
    void _on_file_cannot_open(godot::String file_path, int error_code);

    void _lua_on_started();
    void _lua_on_paused();
    void _lua_on_stopped();

    void _on_context_menu_button_pressed(int button_type);

    void _update_context_button_visibility();

    // if returns NULL, not found
    _path_node* _get_path_node(const std::string& file_path);

    // This only handle creating node, CodeContext excluded
    _path_node* _create_path_node(const std::string& file_path);
    // This only handle deleting node, CodeContext excluded
    bool _delete_path_node(const std::string& file_path);

    void _on_code_context_menu_ready(CodeContextMenu* obj);
    void _on_code_context_menu_ready_event(godot::Object* obj);

    void _on_thread_initialized();
  
  protected:
    static void _bind_methods();

  public:
    CodeWindow();
    ~CodeWindow();

    void _ready() override;

    void change_focus_code_context(const std::string& file_path);
    
    std::string get_current_focus_code() const;
    std::string get_current_focus_code_path() const;

    void open_code_context();
    // for the result of the function, listen to s_code_opened or s_code_cannot_open
    void open_code_context(const std::string& file_path);

    bool close_current_code_context();
    bool close_code_context(const std::string& file_path);

    void run_current_code_context();

    CodeContext* get_current_code_context();
    CodeContext* get_code_context(const std::string& file_path);


    godot::String get_code_context_scene_path() const;
    void set_code_context_scene_path(godot::String scene_path);

    godot::NodePath get_context_menu_path() const;
    void set_context_menu_path(godot::NodePath path);
};

#endif