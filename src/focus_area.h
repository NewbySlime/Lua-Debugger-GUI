#ifndef FOCUS_AREA_HEADER
#define FOCUS_AREA_HEADER

#include "godot_cpp/classes/button.hpp"
#include "godot_cpp/classes/base_button.hpp"


class FocusArea: public godot::BaseButton{
  GDCLASS(FocusArea, godot::BaseButton)

  private:
    std::set<godot::String> _list_disable = {
      "layout_mode",
      "anchors_preset",
      "focus_mode",
      "mouse_filter"
    };

    bool _ignore_focus = false;

    void _on_resized();

    void _reset_config();

  protected:
    static void _bind_methods();

  public:
    void _ready() override;

    bool _set(const godot::StringName& pname, const godot::Variant& value);
    void _validate_property(godot::PropertyInfo& property) const;

    void set_ignore_focus(bool flag);
    bool get_ignore_focus() const;
};

#endif