#include "chosen_state_modulate.h"
#include "logger.h"
#include "strutil.h"

#include "godot_cpp/classes/canvas_item.hpp"
#include "godot_cpp/classes/engine.hpp"


using namespace godot;


void ChosenStateModulate::_bind_methods(){
  ClassDB::bind_method(D_METHOD("set_chosen", "flag"), &ChosenStateModulate::set_chosen);

  ClassDB::bind_method(D_METHOD("set_chosen_color", "color"), &ChosenStateModulate::set_chosen_color);
  ClassDB::bind_method(D_METHOD("get_chosen_color"), &ChosenStateModulate::get_chosen_color);

  ClassDB::bind_method(D_METHOD("set_unchosen_color", "color"), &ChosenStateModulate::set_unchosen_color);
  ClassDB::bind_method(D_METHOD("get_unchosen_color"), &ChosenStateModulate::get_unchosen_color);

  ClassDB::bind_method(D_METHOD("set_modified_list", "list"), &ChosenStateModulate::set_modified_list);
  ClassDB::bind_method(D_METHOD("get_modified_list"), &ChosenStateModulate::get_modified_list);

  ADD_PROPERTY(PropertyInfo(Variant::COLOR, "chosen_color"), "set_chosen_color", "get_chosen_color");
  ADD_PROPERTY(PropertyInfo(Variant::COLOR, "unchosen_color"), "set_unchosen_color", "get_unchosen_color");
  ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "modified_list"), "set_modified_list", "get_modified_list");
}


void ChosenStateModulate::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  _modulate_cb_list.clear();
  _modulate_cb_list.reserve(_modified_list.size());
  for(int i = 0; i < _modified_list.size(); i++){
    NodePath _path = _modified_list[i];
    CanvasItem* _test_node = get_node<CanvasItem>(_path);
    if(!_test_node){
      GameUtils::Logger::print_warn_static(gd_format_str("[ChosenStateModulate] Node ({0}) is not or does not derived from CanvasItem.", _path));
      continue;
    }

    _modulate_cb_list.insert(_modulate_cb_list.end(), Callable(_test_node, "set_modulate"));
  }
}


void ChosenStateModulate::set_chosen(bool flag){
  Color _col = flag? _chosen_color: _unchosen_color;
  for(Callable& cb: _modulate_cb_list)
    cb.call(_col);
}


void ChosenStateModulate::set_chosen_color(const Color& col){
  _chosen_color = col;
}

Color ChosenStateModulate::get_chosen_color() const{
  return _chosen_color;
}


void ChosenStateModulate::set_unchosen_color(const Color& col){
  _unchosen_color = col;
}

Color ChosenStateModulate::get_unchosen_color() const{
  return _unchosen_color;
}


void ChosenStateModulate::set_modified_list(const Array& list){
  _modified_list = list;
}

Array ChosenStateModulate::get_modified_list() const{
  return _modified_list;
}