#include "slide_animation_control.h"

#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/engine.hpp"

using namespace godot;


void SlideAnimationControl::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_update_size"), &SlideAnimationControl::_on_update_size);

  ClassDB::bind_method(D_METHOD("set_slide_value", "value"), &SlideAnimationControl::set_slide_value);
  ClassDB::bind_method(D_METHOD("get_slide_value"), &SlideAnimationControl::get_slide_value);

  ClassDB::bind_method(D_METHOD("set_target_node", "target_node"), &SlideAnimationControl::set_target_node);
  ClassDB::bind_method(D_METHOD("get_target_node"), &SlideAnimationControl::get_target_node);

  ClassDB::bind_method(D_METHOD("set_slide_direction", "direction"), &SlideAnimationControl::set_slide_direction);
  ClassDB::bind_method(D_METHOD("get_slide_direction"), &SlideAnimationControl::get_slide_direction);

  ClassDB::bind_method(D_METHOD("set_slide_pivot", "pivot"), &SlideAnimationControl::set_slide_pivot);
  ClassDB::bind_method(D_METHOD("get_slide_pivot"), &SlideAnimationControl::get_slide_pivot);

  ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "slide_value"), "set_slide_value", "get_slide_value");
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target_node"), "set_target_node", "get_target_node");
  ADD_PROPERTY(PropertyInfo(Variant::INT, "slide_direction", PropertyHint::PROPERTY_HINT_ENUM, "Up,Down,Right,Left"), "set_slide_direction", "get_slide_direction");
  ADD_PROPERTY(PropertyInfo(Variant::INT, "slide_pivot", PropertyHint::PROPERTY_HINT_ENUM, "Top,TopLeft,Left,BottomLeft,Bottom,BottomRight,Right,TopRight"), "set_slide_pivot", "get_slide_pivot");
}


void SlideAnimationControl::_on_update_size(){
  _update_node();
}

void SlideAnimationControl::_update_node(){
  Node* _tnode = get_node<Node>(_target_node);
  if(!_tnode || !_tnode->is_class(Control::get_class_static()))
    return;

  Control* _cnode = (Control*)_tnode;
  Vector2 _size = _cnode->get_size();
  Vector2 _end_pos = Vector2(0, 0);
  Vector2 _begin_pos = Vector2(0, 0);
  Vector2 _offset = Vector2(0, 0);
  switch(_direction){
    break; case slide_direction_up:
      _end_pos.y = -_size.y;

    break; case slide_direction_down:
      _begin_pos.y = -_size.y;

    break; case slide_direction_right:
      _begin_pos.x = -_size.x;

    break; case slide_direction_left:
      _end_pos.x = -_size.x; 
  }

  Rect2 _r2_cnode = _cnode->get_rect();
  switch(_pivot){
    break; case slide_pivot_top:
      _offset.x = -_r2_cnode.size.x/2;
    
    break; case slide_pivot_top_left:
      // nothing to do here

    break; case slide_pivot_left:
      _offset.y = -_r2_cnode.size.y/2;

    break; case slide_pivot_bottom_left:
      _offset.y = -_r2_cnode.size.y;

    break; case slide_pivot_bottom:
      _offset.x = -_r2_cnode.size.x/2;
      _offset.y = -_r2_cnode.size.y;

    break; case slide_pivot_bottom_right:
      _offset.x = -_r2_cnode.size.x;
      _offset.y = -_r2_cnode.size.y;
      
    break; case slide_pivot_right:
      _offset.x = -_r2_cnode.size.x;
      _offset.y = -_r2_cnode.size.y/2;

    break; case slide_pivot_top_right:
      _offset.x = -_r2_cnode.size.x;
  }

  _begin_pos += _offset;
  _end_pos += _offset;

  Vector2 _new_pos = _begin_pos.lerp(_end_pos, _slide_value);
  _cnode->set_position(_new_pos);
}


void SlideAnimationControl::_try_bind_node(){
  Node* _tnode = get_node<Node>(_target_node);
  if(!_tnode || !_tnode->is_class(Control::get_class_static()))
    return;

  _tnode->connect("resized", Callable(this, "_on_update_size"));
}

void SlideAnimationControl::_try_unbind_node(){
  Node* _tnode = get_node<Node>(_target_node);
  if(!_tnode || !_tnode->is_class(Control::get_class_static()))
    return;

  _tnode->disconnect("resized", Callable(this, "_on_update_size"));
}


void SlideAnimationControl::_ready(){
  _slide_value = 0;

  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  _try_bind_node();
  _update_node();
}


void SlideAnimationControl::set_slide_value(float value){
  _slide_value = value;
  _update_node();
}

float SlideAnimationControl::get_slide_value() const{
  return _slide_value;
}


void SlideAnimationControl::set_target_node(const NodePath& target_path){
  Engine* _engine = Engine::get_singleton();
  if(!_engine->is_editor_hint()){
    _target_node = target_path;
    return;
  }

  _try_unbind_node();
  _target_node = target_path;
  _try_bind_node();

  _update_node();
}

NodePath SlideAnimationControl::get_target_node() const{
  return _target_node;
}


void SlideAnimationControl::set_slide_direction(int dir){
  _direction = dir;

  Engine* _engine = Engine::get_singleton();
  if(!_engine->is_editor_hint())
    return;
    
  _update_node();
}

int SlideAnimationControl::get_slide_direction() const{
  return _direction;
}


void SlideAnimationControl::set_slide_pivot(int pivot){
  _pivot = pivot;

  Engine* _engine = Engine::get_singleton();
  if(!_engine->is_editor_hint())
    return;

  _update_node();
}

int SlideAnimationControl::get_slide_pivot() const{
  return _pivot;
}