#ifndef EXECUTION_CONTEXT_HEADER
#define EXECUTION_CONTEXT_HEADER

#include "luaprogram_handle.h"

#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/label.hpp"
#include "godot_cpp/variant/node_path.hpp"


class ExecutionContext: public godot::Control{
  GDCLASS(ExecutionContext, godot::Control)

  private:
    LuaProgramHandle* _program_handle;

    godot::NodePath _execution_info_path;
    godot::Label* _execution_info;

    void _on_lua_starting();
    void _on_lua_stopping();
    void _on_lua_pausing();
    void _on_lua_resuming();

    void _toggle_gray(bool flag);

    void _update_execution_info();
    void _clear_execution_info();

  protected:
    static void _bind_methods();

  public:
    ExecutionContext();
    ~ExecutionContext();

    void _ready() override;

    godot::NodePath get_execution_info_path() const;
    void set_execution_info_path(godot::NodePath path);
};

#endif