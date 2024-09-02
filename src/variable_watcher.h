#ifndef VARIABLE_WATCHER_HEADER
#define VARIABLE_WATCHER_HEADER

#include "liblua_handle.h"
#include "luaprogram_handle.h"

#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/tree.hpp"
#include "godot_cpp/variant/node_path.hpp"


class VariableWatcher: public godot::Control{
  GDCLASS(VariableWatcher, godot::Control)

  private:
    std::set<std::string> _filter_key = {
      "(*temporary)"
    };

    LibLuaHandle* _lua_lib;
    std::shared_ptr<LibLuaStore> _lua_lib_data;

    LuaProgramHandle* _lua_program_handle;

    godot::NodePath _variable_tree_path;
    godot::Tree* _variable_tree = NULL;
  

    void _lua_on_pausing();
    void _lua_on_resuming();
    void _lua_on_stopping();

    void _update_tree_item(TreeItem* parent_item, lua::debug::I_variable_watcher* watcher);
    void _update_tree_item(TreeItem* parent_item, lua::I_table_var* var);

    void _update_variable_tree();
    void _clear_variable_tree();

  protected:
    static void _bind_methods();

  public:
    VariableWatcher();
    ~VariableWatcher();

    void _ready() override;

    godot::NodePath get_variable_tree_path() const;
    void set_variable_tree_path(godot::NodePath path);
};

#endif