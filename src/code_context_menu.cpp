#include "code_context_menu.h"
#include "error_trigger.h"
#include "logger.h"


#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/class_db.hpp"

using namespace godot;


#define BUTTON_OPENING_NODE_NAME "ButtonOpen"
#define BUTTON_CLOSING_NODE_NAME "ButtonClose"
#define BUTTON_RUNNING_NODE_NAME "ButtonRun"
#define BUTTON_REFRESH_NODE_NAME "ButtonRefresh"


void CodeContextMenu::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_opening_button_pressed"), &CodeContextMenu::_opening_button_pressed);
  ClassDB::bind_method(D_METHOD("_closing_button_pressed"), &CodeContextMenu::_closing_button_pressed);
  ClassDB::bind_method(D_METHOD("_running_button_pressed"), &CodeContextMenu::_running_button_pressed);
  ClassDB::bind_method(D_METHOD("_refresh_button_pressed"), &CodeContextMenu::_refresh_button_pressed);

  ClassDB::bind_method(D_METHOD("get_button_container_path"), &CodeContextMenu::get_button_container_path);
  ClassDB::bind_method(D_METHOD("set_button_container_path", "path"), &CodeContextMenu::set_button_container_path);
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "button_container_path"), "set_button_container_path", "get_button_container_path");

  ADD_SIGNAL(MethodInfo(SIGNAL_CODE_CONTEXT_MENU_BUTTON_PRESSED, PropertyInfo(Variant::INT, "button_enum")));
}


CodeContextMenu::CodeContextMenu(){

}

CodeContextMenu::~CodeContextMenu(){

}


void CodeContextMenu::_opening_button_pressed(){
  emit_signal(SIGNAL_CODE_CONTEXT_MENU_BUTTON_PRESSED, Variant(be_opening));
}

void CodeContextMenu::_closing_button_pressed(){
  emit_signal(SIGNAL_CODE_CONTEXT_MENU_BUTTON_PRESSED, Variant(be_closing));
}

void CodeContextMenu::_running_button_pressed(){
  emit_signal(SIGNAL_CODE_CONTEXT_MENU_BUTTON_PRESSED, Variant(be_running));
}

void CodeContextMenu::_refresh_button_pressed(){
  emit_signal(SIGNAL_CODE_CONTEXT_MENU_BUTTON_PRESSED, Variant(be_refresh));
}


void CodeContextMenu::_iterate_button(button_enum type, _iterate_button_cb cb, void* data){
  for(int i = 1; i <= be_allbutton; i = i << 1){
    Button* _button = NULL;
    
    switch(type & i){
      break; case be_opening: _button = _opening_button;
      break; case be_closing: _button = _closing_button;
      break; case be_running: _button = _running_button;
      break; case be_refresh: _button = _refresh_button;
    }

    if(!_button)
      continue;

    cb(_button, data);
  }
}


void CodeContextMenu::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

  _button_container = get_node<Node>(_button_container_path);
  if(!_button_container){
    GameUtils::Logger::print_err_static("[CodeContextMenu] Cannot get specified node for Button Container.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

#define FETCH_BUTTON(variable, name) \
  variable = _button_container->get_node<Button>(name); \
  if(!variable){ \
    GameUtils::Logger::print_err_static("[CodeContextMenu] Cannot get '" name "' button in Button Container."); \
     \
    _quit_code = ERR_UNCONFIGURED; \
    goto on_error_label; \
  }

  FETCH_BUTTON(_opening_button, BUTTON_OPENING_NODE_NAME)
  FETCH_BUTTON(_closing_button, BUTTON_CLOSING_NODE_NAME)
  FETCH_BUTTON(_running_button, BUTTON_RUNNING_NODE_NAME)
  FETCH_BUTTON(_refresh_button, BUTTON_REFRESH_NODE_NAME)

  _opening_button->connect("pressed", Callable(this, "_opening_button_pressed"));
  _closing_button->connect("pressed", Callable(this, "_closing_button_pressed"));
  _running_button->connect("pressed", Callable(this, "_running_button_pressed"));
  _refresh_button->connect("pressed", Callable(this, "_refresh_button_pressed"));

  _initialized = true;
  return;

  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  return;}
}


void CodeContextMenu::show_button(button_enum button, bool show){
  _iterate_button(button, [](Button* _button, void* data){
    bool* _show = (bool*)data;
    _button->set_visible(*_show);
  }, &show);
}

void CodeContextMenu::disable_button(button_enum button, bool flag){
  _iterate_button(button, [](Button* _button, void* data){
    bool *_flag = (bool*)data;
    _button->set_disabled(*_flag);
  }, &flag);
}


bool CodeContextMenu::is_initialized() const{
  return _initialized;
}


NodePath CodeContextMenu::get_button_container_path() const{
  return _button_container_path;
}

void CodeContextMenu::set_button_container_path(NodePath path){
  _button_container_path = path;
}