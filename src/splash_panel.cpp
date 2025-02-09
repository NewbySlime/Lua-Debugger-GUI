#include "error_trigger.h"
#include "gdutils.h"
#include "logger.h"
#include "splash_panel.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/classes/scene_tree_timer.hpp"

using namespace ErrorTrigger;
using namespace gdutils;
using namespace godot;


static const char* _inactivity_timer_anim_start = "PanOutAnimation";
static const char* _inactivity_timer_anim_reset = "RESET";


void SplashPanel::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_mouse_entered"), &SplashPanel::_mouse_entered);
  ClassDB::bind_method(D_METHOD("_mouse_exited"), &SplashPanel::_mouse_exited);

  ClassDB::bind_method(D_METHOD("_inactivity_timer_timeout"), &SplashPanel::_inactivity_timer_timeout);

  ClassDB::bind_method(D_METHOD("set_inactivity_time", "time"), &SplashPanel::set_inactivity_time);
  ClassDB::bind_method(D_METHOD("get_inactivity_time"), &SplashPanel::get_inactivity_time);

  ClassDB::bind_method(D_METHOD("set_reset_control_node_path", "path"), &SplashPanel::set_reset_control_node_path);
  ClassDB::bind_method(D_METHOD("get_reset_control_node_path"), &SplashPanel::get_reset_control_node_path);

  ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "inactivity_time"), "set_inactivity_time", "get_inactivity_time");
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "reset_control_node_path"), "set_reset_control_node_path", "get_reset_control_node_path");
}


void SplashPanel::_mouse_entered(){
  _mouse_entered_flag = true; 
}

void SplashPanel::_mouse_exited(){
  _mouse_entered_flag = false;
}


void SplashPanel::_inactivity_timer_timeout(){
  _inactivity_timed_out = true;

  if(_anim_player)
    _anim_player->play(_inactivity_timer_anim_start);
}


void SplashPanel::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code = 0;

{ // enclosure for using gotos
  _anim_player = find_any_node<AnimationPlayer>(this);
  if(!_anim_player){
    GameUtils::Logger::print_warn_static("[SplashPanel] Cannot find AnimationPlayer in child.");
  }

  Node* _reset_control_node = get_node<Node>(_reset_control_node_path);
  if(!_reset_control_node){
    GameUtils::Logger::print_err_static("[SplashPanel] Cannot get Reset Control Node.");
    _quit_code = ERR_UNCONFIGURED;
    
    goto skip_to_error;
  }

  if(_anim_player)
    _anim_player->play(_inactivity_timer_anim_reset);

  _reset_control_node->connect("mouse_entered", Callable(this, "_mouse_entered"));
  _reset_control_node->connect("mouse_exited", Callable(this, "_mouse_exited"));

  _inactivity_timer = get_tree()->create_timer(_inactivity_time);
  _inactivity_timer->connect("timeout", Callable(this, "_inactivity_timer_timeout"));
} // enclosure closing

  return;


  skip_to_error:{
    trigger_generic_error_message();
    get_tree()->quit(_quit_code);
  }
}

void SplashPanel::_process(double delta){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  if(!_inactivity_timed_out && _mouse_entered_flag)
    _inactivity_timer->set_time_left(_inactivity_time);
}


void SplashPanel::set_inactivity_time(float time){
  _inactivity_time = time;
}

float SplashPanel::get_inactivity_time() const{
  return _inactivity_time;
}


void SplashPanel::set_reset_control_node_path(NodePath path){
  _reset_control_node_path = path;
}

NodePath SplashPanel::get_reset_control_node_path() const{
  return _reset_control_node_path;
}