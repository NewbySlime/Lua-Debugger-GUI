#ifndef VARIABLE_WATCHER_HEADER
#define VARIABLE_WATCHER_HEADER

#include "global_variables.h"
#include "group_invoker.h"
#include "liblua_handle.h"
#include "luaprogram_handle.h"
#include "luavariable_tree.h"
#include "option_control.h"
#include "popup_context_menu.h"
#include "popup_variable_setter.h"

#include "godot_cpp/classes/confirmation_dialog.hpp"
#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/popup_menu.hpp"
#include "godot_cpp/classes/tree.hpp"
#include "godot_cpp/variant/node_path.hpp"

#include "map"


class VariableStorage;

class VariableWatcher: public LuaVariableTree{
  GDCLASS(VariableWatcher, LuaVariableTree)

  private:
    enum _context_menu_id_enum{
      context_menu_add_to_storage_copy        = 0x10001,
      context_menu_add_to_storage_reference   = 0x10002
    };

    std::set<lua::comparison_variant> _filter_key = {
      lua::string_var("(*temporary)")
    };

    LibLuaHandle* _lua_lib;
    std::shared_ptr<LibLuaStore> _lua_lib_data;

    LuaProgramHandle* _lua_program_handle = NULL;

    godot::Ref<godot::Texture> _context_menu_button_texture;
    
    godot::TreeItem* _global_item = NULL;
    godot::TreeItem* _local_item = NULL;
    
    GroupInvoker* _ginvoker = NULL;

    godot::NodePath _vstorage_node_path;
    VariableStorage* _vstorage;

    bool _ignore_internal_variables = false;

    _item_state _global_item_state_tree;
    // NOTE: first layer of the tree is about each function data.
    _item_state _local_item_state_tree;

    void _on_global_variable_changed(const godot::String& key, const godot::Variant& value);
    void _gvar_changed_option_control_path(const godot::Variant& value);
  
    void _on_option_control_changed(const godot::String& key, const godot::Variant& value);
    void _option_changed_ignore_internal_data(const godot::Variant& value);

    void _lua_on_thread_starting();
    void _lua_on_pausing();
    void _lua_on_resuming();
    void _lua_on_stopping();
    
    void _update_variable_tree();
    void _update_item_state_tree();
    void _update_placeholder_state();

    void _add_custom_context(_variable_tree_item_metadata* metadata, PopupContextMenu::MenuData& data) override;
    void _check_custom_context(int id) override;
    bool _check_ignored_variable(_variable_tree_item_metadata* metadata, const lua::I_variant* key) override;

    void _on_variable_setter_popup() override;

    void _get_reference_query_function(ReferenceQueryMenu::ReferenceQueryFunction* func) override;

    void _bind_object(OptionControl* obj);

  protected:
    static void _bind_methods();

  public:
    ~VariableWatcher();

    void _ready() override;

    void set_ignore_internal_variables(bool flag);
    bool get_ignore_internal_variables() const;  

    godot::NodePath get_variable_storage_path() const;
    void set_variable_storage_path(const godot::NodePath& path);

};

#endif