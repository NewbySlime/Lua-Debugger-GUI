#ifndef CODE_CONTEXT_HEADER
#define CODE_CONTEXT_HEADER

#include "liblua_handle.h"

#include "godot_cpp/classes/code_edit.hpp"
#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/scroll_container.hpp"


class CodeContext: public godot::Control{
  GDCLASS(CodeContext, godot::Control)

  public:
    // Param:
    //  - STRING: file_path
    static const char* s_file_loaded;
    // Param:
    //  - STRING: file_path
    //  - INT: error_code
    static const char* s_cannot_load;
    // Param:
    //  - INT: idx
    //  - INT: obj_id
    static const char* s_breakpoint_added;
    // Param:
    //  - INT: idx
    //  - INT: obj_id
    static const char* s_breakpoint_removed;

  private:
    godot::NodePath _code_edit_path;
    godot::CodeEdit* _code_edit;

    std::string _current_file_path;

    bool _initialized = false;
    bool _skip_breakpoint_toggled_event = false;

    std::vector<int> _breakpointed_list;

    std::vector<long> _valid_lines;

    std::shared_ptr<LibLuaStore> _lib_store;


    void _breakpoint_toggled_cb(int line);

    // if return -1, no valid line that can be used as a backup
    long _check_valid_line(long check_line);

    void _load_file_check(bool retain_breakpoints = false);

  protected:
    static void _bind_methods();

  public:
    CodeContext();
    ~CodeContext();

    void _ready() override;

    // listen to s_cannot_load when an error happens
    void load_file(const std::string& file_path);
    // listen to s_cannot_load when an error happens
    void reload_file();
    std::string get_current_file_path() const;

    godot::String get_line_at(int line) const;
    int get_line_count() const;

    void focus_at_line(int line);

    void set_breakpoint_line(int line, bool flag);
    void clear_breakpoints();

    // get breakpoint based on index
    int get_breakpoint_line(int idx);
    long get_breakpoint_counts();
    const std::vector<int>* get_breakpoint_list();

    void set_executing_line(int line, bool flag);
    void clear_executing_lines();

    void set_code_edit_path(godot::NodePath path);
    godot::NodePath get_code_edit_path() const;

    bool is_initialized() const;
};

#endif