#include "logger.h"
#include "split_ratio_maintainer.h"
#include "strutil.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/h_split_container.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/classes/v_split_container.hpp"
#include "godot_cpp/classes/window.hpp"


using namespace godot;


void SplitRatioMaintainer::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_split_dragged", "offset", "node"), &SplitRatioMaintainer::_on_split_dragged);
  ClassDB::bind_method(D_METHOD("_on_size_changed", "node"), &SplitRatioMaintainer::_on_size_changed);
  ClassDB::bind_method(D_METHOD("_on_node_deleted", "node"), &SplitRatioMaintainer::_on_node_deleted);

  ClassDB::bind_method(D_METHOD("set_target_object_list", "list"), &SplitRatioMaintainer::set_target_object_list);
  ClassDB::bind_method(D_METHOD("get_target_object_list"), &SplitRatioMaintainer::get_target_object_list);

  ClassDB::bind_method(D_METHOD("set_find_in_children", "check"), &SplitRatioMaintainer::set_find_in_children);
  ClassDB::bind_method(D_METHOD("get_find_in_children"), &SplitRatioMaintainer::get_find_in_children);

  ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "target_object_list"), "set_target_object_list", "get_target_object_list");
  ADD_PROPERTY(PropertyInfo(Variant::BOOL, "find_in_children"), "set_find_in_children", "get_find_in_children");
}


void SplitRatioMaintainer::_on_split_dragged(int offset, Node* node){
  _update_container_ratio(node->get_instance_id());
}

void SplitRatioMaintainer::_on_size_changed(Node* node){
  _resize_container(node->get_instance_id());
}

void SplitRatioMaintainer::_on_node_deleted(Node* node){
  auto _iter = _container_metadata.find(node->get_instance_id());
  if(_iter == _container_metadata.end())
    return;

  _container_metadata.erase(_iter);
}


void SplitRatioMaintainer::_register_object(SplitContainer* node){
  if(!node)
    return;

  auto _check_iter = _container_metadata.find(node->get_instance_id());
  if(_check_iter != _container_metadata.end())
    return;

  split_container_metadata _metadata;
    _metadata.scontainer = node;

  if(node->is_class(HSplitContainer::get_class_static()))
    _metadata.axes = split_container_metadata::split_axes_x;
  else if(node->is_class(VSplitContainer::get_class_static()))
    _metadata.axes = split_container_metadata::split_axes_y;
  else{
    GameUtils::Logger::print_err_static(gd_format_str("[SplitRatioMaintainer] Cannot determine axes for class '{0}'.", node->get_class()));
    return;
  }

  _container_metadata[node->get_instance_id()] = _metadata;
  
  node->connect("resized", Callable(this, "_on_size_changed").bind((Node*)node));
  node->connect("dragged", Callable(this, "_on_split_dragged").bind((Node*)node));
  _update_container_ratio(node->get_instance_id());
}

void SplitRatioMaintainer::_register_children(Node* node){
  for(int i = 0; i < node->get_child_count(); i++){
    Node* _child_node = node->get_child(i);
    if(_child_node->is_class(SplitContainer::get_class_static()))
      _register_object((SplitContainer*)_child_node);

    _register_children(_child_node);
  }
}


void SplitRatioMaintainer::_update_container_ratio(uint64_t id){
  auto _iter = _container_metadata.find(id);
  if(_iter == _container_metadata.end())
    return;

  split_container_metadata* _metadata = &_iter->second;
  if(_metadata->scontainer->get_child_count() < 2)
    return;

  Control* _first_child = NULL;
  Control* _second_child = NULL;
  for(int i = 0; i < _metadata->scontainer->get_child_count(); i++){
    Node* _child_node = _metadata->scontainer->get_child(i);
    if(!_child_node->is_class(Control::get_class_static()))
      continue;

    if(!_first_child)
      _first_child = (Control*)_child_node;
    else if(!_second_child)
      _second_child = (Control*)_child_node;
    else
      break;
  }

  if(!_first_child || !_second_child)
    return;

  Vector2i _parent_size = _metadata->scontainer->get_size();
  Vector2i _first_size = _first_child->get_minimum_size();
  Vector2i _second_size = _second_child->get_minimum_size();
  int _min_offset = 0;
  int _max_offset = 0;
  int _offset_size = 0;
  switch(_iter->second.axes){
    break; case split_container_metadata::split_axes_x:{
      _min_offset = _first_size.x;
      _max_offset = _parent_size.x - _second_size.x;
      _offset_size = _parent_size.x;
    }

    break; case split_container_metadata::split_axes_y:{
      _min_offset = _second_size.y;
      _max_offset = _parent_size.y - _first_size.y;
      _offset_size = _parent_size.y;
    }

    break; default:
      return;
  }

  int _offset = _metadata->scontainer->get_split_offset();
  if(_offset < _min_offset)
    _offset = _min_offset;
  if(_offset > _max_offset)
    _offset = _max_offset;

  _metadata->ratio = (float)_offset / _offset_size;
}

void SplitRatioMaintainer::_resize_container(uint64_t id){
  auto _iter = _container_metadata.find(id);
  if(_iter == _container_metadata.end())
    return;

  Vector2 _container_size = _iter->second.scontainer->get_size();
  int _offset = _iter->second.scontainer->get_split_offset();
  switch(_iter->second.axes){
    break; case split_container_metadata::split_axes_x:{
      _offset = _container_size.x * _iter->second.ratio;
    }

    break; case split_container_metadata::split_axes_y:{
      _offset = _container_size.y * _iter->second.ratio;
    }

    break; default:
      return;
  }

  _iter->second.scontainer->set_split_offset(_offset);
}


void SplitRatioMaintainer::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  SceneTree* _tree = get_tree();
  _tree->connect("node_removed", Callable(this, "_on_node_deleted"));

  if(_find_in_children)
    _register_children(this);

  for(int i = 0; i < _obj_list.size(); i++){
    Variant _var = _obj_list[i];
    if(_var.get_type() != Variant::NODE_PATH)
      continue;

    NodePath _node_path = _var;
    SplitContainer* _node = get_node<SplitContainer>(_node_path);
    _register_object(_node);
  }
}


void SplitRatioMaintainer::set_target_object_list(const Array& list){
  _obj_list = list;

  Engine* _engine = Engine::get_singleton();
  if(!_engine->is_editor_hint())
    return;
  
  for(int i = 0; i < list.size(); i++){
    Variant _arg = list[i];
    if(_arg.get_type() != Variant::NODE_PATH){
      GameUtils::Logger::print_warn_static(gd_format_str("[SplitRatioMaintainer] Data in idx ({0}) of Target Object list is not a type of NODE_PATH.", i));
      continue;
    }

    NodePath _node_path = _arg;
    Node* _node = get_node<Node>(_node_path);
    if(!_node || !_node->is_class(SplitContainer::get_class_static())){
      GameUtils::Logger::print_warn_static(gd_format_str("[SplitRatioMaintainer] Target Object in idx ({0}) of the list is not a type of SplitContainer.", i));
      continue;
    }
  }
}

Array SplitRatioMaintainer::get_target_object_list() const{
  return _obj_list;
}


void SplitRatioMaintainer::set_find_in_children(bool check){
  _find_in_children = check;
}

bool SplitRatioMaintainer::get_find_in_children() const{
  return _find_in_children;
}