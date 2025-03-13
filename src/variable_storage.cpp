#include "defines.h"
#include "error_trigger.h"
#include "gdutils.h"
#include "gdvariant_util.h"
#include "logger.h"
#include "luautil.h"
#include "reference_query_menu.h"
#include "signal_ownership.h"
#include "strutil.h"
#include "variable_storage.h"
#include "variable_watcher.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"

#include "vector"

using namespace gdutils;
using namespace godot;
using namespace lua;
using namespace lua::util;


static const char* placeholder_group_name = "placeholder_node";

static const int alias_hint_max_len = 8;
static const char* text_clipping_placement = "...";



void VariableStorage::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_lua_on_stopping"), &VariableStorage::_lua_on_stopping);

  ClassDB::bind_method(D_METHOD("_reference_query_data", "item_offset", "item_length", "result_data"), &VariableStorage::_reference_query_data);
  ClassDB::bind_method(D_METHOD("_reference_query_item_count"), &VariableStorage::_reference_query_item_count);
  ClassDB::bind_method(D_METHOD("_reference_fetch_value", "item_id"), &VariableStorage::_reference_fetch_value);
  
  ClassDB::bind_method(D_METHOD("set_variable_watcher_path", "path"), &VariableStorage::set_variable_watcher_path);
  ClassDB::bind_method(D_METHOD("get_variable_watcher_path"), &VariableStorage::get_variable_watcher_path);

  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "variable_watcher_path"), "set_variable_watcher_path", "get_variable_watcher_path");
}


VariableStorage::~VariableStorage(){
  _clear_variable_tree();
}


void VariableStorage::_lua_on_stopping(){
  std::vector<uint64_t> _removed_list;

  for(auto _pair: _vartree_map){
    switch(_pair.second->this_value->get_type()){
      break; case I_table_var::get_static_lua_type():{
        I_table_var* _tvar = dynamic_cast<I_table_var*>(_pair.second->this_value);
        if(!_tvar->is_reference())
          break;

        _removed_list.insert(_removed_list.end(), _pair.first);
      }

      break; case I_function_var::get_static_lua_type():{
        I_function_var* _fvar = dynamic_cast<I_function_var*>(_pair.second->this_value);
        if(!_fvar->is_reference())
          break;

        _removed_list.insert(_removed_list.end(), _pair.first);
      }
    }
  }

  for(uint64_t _id: _removed_list){
    auto _iter = _vartree_map.find(_id);
    if(_iter == _vartree_map.end())
      continue;

    _remove_tree_item(_iter->second->this_item);
  }

  // remove any values that has a reference
  for(auto _pair: _vartree_map){
    switch(_pair.second->this_value->get_type()){
      break; case I_table_var::get_static_lua_type():{
        I_table_var* _tvar = dynamic_cast<I_table_var*>(_pair.second->this_value);
        _tvar->remove_reference_values();
      }
    }
  }
}


void VariableStorage::_on_context_menu_change_alias(){
  auto _iter = _vartree_map.find(_last_selected_id);
  if(_iter == _vartree_map.end())
    return;

  TreeItem* _item = _variable_tree->get_selected();
  _variable_setter->set_variable_key(_item->get_tooltip_text(0));

  _variable_setter->set_edit_flag(PopupVariableSetter::edit_add_key_edit);
  SignalOwnership(Signal(_variable_setter, PopupVariableSetter::s_applied), Callable(this, "_on_setter_applied"))
    .replace_ownership();
  
  Vector2 _mouse_pos = get_tree()->get_root()->get_mouse_position();
  _variable_setter->set_position(_mouse_pos);
  _variable_setter->popup();
}

void VariableStorage::_on_context_menu_as_copy(){
  _set_tree_item_value_as_copy(_last_selected_item);
}


void VariableStorage::_update_placeholder_state(){
  if(!_ginvoker)
    return;

  _ginvoker->invoke(placeholder_group_name, "set_visible", _root_item->get_child_count() <= 0);
}


void VariableStorage::_add_custom_context(_variable_tree_item_metadata* metadata, PopupContextMenu::MenuData& data){
  PopupContextMenu::MenuData::Part _tmp_part;

  
  if(metadata->this_value){
    // check if the value is reference or not
{ // enclosure for using gotos
    if(!is_reference_variant(metadata->this_value))
      goto skip_reference_checking;

    if(data.part_list.size() > 0){
        _tmp_part.item_type = PopupContextMenu::MenuData::type_separator;
        _tmp_part.label = "";
        _tmp_part.id = -1;
      data.part_list.insert(data.part_list.end(), _tmp_part);
    }

      _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
      _tmp_part.label = "To Copy";
      _tmp_part.id = context_menu_as_copy;
    data.part_list.insert(data.part_list.end(), _tmp_part);
} // enclosure closing

    skip_reference_checking:{}
  }
}

void VariableStorage::_check_custom_context(int id){
  switch(id){
    break; case context_menu_as_copy:{
      _on_context_menu_as_copy();
    }
  }
}


void VariableStorage::_on_item_created(TreeItem* item){
  _flag_check_placeholder_state = true;
}

void VariableStorage::_on_item_deleting(TreeItem* item){
  _flag_check_placeholder_state = true;
}


void VariableStorage::_get_reference_query_function(ReferenceQueryMenu::ReferenceQueryFunction* func){
  *func = get_reference_query_function();
}


void VariableStorage::_reference_query_data(int item_offset, int item_length, const Variant& result_data){
  ReferenceQueryMenu::ReferenceQueryResult _result = parse_variant_data<ReferenceQueryMenu::ReferenceQueryResult>(result_data);
  _result.query_list->clear();

  for(int i = 0; i < item_length; i++){
    int _idx = i + item_offset;
    if(_idx < 0 || _idx >= _root_item->get_child_count())
      break;

    TreeItem* _child_item = _root_item->get_child(_idx);
    Dictionary _child_data;
      _child_data["content_preview"] = _child_item->get_text(0);

    _result.query_list->operator[](_child_item->get_instance_id()) = _child_data;
  }
}

int VariableStorage::_reference_query_item_count(){
  return _root_item->get_child_count();
}

PackedByteArray VariableStorage::_reference_fetch_value(uint64_t item_id){
  ReferenceQueryMenu::ReferenceFetchValueResult _result;
    _result.value = NULL;

{ // enclosure for using gotos
  auto _iter = _vartree_map.find(item_id);
  if(_iter == _vartree_map.end())
    goto skip_to_return;

  _result.value = cpplua_create_var_copy(_iter->second->this_value);
} // enclosure closing

  skip_to_return:{}
  return convert_to_variant(&_result);
}


void VariableStorage::_update_item_text(TreeItem* item, const I_variant* key, const I_variant* value){
  std::string _prefix_str = "";
  std::string _postfix_str = "";
  switch(value->get_type()){
    break; case string_var::get_static_lua_type():
      _prefix_str = "\"";
      _postfix_str = "\"";
  }

  string_store _key_str; key->to_string(&_key_str);
  string_store _value_str; value->to_string(&_value_str);

  item->set_tooltip_text(0, _key_str.data.c_str());

  if(_key_str.data.size() > alias_hint_max_len){
    _key_str.data = _key_str.data.substr(0, alias_hint_max_len);
    _key_str.data += text_clipping_placement;
  }

  std::string _text_str = format_str("\"%s\": %s%s%s%s",
    _key_str.data.c_str(),
    is_reference_variant(value)? "(Ref) ": "",
    _prefix_str.c_str(),
    _value_str.data.c_str(),
    _postfix_str.c_str()
  );

  item->set_text(0, _text_str.c_str());
}


void VariableStorage::_ready(){
  LuaVariableTree::_ready();

  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

{ // enclosure for using goto
  _lua_lib = get_node<LibLuaHandle>("/root/GlobalLibLuaHandle");
  if(!_lua_lib){
    GameUtils::Logger::print_err_static("[VariableStorage] Cannot get Library Handle for Lua.");

    _quit_code = ERR_UNAVAILABLE;
    goto on_error_label;
  }

  _program_handle = get_node<LuaProgramHandle>("/root/GlobalLuaProgramHandle");
  if(!_program_handle){
    GameUtils::Logger::print_err_static("[VariableStorage] Cannot get Program Handle for Lua.");

    _quit_code = ERR_UNAVAILABLE;
    goto on_error_label;
  }

  _vwatcher = get_node<VariableWatcher>(_vwatcher_path);
  if(!_vwatcher){
    GameUtils::Logger::print_err_static("[VariableStorage] Cannot get VariableWatcher.");
    
    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _ginvoker = find_any_node<GroupInvoker>(this);
  if(!_ginvoker)
    GameUtils::Logger::print_warn_static("[VariableStorage] Cannot get GroupInvoker.");

  NodePath _context_menu_path = _gvariables->get_global_value(GlobalVariables::key_context_menu_path);
  _context_menu = get_node<PopupContextMenu>(_context_menu_path);
  if(!_context_menu)
    return;

  NodePath _variable_setter_path = _gvariables->get_global_value(GlobalVariables::key_popup_variable_setter_path);
  _variable_setter = get_node<PopupVariableSetter>(_variable_setter_path);
  if(!_variable_setter)
    return;

  _bind_object(_vwatcher);

  // create root item with table variant
  string_var _root_key = "Storage Data";
  table_var _root_value;
  _root_item = _create_tree_item(NULL);
  _variable_tree_item_metadata* _root_metadata = _vartree_map[_root_item->get_instance_id()];
    _root_metadata->already_revealed = true;
  _set_tree_item_key(_root_item, &_root_key);
  _set_tree_item_value(_root_item, &_root_value);

  _root_item->set_text(0, "Storage Data");

  _lua_lib_data = _lua_lib->get_library_store();
  
  _program_handle->connect(LuaProgramHandle::s_stopping, Callable(this, "_lua_on_stopping"));
} // enclosure closing

  return;


  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  }
}

void VariableStorage::_process(double delta){
  LuaVariableTree::_process(delta);

  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  if(_flag_check_placeholder_state){
    _flag_check_placeholder_state = false;
    _update_placeholder_state();
  }
}


void VariableStorage::add_to_storage(const I_variant* var, const I_variant* key, uint32_t flags){
  TreeItem* _new_item = _create_tree_item(_root_item);
  _variable_tree_item_metadata* _new_metadata = _vartree_map[_new_item->get_instance_id()];
    _new_metadata->_mflag = metadata_valid_mutable_item;

  _set_tree_item_key(_new_item, key);
  _set_tree_item_value(_new_item, var);

  if(!(flags & sf_store_as_reference)) 
    _set_tree_item_value_as_copy(_new_item);

  _update_tree_item(_new_item, NULL);
}


ReferenceQueryMenu::ReferenceQueryFunction VariableStorage::get_reference_query_function(){
  ReferenceQueryMenu::ReferenceQueryFunction _query_func_data;
    _query_func_data.query_data = Callable(this, "_reference_query_data");
    _query_func_data.query_item_count = Callable(this, "_reference_query_item_count");
    _query_func_data.fetch_value_item = Callable(this, "_reference_fetch_value");

  return _query_func_data;
}


void VariableStorage::set_variable_watcher_path(const NodePath& path){
  _vwatcher_path = path;
}

NodePath VariableStorage::get_variable_watcher_path() const{
  return _vwatcher_path;
}