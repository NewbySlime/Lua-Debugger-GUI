#ifndef VARIABLE_STORAGE_HEADER
#define VARIABLE_STORAGE_HEADER

#include "global_variables.h"
#include "group_invoker.h"
#include "liblua_handle.h"
#include "luaprogram_handle.h"
#include "luavariable_tree.h"
#include "popup_context_menu.h"
#include "popup_variable_setter.h"
#include "reference_query_menu.h"

#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/tree.hpp"
#include "godot_cpp/classes/tree_item.hpp"
#include "godot_cpp/variant/node_path.hpp"

#include "Lua-CPPAPI/src/luavariant.h"

#include "map"


class VariableWatcher;

class VariableStorage: public LuaVariableTree{
  GDCLASS(VariableStorage, LuaVariableTree)

  public:
    enum storage_flag{
      sf_store_as_reference = 0b1
    };

  private:
    enum _context_menu_type{
      context_menu_as_copy = 0x10001
    };

    LuaProgramHandle* _program_handle;

    LibLuaHandle* _lua_lib;
    std::shared_ptr<LibLuaStore> _lua_lib_data;

    godot::TreeItem* _root_item = NULL;
    godot::Ref<godot::Texture> _context_menu_button_icon;

    GroupInvoker* _ginvoker = NULL;

    PopupContextMenu* _context_menu = NULL;
    PopupVariableSetter* _variable_setter = NULL;

    godot::NodePath _vwatcher_path;
    VariableWatcher* _vwatcher = NULL;

    bool _flag_check_placeholder_state = false;

    void _lua_on_stopping();

    void _on_context_menu_change_alias();
    void _on_context_menu_as_copy();

    void _update_placeholder_state();

    void _add_custom_context(_variable_tree_item_metadata* metadata, PopupContextMenu::MenuData& data) override;
    void _check_custom_context(int id) override;

    void _on_item_created(godot::TreeItem* item) override;
    void _on_item_deleting(godot::TreeItem* item) override;

    void _get_reference_query_function(ReferenceQueryMenu::ReferenceQueryFunction* func) override;
  
    void _reference_query_data(int item_offset, int item_length, const godot::Variant& result_data);
    int _reference_query_item_count();
    godot::PackedByteArray _reference_fetch_value(uint64_t item_id);

    void _update_item_text(godot::TreeItem* item, const lua::I_variant* key, const lua::I_variant* value) override;

  protected:
    static void _bind_methods();

  public:
    ~VariableStorage();

    void _ready() override;
    void _process(double delta) override;

    void add_to_storage(const lua::I_variant* var, const lua::I_variant* key = NULL, uint32_t flags = 0);

    ReferenceQueryMenu::ReferenceQueryFunction get_reference_query_function();

    void set_variable_watcher_path(const godot::NodePath& path);
    godot::NodePath get_variable_watcher_path() const;
};

#endif