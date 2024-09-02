#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

// includes for registered class
#include "code_context.h"
#include "code_context_menu.h"
#include "code_flow_control.h"
#include "code_window.h"
#include "console_window.h"
#include "execution_context.h"
#include "liblua_handle.h"
#include "luaprogram_handle.h"
#include "logger.h"
#include "variable_watcher.h"

using namespace godot;


void initialize_gdextension_module(ModuleInitializationLevel p_level) {
  if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
    return;
  }

  // set all the class here
  ClassDB::register_class<CodeContext>();
  ClassDB::register_class<CodeContextMenu>();
  ClassDB::register_class<CodeFlowControl>();
  ClassDB::register_class<CodeWindow>();
  ClassDB::register_class<ConsoleWindow>();
  ClassDB::register_class<ExecutionContext>();
  ClassDB::register_class<GameUtils::Logger>();
  ClassDB::register_class<LuaProgramHandle>();
  ClassDB::register_class<LibLuaHandle>();
  ClassDB::register_class<VariableWatcher>();
}

void uninitialize_gdextension_module(ModuleInitializationLevel p_level) {
  if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
    return;
  }
}

extern "C" {
  // Initialization.
  GDExtensionBool GDE_EXPORT gdextension_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_gdextension_module);
    init_obj.register_terminator(uninitialize_gdextension_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
  }
}