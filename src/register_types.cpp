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
#include "focus_area.h"
#include "global_variables.h"
#include "group_invoker.h"
#include "liblua_handle.h"
#include "luaprogram_handle.h"
#include "logger.h"
#include "node_data.h"
#include "option_control.h"
#include "option_list_menu.h"
#include "option_value_control.h"
#include "popup_context_menu.h"
#include "popup_variable_setter.h"
#include "slide_animation_control.h"
#include "splash_panel.h"
#include "split_ratio_maintainer.h"
#include "variable_storage.h"
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
  ClassDB::register_class<FocusArea>();
  ClassDB::register_class<GameUtils::Logger>();
  ClassDB::register_class<GlobalVariables>();
  ClassDB::register_class<GroupInvoker>();
  ClassDB::register_class<LuaProgramHandle>();
  ClassDB::register_class<LibLuaHandle>();
  ClassDB::register_class<NodeData>();
  ClassDB::register_class<OptionControl>();
  ClassDB::register_class<OptionListMenu>();
  ClassDB::register_class<OptionValueControl>();
  ClassDB::register_class<PopupContextMenu>();
  ClassDB::register_class<PopupVariableSetter>();
  ClassDB::register_class<SlideAnimationControl>();
  ClassDB::register_class<SplashPanel>();
  ClassDB::register_class<SplitRatioMaintainer>();
  ClassDB::register_class<VariableStorage>();
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