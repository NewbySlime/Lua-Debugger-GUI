#include "code_flow_control.h"
#include "error_trigger.h"
#include "logger.h"
#include "strutil.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"

#define BUTTON_RESTART_NODE_NAME "ButtonRestart"
#define BUTTON_RESUME_NODE_NAME "ButtonResume"
#define BUTTON_PAUSE_NODE_NAME "ButtonPause"
#define BUTTON_STEP_IN_NODE_NAME "ButtonStepIn"
#define BUTTON_STEP_OUT_NODE_NAME "ButtonStepOut"
#define BUTTON_STEP_OVER_NODE_NAME "ButtonStepOver"
#define BUTTON_STOP_NODE_NAME "ButtonStop"


using namespace godot;
using namespace lua::debug;


void CodeFlowControl::_bind_methods(){
  ClassDB::bind_method(D_METHOD("get_control_button_container_path"), &CodeFlowControl::get_control_button_container_path);
  ClassDB::bind_method(D_METHOD("set_control_button_container_path", "path"), &CodeFlowControl::set_control_button_container_path);
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "control_button_container_path"), "set_control_button_container_path", "get_control_button_container_path");

  ClassDB::bind_method(D_METHOD("_on_restart_button_pressed"), &CodeFlowControl::_on_restart_button_pressed);
  ClassDB::bind_method(D_METHOD("_on_resume_button_pressed"), &CodeFlowControl::_on_resume_button_pressed);
  ClassDB::bind_method(D_METHOD("_on_pause_button_pressed"), &CodeFlowControl::_on_pause_button_pressed);
  ClassDB::bind_method(D_METHOD("_on_step_in_button_pressed"), &CodeFlowControl::_on_step_in_button_pressed);
  ClassDB::bind_method(D_METHOD("_on_step_out_button_pressed"), &CodeFlowControl::_on_step_out_button_pressed);
  ClassDB::bind_method(D_METHOD("_on_step_over_button_pressed"), &CodeFlowControl::_on_step_over_button_pressed);
  ClassDB::bind_method(D_METHOD("_on_stop_button_pressed"), &CodeFlowControl::_on_stop_button_pressed);

  ClassDB::bind_method(D_METHOD("_lua_on_starting"), &CodeFlowControl::_lua_on_starting);
  ClassDB::bind_method(D_METHOD("_lua_on_stopping"), &CodeFlowControl::_lua_on_stopping);
  ClassDB::bind_method(D_METHOD("_lua_on_pausing"), &CodeFlowControl::_lua_on_pausing);
  ClassDB::bind_method(D_METHOD("_lua_on_resuming"), &CodeFlowControl::_lua_on_resuming);
}


CodeFlowControl::CodeFlowControl(){

}

CodeFlowControl::~CodeFlowControl(){

}


void CodeFlowControl::_on_restart_button_pressed(){
  _program_handle->restart_lua();
  disable_button(be_stop | be_restart, true);
}

void CodeFlowControl::_on_resume_button_pressed(){
  if(_program_handle->is_running())
    _program_handle->resume_lua();
  else{
    _program_handle->set_blocking_on_start(false);
    _check_blocking_on_start();
  }
}

void CodeFlowControl::_on_pause_button_pressed(){
  if(_program_handle->is_running())
    _program_handle->pause_lua();
  else{
    _program_handle->set_blocking_on_start(true);
    _check_blocking_on_start();
  }
}

void CodeFlowControl::_on_step_in_button_pressed(){
  _program_handle->step_lua(I_execution_flow::st_in);
}

void CodeFlowControl::_on_step_out_button_pressed(){
  _program_handle->step_lua(I_execution_flow::st_out);
}

void CodeFlowControl::_on_step_over_button_pressed(){
  _program_handle->step_lua(I_execution_flow::st_over);
}

void CodeFlowControl::_on_stop_button_pressed(){
  _program_handle->stop_lua();
  disable_button(be_stop | be_restart, true);
}


void CodeFlowControl::_lua_on_starting(){
  disable_button(be_allbutton, false);
  show_button(be_allbutton, false);

  disable_button(be_step_in | be_step_out | be_step_over, true);
  show_button(be_pause | be_restart | be_step_in | be_step_out | be_step_over | be_stop, true);
}

void CodeFlowControl::_lua_on_stopping(){
  disable_button(be_allbutton, true);

  _check_blocking_on_start();
}

void CodeFlowControl::_lua_on_pausing(){
  show_button(be_pause, false);
  show_button(be_resume, true);

  disable_button(be_step_in | be_step_out | be_step_over, false);
}

void CodeFlowControl::_lua_on_resuming(){
  show_button(be_pause, true);
  show_button(be_resume, false);

  disable_button(be_step_in | be_step_out | be_step_over, true);
}


void CodeFlowControl::_check_blocking_on_start(){
  disable_button(be_pause | be_resume, false);

  bool _is_blocking = _program_handle->get_blocking_on_start();
  show_button(be_pause, !_is_blocking);
  show_button(be_resume, _is_blocking);
}


void CodeFlowControl::_iterate_buttons(int type, _iterate_button_cb cb, void* data){
  for(int i = be_restart; i <= be_stop; i = i << 1){
    Button* _button = NULL;

    switch(type & i){
      break; case be_restart: _button = _restart_button;
      break; case be_resume: _button = _resume_button;
      break; case be_pause: _button = _pause_button;
      break; case be_step_in: _button = _step_in_button;
      break; case be_step_out: _button = _step_out_button;
      break; case be_step_over: _button = _step_over_button;
      break; case be_stop: _button = _stop_button;
    }

    if(!_button)
      continue;

    cb(_button, data);
  }
}


void CodeFlowControl::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

  _program_handle = get_node<LuaProgramHandle>("/root/GlobalLuaProgramHandle");
  if(!_program_handle){
    GameUtils::Logger::print_err_static("[CodeFlowControl] Cannot get LuaProgramHandle.");

    _quit_code = ERR_UNAVAILABLE;
    goto on_error_label;
  }

  _control_button_container = get_node<Node>(_control_button_container_path);
  if(!_control_button_container){
    GameUtils::Logger::print_err_static("[CodeFlowControl] Cannot get Control Button Container.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

#define FETCH_BUTTON(variable, path) \
  variable = _control_button_container->get_node<Button>(path); \
  if(!variable){ \
    GameUtils::Logger::print_err_static("[CodeFlowControl] Cannot get '" path "' button in Button Container."); \
     \
    _quit_code = ERR_UNCONFIGURED; \
    goto on_error_label; \
  }

  FETCH_BUTTON(_restart_button, BUTTON_RESTART_NODE_NAME)
  FETCH_BUTTON(_resume_button, BUTTON_RESUME_NODE_NAME)
  FETCH_BUTTON(_pause_button, BUTTON_PAUSE_NODE_NAME)
  FETCH_BUTTON(_step_in_button, BUTTON_STEP_IN_NODE_NAME)
  FETCH_BUTTON(_step_out_button, BUTTON_STEP_OUT_NODE_NAME)
  FETCH_BUTTON(_step_over_button, BUTTON_STEP_OVER_NODE_NAME)
  FETCH_BUTTON(_stop_button, BUTTON_STOP_NODE_NAME)

  _program_handle->connect(LuaProgramHandle::s_starting, Callable(this, "_lua_on_starting"));
  _program_handle->connect(LuaProgramHandle::s_stopping, Callable(this, "_lua_on_stopping"));
  _program_handle->connect(LuaProgramHandle::s_pausing, Callable(this, "_lua_on_pausing"));
  _program_handle->connect(LuaProgramHandle::s_resuming, Callable(this, "_lua_on_resuming"));

  _restart_button->connect("pressed", Callable(this, "_on_restart_button_pressed"));
  _resume_button->connect("pressed", Callable(this, "_on_resume_button_pressed"));
  _pause_button->connect("pressed", Callable(this, "_on_pause_button_pressed"));
  _step_in_button->connect("pressed", Callable(this, "_on_step_in_button_pressed"));
  _step_out_button->connect("pressed", Callable(this, "_on_step_out_button_pressed"));
  _step_over_button->connect("pressed", Callable(this, "_on_step_over_button_pressed"));
  _stop_button->connect("pressed", Callable(this, "_on_stop_button_pressed"));

  disable_button(be_allbutton, true);
  _check_blocking_on_start();

  return;


  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  return;}
}


void CodeFlowControl::show_button(int type, bool show){
  _iterate_buttons(type, [](Button* button, void* data){
    bool* _show = (bool*)data;
    button->set_visible(*_show);
  }, &show);
}

void CodeFlowControl::disable_button(int type, bool disable){
  _iterate_buttons(type, [](Button* button, void* data){
    bool* _disable = (bool*)data;
    button->set_disabled(*_disable);
  }, &disable);
}


NodePath CodeFlowControl::get_control_button_container_path() const{
  return _control_button_container_path;
}

void CodeFlowControl::set_control_button_container_path(NodePath path){
  _control_button_container_path = path;
}