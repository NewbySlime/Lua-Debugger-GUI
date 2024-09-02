#ifndef CODE_WINDOW_HEADER
#define CODE_WINDOW_HEADER

#include "code_context.h"
#include "code_context_menu.h"
#include "console_window.h"
#include "luaprogram_handle.h"

#include "godot_cpp/classes/packed_scene.hpp"
#include "godot_cpp/classes/tab_container.hpp"
#include "godot_cpp/variant/node_path.hpp"


#define SIGNAL_CODE_WINDOW_FILE_LOADED "file_loaded"
#define SIGNAL_CODE_WINDOW_FILE_CLOSED "file_closed"
#define SIGNAL_CODE_WINDOW_FOCUS_SWITCHED "focus_switched"
#define SIGNAL_CODE_WINDOW_BREAKPOINT_ADDED "breakpoint_added"
#define SIGNAL_CODE_WINDOW_BREAKPOINT_REMOVED "breakpoint_removed"


class CodeWindow: public godot::TabContainer{
  GDCLASS(CodeWindow, godot::TabContainer)

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
    ConsoleWindow* _console_window;

    _path_node* _path_code_root;
    std::map<uint64_t, CodeContext*> _context_map;

    std::string _initial_prompt_path;

    bool _initialized = false;
    bool _context_menu_node_init = false;


    void _recursive_delete(_path_node* node);

    void _on_file_loaded(godot::String file_path);
    void _on_breakpoint_added(int line, uint64_t id);
    void _on_breakpoint_removed(int line, uint64_t id);

    void _lua_on_started();
    void _lua_on_paused();
    void _lua_on_stopped();

    void _on_context_menu_button_pressed(int button_type);

    void _update_context_button_visibility();


    _path_node* _get_path_node(const std::string& file_path);

    // This only handle creating node, CodeContext excluded
    _path_node* _create_path_node(const std::string& file_path);
    // This only handle deleting node, CodeContext excluded
    bool _delete_path_node(const std::string& file_path);
  
  protected:
    static void _bind_methods();

  public:
    CodeWindow();
    ~CodeWindow();

    void _ready() override;
    void _process(double delta) override;

    void change_focus_code_context(const std::string& file_path);
    
    std::string get_current_focus_code() const;
    std::string get_current_focus_code_path() const;

    void open_code_context();
    bool open_code_context(const std::string& file_path);

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