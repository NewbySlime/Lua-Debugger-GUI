#ifndef VARIABLE_STORAGE_HEADER
#define VARIABLE_STORAGE_HEADER

#include "global_variables.h"
#include "group_invoker.h"
#include "liblua_handle.h"
#include "luaprogram_handle.h"
#include "popup_context_menu.h"
#include "popup_variable_setter.h"

#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/tree.hpp"
#include "godot_cpp/classes/tree_item.hpp"
#include "godot_cpp/variant/node_path.hpp"

#include "Lua-CPPAPI/src/luavariant.h"

#include "map"


class VariableStorage: public godot::Control{
  GDCLASS(VariableStorage, godot::Control)

  public:
    enum storage_flag{
      sf_skip_checks              = 0b001,
      sf_skip_alias_replacement   = 0b010,
      sf_store_as_reference       = 0b100
    };

    static const char* s_table_data_changed;


  private:
    enum _context_menu_type{
      context_menu_change_alias,
      context_menu_as_copy
    };

    struct _item_metadata{
      godot::TreeItem* _this;

      lua::I_variant* value = NULL;
      bool already_revealed = false;
    };

    LuaProgramHandle* _program_handle;

    LibLuaHandle* _lua_lib;
    std::shared_ptr<LibLuaStore> _lua_lib_data;

    godot::NodePath _variable_tree_path;
    godot::Tree* _variable_tree = NULL;
    godot::TreeItem* _root_item = NULL;
    godot::Ref<godot::Texture> _context_menu_button_icon;

    GlobalVariables* _gvariables;
    GroupInvoker* _ginvoker = NULL;

    PopupContextMenu* _context_menu = NULL;
    PopupVariableSetter* _variable_setter = NULL;

    std::map<uint64_t, _item_metadata*> _metadata_map;

    std::vector<godot::Callable> _safe_callable_list;

    uint32_t _last_context_enum;
    uint64_t _last_selected_id;

    void _lua_on_stopping();

    void _on_context_menu_clicked(int id);
    void _on_context_menu_change_alias();
    void _on_context_menu_as_copy();

    void _on_setter_applied();

    void _add_new_tree_item(const lua::I_variant* var, const lua::I_variant* alias, uint32_t flags, godot::TreeItem* parent_item = NULL);
    void _reveal_tree_item(godot::TreeItem* item);
    void _update_tree_item(godot::TreeItem* item, const lua::I_variant* alias = NULL, uint32_t flags = 0);
    void _update_tree_item_child(godot::TreeItem* item);
    void _update_placeholder_state();

    void _open_context_menu();

    void _item_collapsed_safe(godot::TreeItem* item);
    void _item_collapsed(godot::TreeItem* item);
    void _item_selected();
    void _item_nothing_selected();
    void _item_selected_mouse(const godot::Vector2 mouse_pos, int mouse_idx);
    void _item_empty_clicked(const godot::Vector2 mouse_pos, int mouse_idx);
    void _item_activated();

    _item_metadata* _create_metadata(godot::TreeItem* item);
    godot::TreeItem* _create_item(godot::TreeItem* parent_item = NULL);

    void _delete_metadata(_item_metadata* metadata);
    void _delete_tree_item(godot::TreeItem* item);
    void _delete_tree_item_child(godot::TreeItem* item);

    void _clear_metadata(_item_metadata* metadata);
    void _clear_metadata_map();
    void _clear_variable_tree();

  protected:
    static void _bind_methods();

  public:
    ~VariableStorage();

    void _ready() override;
    void _process(double delta) override;

    void add_to_storage(const lua::I_variant* var, const lua::I_variant* alias = NULL, uint32_t flags = 0);

    void set_variable_tree_path(const godot::NodePath& path);
    godot::NodePath get_variable_tree_path() const;

    void set_context_menu_button_icon(godot::Ref<godot::Texture> image);
    godot::Ref<godot::Texture> get_context_menu_button_icon() const;
};

#endif