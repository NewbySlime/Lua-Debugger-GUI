#ifndef POPUP_VARIABLE_SETTER_HEADER
#define POPUP_VARIABLE_SETTER_HEADER

#include "option_list_menu.h"

#include "godot_cpp/classes/button.hpp"
#include "godot_cpp/classes/option_button.hpp"
#include "godot_cpp/classes/popup_panel.hpp"

#include "map"


class PopupVariableSetter: public godot::PopupPanel{
  GDCLASS(PopupVariableSetter, godot::PopupPanel)

  public:
    static const char* s_applied;
    static const char* s_cancelled;

    // Param:
    //  - INT: new SetterMode
    static const char* s_mode_type_changed;

    static const char* key_string_data;
    static const char* key_number_data;
    static const char* key_boolean_data;

    static const char* key_enum_button;
    static const char* key_accept_button;
    static const char* key_cancel_button;


    enum SetterMode{
      setter_mode_number,
      setter_mode_string,
      setter_mode_bool
    };

    struct VariableData{
      public:
        int setter_mode;

        double number_data = 0;
        std::string string_data;
        bool bool_data = false;
    };

  
  private:
    struct _mode_node_data{
      public:
        struct _node_data{
          public:
            godot::Callable set_visible_funcs;
        };

        std::map<uint64_t, _node_data*> node_data_map;
    };

    godot::NodePath _option_list_path;
    godot::Dictionary _mode_node_list;

    VariableData _data_init;
    VariableData _data_output;

    SetterMode _current_mode;

    OptionListMenu* _option_list;

    godot::OptionButton* _enum_button;
    godot::Button* _accept_button;
    godot::Button* _cancel_button;

    std::map<int, _mode_node_data*> _mode_node_map;

    bool _enable_enum_button_signal = true;


    void _on_value_set(const godot::String& key, const godot::Variant& value);
    void _on_value_set_string_data(const godot::Variant& data);
    void _on_value_set_number_data(const godot::Variant& data);
    void _on_value_set_bool_data(const godot::Variant& data);

    void _on_accept_button_pressed();
    void _on_cancel_button_pressed();

    void _on_enum_button_selected(int idx);

    void _try_parse_mode_node_list();

    void _reset_enum_button_config();

    void _clear_mode_node_map();

    static void _code_initiate();

  protected:
    static void _bind_methods();

  public:
    ~PopupVariableSetter();

    void _ready() override;

    void set_mode_type(SetterMode mode);
    SetterMode get_mode_type() const;

    // VariableData::setter_mode is ignored when updated.
    void update_input_data_ui();

    // VariableData::setter_mode is ignored when updated.
    VariableData& get_input_data();
    const VariableData& get_output_data() const;    

    void set_option_list_path(const godot::NodePath& path);
    godot::NodePath get_option_list_path() const;

    void set_mode_node_list(const godot::Dictionary& node_list);
    godot::Dictionary get_mode_node_list() const;
};

#endif