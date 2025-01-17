#include "focus_area.h"

#include "set"


using namespace godot;


void FocusArea::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_resized"), &FocusArea::_on_resized);
  
  ClassDB::bind_method(D_METHOD("set_ignore_focus", "flag"), &FocusArea::set_ignore_focus);
  ClassDB::bind_method(D_METHOD("get_ignore_focus"), &FocusArea::get_ignore_focus);

  ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ignore_focus"), "set_ignore_focus", "get_ignore_focus");
}


void FocusArea::_on_resized(){
  _reset_config();
}


void FocusArea::_reset_config(){
  set_anchors_and_offsets_preset(LayoutPreset::PRESET_FULL_RECT);
  set_focus_mode(FocusMode::FOCUS_CLICK);
  set_mouse_filter(MouseFilter::MOUSE_FILTER_PASS);
}


void FocusArea::_ready(){
  _reset_config();
  connect("resized", Callable(this, "_on_resized"));
}


bool FocusArea::_set(const StringName& pname, const Variant& value){
  _reset_config();

  auto _iter = _list_disable.find(pname);
  if(_iter != _list_disable.end())
    return true;

  return BaseButton::_set(pname, value);
}

void FocusArea::_validate_property(PropertyInfo& property) const{
  auto _iter = _list_disable.find(property.name);
  if(_iter == _list_disable.end()){
    BaseButton::_validate_property(property);
    return;
  }
  
  property.usage = PROPERTY_USAGE_NONE;
}


void FocusArea::set_ignore_focus(bool flag){
  _ignore_focus = flag;
  set_scale(_ignore_focus? Vector2(0, 0): Vector2(1, 1));
}

bool FocusArea::get_ignore_focus() const{
  return _ignore_focus;
}