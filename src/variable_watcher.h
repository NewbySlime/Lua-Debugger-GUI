#ifndef VARIABLE_WATCHER_HEADER
#define VARIABLE_WATCHER_HEADER

#include "global_variables.h"
#include "liblua_handle.h"
#include "luaprogram_handle.h"
#include "luavariable_setter.h"
#include "option_control.h"
#include "popup_context_menu.h"
#include "popup_variable_setter.h"

#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/popup_menu.hpp"
#include "godot_cpp/classes/tree.hpp"
#include "godot_cpp/variant/node_path.hpp"

#include "map"


class VariableWatcher: public godot::Control{
  GDCLASS(VariableWatcher, godot::Control)

  private:
    enum _button_id_enum{
      button_id_context_menu
    };

    enum _context_menu_id_enum{
      context_menu_edit,
      context_menu_add,
      context_menu_copy,
      context_menu_remove
    };

    enum _metadata_flag{
      metadata_global_item = 0b100,
      metadata_local_item = 0b010,
      metadata_valid_mutable_item = 0b001
    };

    struct _variable_tree_item_metadata{
      public:
        godot::TreeItem* item = NULL;

        uint64_t _mflag = 0;

        lua::util::IVariableSetter* var_setter = NULL;
        lua::I_variant* this_key = NULL;
        lua::I_variant* this_value = NULL;

        bool already_revealed = false;
    };


    std::set<lua::comparison_variant> _filter_key = {
      lua::string_var("(*temporary)")
    };

    LibLuaHandle* _lua_lib;
    std::shared_ptr<LibLuaStore> _lua_lib_data;

    std::set<const void*> _parsed_table_list;

    LuaProgramHandle* _lua_program_handle = NULL;

    godot::NodePath _variable_tree_path;
    godot::Tree* _variable_tree = NULL;
    godot::Ref<godot::Texture> _context_menu_button_texture;

    godot::TreeItem* _global_item = NULL;
    godot::TreeItem* _local_item = NULL;

    GlobalVariables* _gvariables;

    std::vector<lua::util::IVariableSetter*> _setter_list;
    std::map<uint64_t, _variable_tree_item_metadata*> _vartree_map;
    PopupVariableSetter* _popup_variable_setter;
    PopupContextMenu* _global_context_menu;

    bool _ignore_internal_variables = false;

    std::vector<godot::Callable> _update_callable_list;

    uint64_t _last_selected_id = 0;
    godot::TreeItem* _last_selected_item = NULL;


    void _on_global_variable_changed(const godot::String& key, const godot::Variant& value);
    void _gvar_changed_option_control_path(const godot::Variant& value);
  
    void _on_option_control_changed(const godot::String& key, const godot::Variant& value);
    void _option_changed_ignore_internal_data(const godot::Variant& value);

    void _lua_on_thread_starting();
    void _lua_on_pausing();
    void _lua_on_resuming();
    void _lua_on_stopping();
 
    void _item_collapsed_safe(godot::TreeItem* item);
    void _item_collapsed(godot::TreeItem* item);
    void _item_selected(const godot::Vector2 mouse_pos, int mouse_idx);
    void _item_activated();

    void _on_setter_applied(godot::Node* node);
    void _on_tree_button_clicked(godot::TreeItem* item, int column, int id, int mouse_button);
    void _on_context_menu_clicked(int id);

    void _variable_setter_do_popup(uint64_t flag = 0);
    void _variable_setter_do_popup_id(uint64_t id, uint64_t flag = 0);

    void _open_context_menu();

    void _update_variable_tree();
    void _update_tree_item(godot::TreeItem* parent_item, lua::debug::I_variable_watcher* watcher, bool as_global);
    // NOTE: don't use any variant object from the metedata, as it will be deleted when updated.
    void _update_tree_item(godot::TreeItem* parent_item, const lua::I_variant* key_var, lua::I_variant* var);

    void _reveal_tree_item(godot::TreeItem* parent_item, lua::I_table_var* var);

    godot::TreeItem* _create_tree_item(godot::TreeItem* parent_item);
    _variable_tree_item_metadata* _create_vartree_metadata(godot::TreeItem* item);

    void _remove_item(godot::TreeItem* item);

    void _clear_variable_tree();
    void _clear_vsetter_list();
    void _clear_variable_metadata_map();
    void _clear_variable_metadata(_variable_tree_item_metadata* metadata);
    void _delete_variable_metadata(_variable_tree_item_metadata* metadata);
    // this does recursively
    void _delete_tree_item_child(godot::TreeItem* item);
    void _delete_tree_item(godot::TreeItem* item);

    void _bind_object(OptionControl* obj);

  protected:
    static void _bind_methods();

  public:
    VariableWatcher();
    ~VariableWatcher();

    void _ready() override;
    void _process(double delta) override;

    void set_ignore_internal_variables(bool flag);
    bool get_ignore_internal_variables() const;

    godot::NodePath get_variable_tree_path() const;
    void set_variable_tree_path(godot::NodePath path);

    godot::Ref<godot::Texture> get_context_menu_button_texture() const;
    void set_context_menu_button_texture(godot::Ref<godot::Texture> texture);
};

#endif