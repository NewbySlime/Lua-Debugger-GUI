#include "popup_context_menu.h"

#include "godot_cpp/classes/engine.hpp"


using namespace godot;


void PopupContextMenu::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_popup"), &PopupContextMenu::_on_popup);
  ClassDB::bind_method(D_METHOD("_on_id_pressed", "id"), &PopupContextMenu::_on_id_pressed);
}


void PopupContextMenu::_on_popup(){
  // adjust size to contained items
  set_size(Vector2(0, 0));
}

void PopupContextMenu::_on_id_pressed(int id){
  hide();
}


void PopupContextMenu::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  connect("about_to_popup", Callable(this, "_on_popup"));
  connect("id_pressed", Callable(this, "_on_id_pressed"));
}


void PopupContextMenu::init_menu(const MenuData& data){
  // resize to min size
  set_size(Vector2(0, 0));

  clear(true);
  for(const MenuData::Part& part: data.part_list){
    switch(part.item_type){
      break; case MenuData::type_normal:
        add_item(part.label, part.id, part.k_accel);

      break; case MenuData::type_icon:
        add_icon_item(part.icon_texture, part.label, part.id, part.k_accel);

      break; case MenuData::type_check:
        add_check_item(part.label, part.id, part.k_accel);

      break; case MenuData::type_submenu:
        add_submenu_item(part.label, part.submenu_label, part.id);

      break; case MenuData::type_multistate:
        add_multistate_item(part.label, part.max_state, part.default_state, part.id, part.k_accel);

      break; case MenuData::type_icon_check:
        add_icon_check_item(part.icon_texture, part.label, part.id, part.k_accel);

      break; case MenuData::type_radio_check:
        add_radio_check_item(part.label, part.id, part.k_accel);

      break; case MenuData::type_icon_radio_check:
        add_icon_radio_check_item(part.icon_texture, part.label, part.id, part.k_accel);

      break; case MenuData::type_separator:
        add_separator(part.label, part.id);
    }

    int idx = get_item_index(part.id);
    set_item_tooltip(idx, part.tooltip_text);
    set_item_disabled(idx, part.is_disabled);
  }
}