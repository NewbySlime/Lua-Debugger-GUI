#ifndef POPUP_VARIABLE_SETTER_HEADER
#define POPUP_VARIABLE_SETTER_HEADER

#include "group_invoker.h"
#include "option_list_menu.h"

#include "godot_cpp/classes/button.hpp"
#include "godot_cpp/classes/option_button.hpp"
#include "godot_cpp/classes/popup_panel.hpp"

#include "Lua-CPPAPI/Src/luavariant.h"

#include "map"


class PopupVariableSetter: public godot::PopupPanel{
  GDCLASS(PopupVariableSetter, godot::PopupPanel)

  public:
    // Note: Use SignalOwnership
    static const char* s_applied;
    static const char* s_cancelled;

    // Param:
    //  - INT: new SetterMode
    static const char* s_mode_type_changed;

    // This assumes that the parent of the node will be hidden if not used.
    static const char* key_global_key_data;
    // This assumes that the parent of the node will be hidden if not used.
    static const char* key_local_key_data;
    static const char* key_string_data;
    static const char* key_number_data;
    static const char* key_boolean_data;

    static const char* key_type_enum_button;
    static const char* key_accept_button;
    static const char* key_cancel_button;


    enum SetterMode{
      setter_mode_number,
      setter_mode_string,
      setter_mode_bool
    };

    enum EditFlag{
      edit_flag_none = 0,
      edit_add_key_edit = 0b001,
      edit_local_key = 0b010,       // if not set, UI will be setup for global key
      edit_clear_on_popup = 0b100
    };

    struct VariableData{
      public:
        int setter_mode = setter_mode_number;

        double number_data = 0;
        std::string string_data;
        bool bool_data = false;
    };

  
  private:
    godot::NodePath _option_list_path;

    GroupInvoker* _ginvoker;

    VariableData _data_init;
    VariableData _data_output;

    uint32_t _current_mode;

    OptionListMenu* _option_list; 

    godot::Button* _accept_button;
    godot::Button* _cancel_button;

    std::map<int, godot::String> _local_key_lookup;

    godot::String _key_name;
    uint32_t _edit_flag;

    bool _type_enum_button_signal = false;
    bool _applied = false;


    void _on_value_set(const godot::String& key, const godot::Variant& value);
    void _on_value_set_string_data(const godot::Variant& data);
    void _on_value_set_number_data(const godot::Variant& data);
    void _on_value_set_bool_data(const godot::Variant& data);
    void _on_value_set_type_enum_data(const godot::Variant& data);

    void _on_accept_button_pressed();
    void _on_cancel_button_pressed();
    void _on_popup();
    void _on_popup_hide();

    void _reset_enum_button_config();

  void _update_setter_ui();

    static void _code_initiate();

  protected:
    static void _bind_methods();

  public:
    void _ready() override;

    void set_mode_type(uint32_t mode);
    uint32_t get_mode_type() const;

    // DEPRECATED, always update on popup
    // VariableData::setter_mode is ignored when updated.
    void update_input_data_ui();

    // VariableData::setter_mode is ignored when updated.
    VariableData& get_input_data();
    void clear_input_data();

    const VariableData& get_output_data() const;

    // if var NULL, it will reset
    void set_popup_data(const lua::I_variant* var = NULL);

    // DEPRECATED, use SignalOwnership on s_applied
    // NOTE: the callable will be unbound when the popup is no longer be used (on popup_hide).
    //void bind_apply_callable(const godot::Callable& cb);

    void set_edit_flag(uint32_t flag);
    uint32_t get_edit_flag() const;

    void set_local_key_choice(const godot::PackedStringArray& key_list);
    godot::String get_local_key_applied() const;

    void set_global_key(const godot::String& key);
    godot::String get_global_key() const;

    void set_option_list_path(const godot::NodePath& path);
    godot::NodePath get_option_list_path() const;
};

#endif