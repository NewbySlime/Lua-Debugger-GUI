#ifndef CODE_CONTEXT_HEADER
#define CODE_CONTEXT_HEADER

#include "godot_cpp/classes/code_edit.hpp"
#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/scroll_container.hpp"


#define SIGNAL_CODE_CONTEXT_FILE_LOADED "file_loaded"
#define SIGNAL_CODE_CONTEXT_BREAKPOINT_ADDED "breakpoint_added"
#define SIGNAL_CODE_CONTEXT_BREAKPOINT_REMOVED "breakpoint_removed"


class CodeContext: public godot::Control{
  GDCLASS(CodeContext, godot::Control)

  private:
    godot::NodePath _code_edit_path;
    godot::CodeEdit* _code_edit;

    std::string _current_file_path;

    godot::String _tmp_file_data;

    bool _initialized = false;

    std::vector<int> _breakpointed_list;


    void _breakpoint_toggled_cb(int line);

  protected:
    static void _bind_methods();

  public:
    CodeContext();
    ~CodeContext();

    void _ready() override;

    godot::Error load_file(const std::string& file_path);
    godot::Error reload_file();
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