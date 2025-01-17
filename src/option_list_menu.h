#ifndef OPTION_LIST_MENU_HEADER
#define OPTION_LIST_MENU_HEADER

#include "godot_cpp/classes/control.hpp"

#include "map"


class OptionValueControl;
class OptionListMenu: public godot::Control{
  GDCLASS(OptionListMenu, godot::Control)

  public:
    // Param:
    //  - STRING: key
    //  - ANY: value
    static const char* s_value_set;

  private:
    std::map<godot::String, OptionValueControl*> _option_lists;

    void _on_option_changed(const godot::String& key, const godot::Variant& value);

    void _update_option_nodes(godot::Node* parent);

  protected:
    static void _bind_methods();

  public:
    void _ready() override;

    void set_value_data(const godot::String& key, const godot::Variant& value);
    godot::Variant get_value_data(const godot::String& key) const;
};

#endif