#ifndef CODE_CONTEXT_MENU_HEADER
#define CODE_CONTEXT_MENU_HEADER

#include "godot_cpp/classes/button.hpp"
#include "godot_cpp/classes/control.hpp"


#define SIGNAL_CODE_CONTEXT_MENU_BUTTON_PRESSED "button_pressed"


class CodeContextMenu: public godot::Control{
  GDCLASS(CodeContextMenu, godot::Control)

  public:
    enum button_enum{
      be_opening    = 0b1,
      be_closing    = 0b10,
      be_running    = 0b100,
      be_refresh    = 0b1000,
      be_allbutton  = 0b1111
    };


  private:
    godot::NodePath _button_container_path;
    godot::Node* _button_container;

    godot::Button* _opening_button;
    godot::Button* _closing_button;
    godot::Button* _running_button;
    godot::Button* _refresh_button;

    bool _initialized = false;

    
    void _opening_button_pressed();
    void _closing_button_pressed();
    void _running_button_pressed();
    void _refresh_button_pressed();

    typedef void (*_iterate_button_cb)(godot::Button* button, void* data);
    void _iterate_button(button_enum type, _iterate_button_cb cb, void* data);

  protected:
    static void _bind_methods();

  public:
    CodeContextMenu();
    ~CodeContextMenu();

    void _ready() override;

    void show_button(button_enum button, bool show);
    void disable_button(button_enum button, bool flag);

    bool is_initialized() const;
    
    godot::NodePath get_button_container_path() const;
    void set_button_container_path(godot::NodePath path);
};

#endif