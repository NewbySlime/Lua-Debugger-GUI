#ifndef POPUP_CONTEXT_MENU_HEADER
#define POPUP_CONTEXT_MENU_HEADER

#include "godot_cpp/classes/popup_menu.hpp"

#include "vector"


class PopupContextMenu: public godot::PopupMenu{
  GDCLASS(PopupContextMenu, godot::PopupMenu)

  public:
    struct MenuData{
      enum Type{
        type_normal,
        type_icon,
        type_check,
        type_submenu,
        type_multistate,
        type_icon_check,
        type_radio_check,
        // TODO type_submenu_node,
        type_icon_radio_check,

        type_separator
      };

      struct Part{
        Type item_type;

        godot::String label;
        int32_t id = 0;

        godot::Key k_accel = godot::KEY_NONE;

        godot::Ref<godot::Texture> icon_texture;
        godot::String submenu_label;
        int32_t max_state, default_state;

        godot::String tooltip_text;
        bool is_disabled = false;
      };

      std::vector<Part> part_list;
    };


  private:
    void _on_popup();
    void _on_id_pressed(int id);

  protected:
    static void _bind_methods();

  public:
    void _ready() override;

    void init_menu(const MenuData& data);
};

#endif