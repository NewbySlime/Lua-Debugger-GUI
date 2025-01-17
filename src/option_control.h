#ifndef OPTION_CONTROL_HEADER
#define OPTION_CONTROL_HEADER

#include "global_variables.h"
#include "option_list_menu.h"
#include "slide_animation_control.h"

#include "godot_cpp/classes/animation_player.hpp"
#include "godot_cpp/classes/button.hpp"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/texture.hpp"
#include "godot_cpp/variant/dictionary.hpp"


class OptionControl: public godot::Node{
  GDCLASS(OptionControl, godot::Node)

  public:
    static const char* default_option_config_file_path;
    static const char* default_config_gvarname;
    static const char* expected_config_vartype_gvarname;

    static const char* gvar_object_node_path;

    // Param:
    //  - STRING: key
    //  - ANY: value
    static const char* s_value_set;

  private:
    godot::Ref<godot::Texture> _logo_settings_image;
    godot::Ref<godot::Texture> _logo_settings_close_image;

    godot::NodePath _animation_player_path;
    godot::NodePath _settings_button_path;
    godot::NodePath _settings_unfocus_area_path;
    godot::NodePath _option_menu_path;
    godot::String _config_file_path = default_option_config_file_path;

    GlobalVariables* _gvariables;    

    godot::AnimationPlayer* _animation_player;
    godot::Button* _settings_button;
    godot::Control* _settings_unfocus_area;

    OptionListMenu* _option_menu;

    godot::Dictionary _option_data;

    bool _is_showing = false;

    void _on_value_set(const godot::String& key, const godot::Variant& value);

    void _on_option_button_pressed();
    void _on_option_focus_exited();

    void _on_option_list_menu_ready(godot::Node* node);

    void _update_option_ui();

    void _play_show_option_ui(bool hide = false);
    void _show_option_ui();
    void _hide_option_ui();

  protected:
    static void _bind_methods();

  public:
    void _ready() override;

    godot::Error save_option_data();
    godot::Error load_option_data(bool update_ui = true);

    void set_option_value(const godot::String& key, const godot::Variant& value, bool update_ui = true);
    godot::Variant get_option_value(const godot::String& key);

    void set_logo_settings_image(godot::Ref<godot::Texture> image);
    godot::Ref<godot::Texture> get_logo_settings_image() const;

    void set_logo_settings_close_image(godot::Ref<godot::Texture> image);
    godot::Ref<godot::Texture> get_logo_settings_close_image() const;

    void set_animation_player(const godot::NodePath& path);
    godot::NodePath get_animation_player() const;

    void set_settings_button(const godot::NodePath& path);
    godot::NodePath get_settings_button() const;

    void set_settings_unfocus_area(const godot::NodePath& path);
    godot::NodePath get_settings_unfocus_area() const;

    void set_option_menu_path(const godot::NodePath& path);
    godot::NodePath get_option_menu_path() const;
};

#endif