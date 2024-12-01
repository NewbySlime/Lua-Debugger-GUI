#ifndef CONSOLE_WINDOWS_HEADER
#define CONSOLE_WINDOWS_HEADER

#include "luaprogram_handle.h"

#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/line_edit.hpp"
#include "godot_cpp/classes/text_edit.hpp"
#include "godot_cpp/variant/node_path.hpp"


#define SIGNAL_CONSOLE_ON_INPUT_ENTERED "on_input_entered"


class ConsoleWindow: public godot::Control{
  GDCLASS(ConsoleWindow, godot::Control)

  private:
    LuaProgramHandle* _program_handle;

    // NOTE: This buffer uses repeating indexing
    char* _output_buffer;
    int _ob_index_top = 0;
    int _ob_index_bottom = 0;
    bool _ob_is_repeating = false;

    godot::NodePath _output_text_path;
    godot::TextEdit* _output_text;
    godot::NodePath _input_text_path;
    godot::LineEdit* _input_text;

    void _on_output_written(godot::String str);
    void _on_input_entered(godot::String str);

    void _add_string_to_output_buffer(const std::string& str);
    void _write_to_output_text();

  protected:
    static void _bind_methods();

  public:
    ConsoleWindow();
    ~ConsoleWindow();

    void _ready() override;

    void clear_output_buffer();
    void append_output_buffer(const std::string& str);

    godot::NodePath get_output_text_path() const;
    void set_output_text_path(godot::NodePath path);

    godot::NodePath get_input_text_path() const;
    void set_input_text_path(godot::NodePath path);
};

#endif