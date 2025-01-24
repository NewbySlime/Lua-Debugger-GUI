#ifndef VARIABLE_WATCHER_HEADER
#define VARIABLE_WATCHER_HEADER

#include "global_variables.h"
#include "liblua_handle.h"
#include "luaprogram_handle.h"
#include "option_control.h"
#include "popup_variable_setter.h"

#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/tree.hpp"
#include "godot_cpp/variant/node_path.hpp"

#include "map"


class VariableWatcher: public godot::Control{
  GDCLASS(VariableWatcher, godot::Control)

  private:
    struct _variable_tree_item_metadata{
      public:
        int variable_type;
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

    GlobalVariables* _gvariables;

    PopupVariableSetter* _popup_variable_setter;

    bool _ignore_internal_variables = false;


    void _on_global_variable_changed(const godot::String& key, const godot::Variant& value);
    void _gvar_changed_option_control_path(const godot::Variant& value);
  
    void _on_option_control_changed(const godot::String& key, const godot::Variant& value);
    void _option_changed_ignore_internal_data(const godot::Variant& value);

    void _lua_on_thread_starting();
    void _lua_on_pausing();
    void _lua_on_resuming();
    void _lua_on_stopping();

    void _item_selected(const godot::Vector2 mouse_pos, int mouse_idx);
    void _item_activated();

    void _variable_setter_do_popup();

    void _update_tree_item(godot::TreeItem* parent_item, lua::debug::I_variable_watcher* watcher);
    void _update_tree_item(godot::TreeItem* parent_item, const lua::I_table_var* var);
    void _update_tree_item(godot::TreeItem* parent_item, const lua::I_variant* key_var, const lua::I_variant* var);

    void _update_global_variables();
    void _update_local_variables();
    void _clear_variable_data();

    void _update_variable_tree();
    void _clear_variable_tree();

    void _bind_object(OptionControl* obj);

  protected:
    static void _bind_methods();

  public:
    VariableWatcher();
    ~VariableWatcher();

    void _ready() override;

    void set_ignore_internal_variables(bool flag);
    bool get_ignore_internal_variables() const;

    godot::NodePath get_variable_tree_path() const;
    void set_variable_tree_path(godot::NodePath path);
};

#endif