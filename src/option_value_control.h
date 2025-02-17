#ifndef OPTION_VALUE_CONTROL_HEADER
#define OPTION_VALUE_CONTROL_HEADER

#include "godot_cpp/classes/box_container.hpp"
#include "godot_cpp/variant/node_path.hpp"

#include "string"
#include "vector"


// NOTE:
//  For OptionButton, the value will be a type of Dictionary consisting of:
//    - picked (INT): choice (key ID) that is picked by user.
//    - choices (DICTIONARY): choices that can be picked by user. Consisting of:
//      - KEY (INT): the ID of the choice.
//      - VALUE (STRING): the user-readeable name of the choice.
//  
//  Setter does not need to include all of the keys.
//  Getter will return all the data.

class OptionValueControl: public godot::BoxContainer{
  GDCLASS(OptionValueControl, godot::BoxContainer)

  public:
    // Param:
    //  - STRING: key
    //  - ANY: value
    static const char* s_value_set;

  private:
    godot::Node* _option_control_node = NULL;

    godot::String _option_key;

    void _on_changed_range(float num);
    void _on_changed_option_button(int idx);
    void _on_changed_button(bool toggle);
    void _on_changed_line_edit(const godot::String& new_text);
    void _on_changed_text_edit();

    void _set_option_button_value(const godot::Variant& value);
    godot::Variant _get_option_button_value() const;

    godot::Node* _find_control_node(godot::Node* parent);
    
  protected:
    static void _bind_methods();

  public:
    void _ready() override;

    // Might return empty path if this object not yet ready or value control node cannot be found. 
    // If relative_node is NULL, returned path is absolute.
    godot::NodePath get_value_control_path(godot::Node* relative_node = NULL) const; 
    // Might return NULL if this object not yet ready or value control node cannot be found.
    godot::Node* get_value_control_node() const;

    void set_option_key(const godot::String& key);
    godot::String get_option_key() const;

    void set_option_value(const godot::Variant& value);
    godot::Variant get_option_value() const;
};

#endif