#ifndef LUAVARIABLE_TREE_HEADER
#define LUAVARIABLE_TREE_HEADER

#include "global_variables.h"
#include "popup_context_menu.h"
#include "popup_variable_setter.h"
#include "reference_query_menu.h"

#include "godot_cpp/classes/confirmation_dialog.hpp"
#include "godot_cpp/classes/tree.hpp"

#include "Lua-CPPAPI/Src/luavariant.h"

#include "map"
#include "set"


class LuaVariableTree: public godot::Tree{
  GDCLASS(LuaVariableTree, godot::Tree)

  public:
    // NOTE: This can emit more than once in an update. 
    // Param:
    //  - PACKED_BYTE_ARRAY: data of table_reference_data
    static const char* s_on_reference_changed;

    struct reference_changed_data{
      const void* reference_address;
    };

  protected:
    enum _button_id_enum{
      button_id_context_menu = 0x1
      // To add more custom button enum, preferably to use more than 0x10000
    };

    enum _context_menu_id_enum{
      context_menu_edit       = 0x1,
      context_menu_add        = 0x2,
      context_menu_add_table  = 0x3,
      context_menu_copy       = 0x4,
      context_menu_remove     = 0x5,
      // To add more custom enum, preferably to use more than 0x10000
    };

    enum _metadata_flag{
      metadata_valid_mutable_item = 0b001,
      metadata_local_item = 0b010
    };

    struct _variable_tree_item_metadata{
      public:
        godot::TreeItem* this_item = NULL;

        lua::I_variant* this_key = NULL;
        lua::I_variant* this_value = NULL;
        
        bool already_revealed = false;

        // created when this_value is a table value (check _set_tree_item_value function)
        std::map<lua::comparison_variant, uint64_t>* child_lookup_list = NULL;
        
        uint64_t _mflag = 0;
    };

    // used to save and restore item states
    struct _item_state{
      public:
        std::map<lua::comparison_variant, _item_state*> branches;

        bool is_revealed = false;
    };

  private:
    bool _is_using_variable_setter = false;

    void _item_collapsed_safe(godot::TreeItem* item);
    void _item_collapsed(godot::TreeItem* item);
    void _item_selected();
    void _item_nothing_selected();
    void _item_selected_mouse(const godot::Vector2 mouse_pos, int mouse_idx);
    void _item_empty_clicked(const godot::Vector2 mouse_pos, int mouse_idx);
    void _item_activated();
    
    void _variable_setter_do_popup_add_table_item(godot::TreeItem* parent_item, uint64_t flag = PopupVariableSetter::edit_add_value_edit);
    void _variable_setter_do_popup_add_table_item(godot::TreeItem* parent_item, godot::TreeItem* value_item, uint64_t flag = PopupVariableSetter::edit_add_value_edit);

    void _on_setter_cancelled();
    void _on_setter_applied(const godot::Variant& pass_data);
    void _on_setter_applied_add_table(godot::TreeItem* current_item, lua::I_variant* key, lua::I_variant* value);
    void _on_setter_applied_add_table_confirmed_variant(const godot::Variant& data);
    void _on_setter_applied_add_table_cancelled_variant(const godot::Variant& data);
    void _on_setter_applied_add_table_confirmed(_variable_tree_item_metadata* _metadata, lua::I_variant* key, lua::I_variant* value);
    void _on_setter_applied_edit(godot::TreeItem* current_item, lua::I_variant* value);
    
    void _on_tree_button_clicked(godot::TreeItem* item, int column, int id, int mouse_button);

    void _on_reference_data_changed(const godot::Variant& data);
    void _on_reference_data_changed_filter(const reference_changed_data& data, const std::set<uint64_t>* item_filter);

    void _open_context_menu();
    void _on_context_menu_clicked(int id);

    void _add_reference_lookup_value(godot::TreeItem* item, const lua::I_variant* value);
    void _delete_reference_lookup_value(godot::TreeItem* item, const lua::I_variant* value);
    void _clear_reference_lookup_list();

  protected:
    struct _reference_lookup_data{
      std::set<uint64_t> item_list;
    };


    std::set<lua::comparison_variant> _filter_key = {
      lua::string_var("(*temporary)")
    };

    godot::NodePath _variable_tree_path;
    godot::Tree* _variable_tree = NULL;
    godot::Ref<godot::Texture> _context_menu_button_texture;

    std::map<uint64_t, _variable_tree_item_metadata*> _vartree_map;
    godot::ConfirmationDialog* _global_confirmation_dialog;
    PopupVariableSetter* _popup_variable_setter;
    PopupContextMenu* _global_context_menu;
    
    GlobalVariables* _gvariables;

    std::vector<godot::Callable> _update_callable_list;
    // key: table pointer, value: list of TreeItem IDs.
    std::map<uint64_t, _reference_lookup_data*> _reference_lookup_list;
    
    uint64_t _last_selected_id = 0;
    godot::TreeItem* _last_selected_item = NULL;

    uint64_t _last_context_id = 0;


    // item_state can be NULL to skip state checking.
    void _update_tree_item(godot::TreeItem* item, _item_state* item_state);
    // item_state can be NULL to skip state checking.
    void _reveal_tree_item(godot::TreeItem* item, _item_state* item_state);
    void _sort_item_child(godot::TreeItem* parent_item);
    
    void _store_item_state(godot::TreeItem* item, _item_state* item_state);

    // Key parameter can be NULL to clear the key value.
    // The function will handle adding/remove key and value from parent's table values. 
    void _set_tree_item_key(godot::TreeItem* item, const lua::I_variant* key);
    void _set_tree_item_key_direct(godot::TreeItem* item, lua::I_variant* key);
    // Value parameter can be NULL to clear the value.
    // The function will handle adding/remove key and value from parent's table values. 
    void _set_tree_item_value(godot::TreeItem* item, const lua::I_variant* value);
    void _set_tree_item_value_direct(godot::TreeItem* item, lua::I_variant* value);
    void _set_tree_item_value_as_copy(godot::TreeItem* item);
    void _set_tree_item_from_parent(godot::TreeItem* child_item, const lua::I_variant* key);
    // This both remove values from parent item and delete the TreeItem
    void _remove_tree_item(godot::TreeItem* item);

    godot::TreeItem* _create_tree_item(godot::TreeItem* parent_item);
    _variable_tree_item_metadata* _create_vartree_metadata(godot::TreeItem* item);

    void _clear_item_state(_item_state* node);
    void _delete_item_state(_item_state* node);
    void _clear_variable_tree();
    void _clear_variable_metadata_map();
    void _clear_variable_metadata(_variable_tree_item_metadata* metadata);
    void _delete_variable_metadata(_variable_tree_item_metadata* metadata);
    // this does recursively
    void _delete_tree_item_child(godot::TreeItem* item);
    // Only deletes the TreeItem object, not removing values from parent item. Use _remove_tree_item() function.
    void _delete_tree_item(godot::TreeItem* item);


    void _notify_changed_tree_item(godot::TreeItem* item);

    void _bind_object(LuaVariableTree* obj);

    
    virtual void _add_custom_context(_variable_tree_item_metadata* metadata, PopupContextMenu::MenuData& data){}
    virtual void _check_custom_context(int id){}
    virtual void _check_custom_button_id(int id){}
    virtual bool _check_ignored_variable(_variable_tree_item_metadata* metadata, const lua::I_variant* key){return false;}

    virtual void _on_item_created(godot::TreeItem* item){}
    virtual void _on_item_deleting(godot::TreeItem* item){}

    virtual void _on_variable_setter_popup();
    virtual void _on_variable_setter_closing();

    // pkey and pvalue will be cleaned after by caller
    virtual void _get_data_from_variable_setter(_variable_tree_item_metadata* target_metadata, lua::I_variant** pkey, lua::I_variant** pvalue);
    virtual void _get_reference_query_function(ReferenceQueryMenu::ReferenceQueryFunction* func){}

    virtual void _update_item_text(godot::TreeItem* item, const lua::I_variant* key, const lua::I_variant* value);

    static void _bind_methods();

  public:
    ~LuaVariableTree();

    void _ready() override;
    void _process(double delta) override;

    godot::NodePath get_variable_tree_path() const;
    void set_variable_tree_path(const godot::NodePath& path);

    godot::Ref<godot::Texture> get_context_menu_button_texture() const;
    void set_context_menu_button_texture(godot::Ref<godot::Texture> texture);
};

#endif