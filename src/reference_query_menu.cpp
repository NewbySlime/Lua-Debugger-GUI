#include "error_trigger.h"
#include "gdutils.h"
#include "gdvariant_util.h"
#include "group_invoker.h"
#include "logger.h"
#include "node_utils.h"
#include "reference_query_menu.h"
#include "strutil.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/packed_scene.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/math.hpp"


using namespace ErrorTrigger;
using namespace godot;
using namespace gdutils;


void ReferenceQueryMenu::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_content_button_pressed", "id"), &ReferenceQueryMenu::_on_content_button_pressed);
  ClassDB::bind_method(D_METHOD("_on_button_pressed", "type"), &ReferenceQueryMenu::_on_button_pressed);

  ClassDB::bind_method(D_METHOD("get_label_page_info_path"), &ReferenceQueryMenu::get_label_page_info_path);
  ClassDB::bind_method(D_METHOD("set_label_page_info_path", "path"), &ReferenceQueryMenu::set_label_page_info_path);

  ClassDB::bind_method(D_METHOD("get_first_button_path"), &ReferenceQueryMenu::get_first_button_path);
  ClassDB::bind_method(D_METHOD("set_first_button_path", "path"), &ReferenceQueryMenu::set_first_button_path);

  ClassDB::bind_method(D_METHOD("get_before_button_path"), &ReferenceQueryMenu::get_before_button_path);
  ClassDB::bind_method(D_METHOD("set_before_button_path", "path"), &ReferenceQueryMenu::set_before_button_path);

  ClassDB::bind_method(D_METHOD("get_next_button_path"), &ReferenceQueryMenu::get_next_button_path);
  ClassDB::bind_method(D_METHOD("set_next_button_path", "path"), &ReferenceQueryMenu::set_next_button_path);

  ClassDB::bind_method(D_METHOD("get_last_button_path"), &ReferenceQueryMenu::get_last_button_path);
  ClassDB::bind_method(D_METHOD("set_last_button_path", "path"), &ReferenceQueryMenu::set_last_button_path);

  ClassDB::bind_method(D_METHOD("get_items_per_page"), &ReferenceQueryMenu::get_items_per_page);
  ClassDB::bind_method(D_METHOD("set_items_per_page", "item_count"), &ReferenceQueryMenu::set_items_per_page);
  
  ClassDB::bind_method(D_METHOD("get_content_preview_pivot_path"), &ReferenceQueryMenu::get_content_preview_pivot_path);
  ClassDB::bind_method(D_METHOD("set_content_preview_pivot_path", "path"), &ReferenceQueryMenu::set_content_preview_pivot_path);

  ClassDB::bind_method(D_METHOD("get_content_preview_pckscene"), &ReferenceQueryMenu::get_content_preview_pckscene);
  ClassDB::bind_method(D_METHOD("set_content_preview_pckscene", "scene"), &ReferenceQueryMenu::set_content_preview_pckscene);

  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "label_page_info"), "set_label_page_info_path", "get_label_page_info_path");
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "first_button"), "set_first_button_path", "get_first_button_path");
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "before_button"), "set_before_button_path", "get_before_button_path");
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "next_button"), "set_next_button_path", "get_next_button_path");
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "last_button"), "set_last_button_path", "get_last_button_path");
  ADD_PROPERTY(PropertyInfo(Variant::INT, "items_per_page"), "set_items_per_page", "get_items_per_page");
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "content_preview_pivot"), "set_content_preview_pivot_path", "get_content_preview_pivot_path");
  ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "content_preview_scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_content_preview_pckscene", "get_content_preview_pckscene");
}


void ReferenceQueryMenu::_on_content_button_pressed(uint64_t id){
  choose_reference(id);
}


void ReferenceQueryMenu::_on_button_pressed(int type){
  bool _do_update_ref_page = false;
  switch(type){
    break; case button_type_first:
      _current_page = 0;
      _do_update_ref_page = true;

    break; case button_type_before:
      _current_page--;
      _do_update_ref_page = true;

    break; case button_type_next:
      _current_page++;
      _do_update_ref_page = true;

    break; case button_type_last:
      _current_page = INT32_MAX;
      _do_update_ref_page = true;
  }

  if(_do_update_ref_page)
    _update_reference_page();
}


void ReferenceQueryMenu::_update_reference_page(){
  clear_child(_content_preview_pivot);

  int _item_count = _query_func_data.query_item_count.call();
  int _page_count = Math::clamp((int)ceil((double)_item_count/_items_per_page), 1, INT32_MAX);
  
  _current_page = Math::clamp(_current_page, 0, _page_count-1);

  _label_page_info->set_text(gd_format_str("P {0}", _current_page+1));

  ReferenceQueryResult _result;
    Dictionary _query_list; _result.query_list = &_query_list;
  
  _query_func_data.query_data.call(_current_page * _items_per_page, _items_per_page, convert_to_variant(&_result));

  _content_metadata_map.clear();

  Array _keys_list = _query_list.keys();
  for(int i = 0; i < _keys_list.size(); i++){
    Variant _key = _keys_list[i];
    Dictionary _value = _query_list[_key];
    
    String _content_preview = _value["content_preview"];

    Node* _instantiated_node = _content_preview_pckscene->instantiate();
    _content_preview_pivot->add_child(_instantiated_node);

    GroupInvoker* _ginvoker = get_any_node<GroupInvoker>(_instantiated_node);
    if(!_ginvoker)
      break;

    _ginvoker->invoke("content_text", "set_text", _content_preview);
    _ginvoker->invoke("content_text", "set_tooltip_text", _content_preview);

    _ginvoker->invoke("content_state", "set_chosen", false);

    _ginvoker->invoke("content_button", "connect", "pressed", Callable(this, "_on_content_button_pressed").bind(_key));

    _content_metadata _metadata;
      _metadata.group_invoke = Callable(_ginvoker, "invokev");

    _content_metadata_map[_key] = _metadata;
  }
}


void ReferenceQueryMenu::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

{ // enclosure for using gotos
  _label_page_info = get_node<Label>(_label_page_info_path);
  if(!_label_page_info){
    GameUtils::Logger::print_err_static("[ReferenceQueryMenu] Cannot get label for page info.");
    
    _quit_code = ERR_UNCONFIGURED;
    goto on_error;
  }

  _first_button = get_node<Button>(_first_button_path);
  if(!_first_button){
    GameUtils::Logger::print_err_static("[ReferenceQueryMenu] Cannot get 'first' button.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error;
  }

  _before_button = get_node<Button>(_before_button_path);
  if(!_before_button){
    GameUtils::Logger::print_err_static("[ReferenceQueryMenu] Cannot get 'before' button.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error;
  }

  _next_button = get_node<Button>(_next_button_path);
  if(!_next_button){
    GameUtils::Logger::print_err_static("[ReferenceQueryMenu] Cannot get 'next' button.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error;
  }

  _last_button = get_node<Button>(_last_button_path);
  if(!_last_button){
    GameUtils::Logger::print_err_static("[ReferenceQueryMenu] Cannot get 'last' button.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error;
  }

  _content_preview_pivot = get_node<Node>(_content_preview_pivot_path);
  if(!_content_preview_pivot){
    GameUtils::Logger::print_err_static("[ReferenceQueryMenu] Cannot get pivot for Content Preview.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error;
  } 
   
  // Check content preview packed scene
  if(_content_preview_pckscene.is_valid()){
    Node* _test_node = _content_preview_pckscene->instantiate();
    if(!_test_node){
      GameUtils::Logger::print_err_static("[ReferenceQueryMenu] Content Preview PackedScene is not a valid PackedScene.");

      _quit_code = ERR_UNCONFIGURED;
      goto on_error;
    }

    Node* _group_invoker = get_any_node<GroupInvoker>(_test_node);
    if(!_group_invoker){
      GameUtils::Logger::print_err_static("[ReferenceQueryMenu] Content preview object does not have a GroupInvoker child.");

      _quit_code = ERR_UNCONFIGURED;
      goto on_error;
    }

    _test_node->queue_free();
  }

  _first_button->connect("pressed", Callable(this, "_on_button_pressed").bind(button_type_first));
  _before_button->connect("pressed", Callable(this, "_on_button_pressed").bind(button_type_before));
  _next_button->connect("pressed", Callable(this, "_on_button_pressed").bind(button_type_next));
  _last_button->connect("pressed", Callable(this, "_on_button_pressed").bind(button_type_last));
} // enclosure closing

  return;


  on_error:{
    trigger_generic_error_message();
    get_tree()->quit(_quit_code);
  }
}


void ReferenceQueryMenu::set_page(int page){
  _current_page = page;
  _update_reference_page();
}

int ReferenceQueryMenu::get_page() const{
  return _current_page;
}


void ReferenceQueryMenu::set_reference_query_function_data(const ReferenceQueryFunction& data){
  _query_func_data = data;
}

ReferenceQueryMenu::ReferenceQueryFunction ReferenceQueryMenu::get_reference_query_function_data() const{
  return _query_func_data;
}


void ReferenceQueryMenu::choose_reference(uint64_t content_id){
  // reset all state first
  for(auto _pair: _content_metadata_map)
    _pair.second.group_invoke.call("content_state", "set_chosen", create_array(false));

  _chosen_content_id = 0;

  auto _iter = _content_metadata_map.find(content_id);
  if(_iter == _content_metadata_map.end())
    return;

  _iter->second.group_invoke.call("content_state", "set_chosen", create_array(true));
  _chosen_content_id = content_id;
}

uint64_t ReferenceQueryMenu::get_chosen_reference_id(){
  return _chosen_content_id;
}


void ReferenceQueryMenu::update_reference_page(){
  _update_reference_page();
}


NodePath ReferenceQueryMenu::get_label_page_info_path() const{
  return _label_page_info_path;
}

void ReferenceQueryMenu::set_label_page_info_path(const NodePath& path){
  _label_page_info_path = path;
}


NodePath ReferenceQueryMenu::get_first_button_path() const{
  return _first_button_path;
}

void ReferenceQueryMenu::set_first_button_path(const NodePath& path){
  _first_button_path = path;
}


NodePath ReferenceQueryMenu::get_before_button_path() const{
  return _before_button_path;
}

void ReferenceQueryMenu::set_before_button_path(const NodePath& path){
  _before_button_path = path;
}


NodePath ReferenceQueryMenu::get_next_button_path() const{
  return _next_button_path;
}

void ReferenceQueryMenu::set_next_button_path(const NodePath& path){
  _next_button_path = path;
}


NodePath ReferenceQueryMenu::get_last_button_path() const{
  return _last_button_path;
}

void ReferenceQueryMenu::set_last_button_path(const NodePath& path){
  _last_button_path = path;
}


size_t ReferenceQueryMenu::get_items_per_page() const{
  return _items_per_page;
}

void ReferenceQueryMenu::set_items_per_page(size_t item_count){
  _items_per_page = item_count;
}


NodePath ReferenceQueryMenu::get_content_preview_pivot_path() const{
  return _content_preview_pivot_path;
}

void ReferenceQueryMenu::set_content_preview_pivot_path(const NodePath& path){
  _content_preview_pivot_path = path;
}


Ref<PackedScene> ReferenceQueryMenu::get_content_preview_pckscene() const{
  return _content_preview_pckscene;
}

void ReferenceQueryMenu::set_content_preview_pckscene(Ref<PackedScene> scene){
  _content_preview_pckscene = scene;
}