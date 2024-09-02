#ifndef CODE_FLOW_CONTROL_HEADER
#define CODE_FLOW_CONTROL_HEADER

#include "luaprogram_handle.h"

#include "godot_cpp/classes/button.hpp"
#include "godot_cpp/classes/control.hpp"


class CodeFlowControl: public godot::Control{
  GDCLASS(CodeFlowControl, godot::Control)

  public:
    enum button_enum{
      be_restart    = 0b1,
      be_resume     = 0b10,
      be_pause      = 0b100,
      be_step_in    = 0b1000,
      be_step_out   = 0b10000,
      be_step_over  = 0b100000,
      be_stop       = 0b1000000,
      be_allbutton  = 0b1111111
    };

  private:
    LuaProgramHandle* _program_handle;

    godot::NodePath _control_button_container_path;
    godot::Node* _control_button_container;

    godot::Button* _restart_button;
    godot::Button* _resume_button;
    godot::Button* _pause_button;
    godot::Button* _step_in_button;
    godot::Button* _step_out_button;
    godot::Button* _step_over_button;
    godot::Button* _stop_button;

    void _on_restart_button_pressed();
    void _on_resume_button_pressed();
    void _on_pause_button_pressed();
    void _on_step_in_button_pressed();
    void _on_step_out_button_pressed();
    void _on_step_over_button_pressed();
    void _on_stop_button_pressed();

    void _lua_on_starting();
    void _lua_on_stopping();
    void _lua_on_pausing();
    void _lua_on_resuming();

    void _check_blocking_on_start();

    typedef void (*_iterate_button_cb)(godot::Button* button, void* data);
    void _iterate_buttons(int type, _iterate_button_cb cb, void* data);

  protected:
    static void _bind_methods();

  public:
    CodeFlowControl();
    ~CodeFlowControl();

    void _ready() override;

    void show_button(int type, bool show);
    void disable_button(int type, bool disable);

    godot::NodePath get_control_button_container_path() const;
    void set_control_button_container_path(godot::NodePath path);
};

#endif