#include "custom_variant.h"
#include "error_trigger.h"
#include "defines.h"
#include "gdutils.h"
#include "gdvariant_util.h"
#include "logger.h"
#include "luautil.h"
#include "luavariable_tree.h"
#include "signal_ownership.h"
#include "strutil.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"

#include "Lua-CPPAPI/Src/luaapi_core.h"
#include "Lua-CPPAPI/Src/luadebug_variable_watcher.h"

#include "algorithm"


using namespace gdutils;
using namespace godot;
using namespace lua;
using namespace lua::api;
using namespace lua::debug;
using namespace lua::util;



const char* LuaVariableTree::s_on_reference_changed = "reference_changed";


struct _setter_usage_pass_data{
  uint64_t parent_id;
};


void LuaVariableTree::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_item_collapsed_safe", "item"), &LuaVariableTree::_item_collapsed_safe);
  ClassDB::bind_method(D_METHOD("_item_collapsed", "item"), &LuaVariableTree::_item_collapsed);
  ClassDB::bind_method(D_METHOD("_item_selected"), &LuaVariableTree::_item_selected);
  ClassDB::bind_method(D_METHOD("_item_nothing_selected"), &LuaVariableTree::_item_nothing_selected);
  ClassDB::bind_method(D_METHOD("_item_selected_mouse", "mouse_pos", "mouse_button_index"), &LuaVariableTree::_item_selected_mouse);
  ClassDB::bind_method(D_METHOD("_item_empty_clicked", "mouse_pos", "mouse_button_index"), &LuaVariableTree::_item_empty_clicked);
  ClassDB::bind_method(D_METHOD("_item_activated"), &LuaVariableTree::_item_activated);

  ClassDB::bind_method(D_METHOD("_on_setter_applied", "pass_data"), &LuaVariableTree::_on_setter_applied);
  ClassDB::bind_method(D_METHOD("_on_setter_cancelled"), &LuaVariableTree::_on_setter_cancelled);
  ClassDB::bind_method(D_METHOD("_on_setter_applied_add_table_confirmed_variant"), &LuaVariableTree::_on_setter_applied_add_table_confirmed_variant);
  ClassDB::bind_method(D_METHOD("_on_setter_applied_add_table_cancelled_variant"), &LuaVariableTree::_on_setter_applied_add_table_cancelled_variant);
  ClassDB::bind_method(D_METHOD("_on_tree_button_clicked", "tree", "column", "id", "mouse_btn"), &LuaVariableTree::_on_tree_button_clicked);
  ClassDB::bind_method(D_METHOD("_on_reference_data_changed", "data"), &LuaVariableTree::_on_reference_data_changed);
  ClassDB::bind_method(D_METHOD("_on_context_menu_clicked", "id"), &LuaVariableTree::_on_context_menu_clicked);

  ClassDB::bind_method(D_METHOD("get_variable_tree_path"), &LuaVariableTree::get_variable_tree_path);
  ClassDB::bind_method(D_METHOD("set_variable_tree_path", "path"), &LuaVariableTree::set_variable_tree_path);

  ClassDB::bind_method(D_METHOD("get_context_menu_button_texture"), &LuaVariableTree::get_context_menu_button_texture);
  ClassDB::bind_method(D_METHOD("set_context_menu_button_texture", "texture"), &LuaVariableTree::set_context_menu_button_texture);

  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "variable_tree_path"), "set_variable_tree_path", "get_variable_tree_path");
  ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "context_menu_button_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), "set_context_menu_button_texture", "get_context_menu_button_texture");

  ADD_SIGNAL(MethodInfo(s_on_reference_changed, PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
}


LuaVariableTree::~LuaVariableTree(){
  // signaling to some functions the TreeItems cannot be used
  _variable_tree = NULL;

  _clear_variable_tree();
  _clear_reference_lookup_list();
}


void LuaVariableTree::_item_collapsed_safe(TreeItem* item){
  _update_callable_list.insert(_update_callable_list.end(), Callable(this, "_item_collapsed").bind(item));
}

void LuaVariableTree::_item_collapsed(TreeItem* item){
  if(item->is_collapsed())
    return;

  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end() || _iter->second->already_revealed)
    return;

  if(_iter->second->this_value->get_type() != I_table_var::get_static_lua_type())
    return;

  _reveal_tree_item(item, NULL);
}

void LuaVariableTree::_item_selected(){
  _last_selected_item = _variable_tree->get_selected();
  _last_selected_id = _last_selected_item->get_instance_id();
}

void LuaVariableTree::_item_nothing_selected(){
  _last_selected_item = NULL;
  _last_selected_id = 0;
}

void LuaVariableTree::_item_selected_mouse(const Vector2 mouse_pos, int mouse_idx){
  TreeItem* _item = _variable_tree->get_selected();
  _last_selected_item = _item;
  _last_selected_id = _item->get_instance_id();

  switch(mouse_idx){
    break; case MOUSE_BUTTON_RIGHT:{
      auto _iter = _vartree_map.find(_item->get_instance_id());
      if(_iter == _vartree_map.end())
        break;
        
      _open_context_menu();
    }
  }
}

void LuaVariableTree::_item_empty_clicked(const Vector2 mouse_pos, int mouse_idx){
  _last_selected_item = NULL;
  _last_selected_id = 0;
}

void LuaVariableTree::_item_activated(){
  TreeItem* _item = _variable_tree->get_selected();
  if(!_item)
    return;

  TreeItem* _parent_item = _item->get_parent();
  if(!_parent_item)
    return;

  _last_selected_item = _item;
  _last_selected_id = _item->get_instance_id();

  _last_context_id = context_menu_edit;

  _variable_setter_do_popup_add_table_item(_parent_item, _item);
}


void LuaVariableTree::_on_setter_cancelled(){
  _is_using_variable_setter = false;
  _on_variable_setter_closing();
}

void LuaVariableTree::_on_setter_applied(const Variant& pass_data){
  if(!_is_using_variable_setter)
    return;

  _setter_usage_pass_data _usage_data = parse_variant_data<_setter_usage_pass_data>(pass_data);
  
  auto _iter = _vartree_map.find(_usage_data.parent_id);
  if(_iter == _vartree_map.end())
    return;

  // this should be a copy of the var
  I_variant* _key_var = NULL;
  // this should be a copy of the var
  I_variant* _value_var = NULL;

{ // enclosure for using goto
  _get_data_from_variable_setter(_iter->second, &_key_var, &_value_var);

  if(!_key_var || !_value_var)
    goto skip_to_cleaning;

  // invoke based on context
  switch(_last_context_id){
    break;
    case context_menu_add:
    case context_menu_copy:
    case context_menu_add_table:
      _on_setter_applied_add_table(_iter->second->this_item, _key_var, _value_var);

    break; case context_menu_edit:
      _on_setter_applied_edit(_last_selected_item, _value_var);
  }
} // enclosure closing

  skip_to_cleaning:{}

  if(_key_var)
    cpplua_delete_variant(_key_var);

  if(_value_var)
    cpplua_delete_variant(_value_var);

  _is_using_variable_setter = false;
  _on_variable_setter_closing();
}


struct _setter_applied_add_table_data{
  uint64_t item_id;
  I_variant* key_var;
  I_variant* value_var;
};

void LuaVariableTree::_on_setter_applied_add_table(TreeItem* parent_item, I_variant* key_var, I_variant* value_var){
  auto _iter = _vartree_map.find(parent_item->get_instance_id());
  if(_iter == _vartree_map.end() || !_iter->second->this_value->is_type(I_table_var::get_static_lua_type()))
    return;

  I_table_var* _tvar = dynamic_cast<I_table_var*>(_iter->second->this_value);
  I_variant* _value_test = _tvar->get_value(key_var);
  
  // value already exists
  if(_value_test && _value_test->get_type() != I_nil_var::get_static_lua_type()){
    _setter_applied_add_table_data _data;
      _data.item_id = parent_item->get_instance_id();
      _data.key_var = cpplua_create_var_copy(key_var);
      _data.value_var = cpplua_create_var_copy(value_var);

    Variant _param = convert_to_variant(&_data);
    SignalOwnership(Signal(_global_confirmation_dialog, "confirmed"), Callable(this, "_on_setter_applied_add_table_confirmed_variant").bind(_param))
      .replace_ownership();
    SignalOwnership(Signal(_global_confirmation_dialog, "canceled"), Callable(this, "_on_setter_applied_add_table_cancelled_variant").bind(_param))
      .replace_ownership();
    
    _global_confirmation_dialog->set_text("Another value with same key already exists. Do you wish to override?");
    _global_confirmation_dialog->set_title("Confirmation");
    _global_confirmation_dialog->popup_centered();
  }
  else
    _on_setter_applied_add_table_confirmed(_iter->second, key_var, value_var);  
}

void LuaVariableTree::_on_setter_applied_add_table_confirmed_variant(const Variant& data){
  _setter_applied_add_table_data _data = parse_variant_data<_setter_applied_add_table_data>(data);
  auto _iter = _vartree_map.find(_data.item_id);
  if(_iter == _vartree_map.end())
  return;
  
  _on_setter_applied_add_table_confirmed(_iter->second, _data.key_var, _data.value_var);
  
  // delete _data values
  cpplua_delete_variant(_data.key_var);
  cpplua_delete_variant(_data.value_var);
}

void LuaVariableTree::_on_setter_applied_add_table_cancelled_variant(const Variant& data){
  // delete values
  _setter_applied_add_table_data _data = parse_variant_data<_setter_applied_add_table_data>(data);
  cpplua_delete_variant(_data.key_var);
  cpplua_delete_variant(_data.value_var);
}

void LuaVariableTree::_on_setter_applied_add_table_confirmed(_variable_tree_item_metadata* metadata, I_variant* key_var, I_variant* value_var){
  I_table_var* _tvar = dynamic_cast<I_table_var*>(metadata->this_value);

  // reveal the table
  if(!metadata->already_revealed){
    // set the value first
    _tvar->set_value(key_var, value_var);
    _reveal_tree_item(metadata->this_item, NULL);
  }
  // else, add new treeitem
  else{
    TreeItem* _child_item = NULL;

    if(metadata->child_lookup_list){
{ // enclosure for using gotos
      auto _child_lookup_iter = metadata->child_lookup_list->find(key_var);
      if(_child_lookup_iter == metadata->child_lookup_list->end())
        goto skip_child_fetching;

      auto _child_iter = _vartree_map.find(_child_lookup_iter->second);
      if(_child_iter == _vartree_map.end())
        goto skip_child_fetching;

      _child_item = _child_iter->second->this_item;
} // enclosure closing

      skip_child_fetching:{}
    }

    // create new item if child not found
    if(!_child_item){
      _child_item = _create_tree_item(metadata->this_item);
      _variable_tree_item_metadata* _metadata = _vartree_map[_child_item->get_instance_id()];
      _metadata->_mflag = metadata_valid_mutable_item;
    }
    
    // will handle adding the value to the table
    _set_tree_item_key(_child_item, key_var);
    _set_tree_item_value(_child_item, value_var);
  
    _update_tree_item(_child_item, NULL);

    _notify_changed_tree_item(_child_item->get_parent());
  }
}


void LuaVariableTree::_on_setter_applied_edit(TreeItem* current_item, I_variant* value_var){
  // will handle adding the value to the table
  _set_tree_item_value(current_item, value_var);

  _update_tree_item(current_item, NULL);

  _notify_changed_tree_item(current_item->get_parent());
}


void LuaVariableTree::_on_tree_button_clicked(TreeItem* item, int column, int id, int mouse_button){
  _last_selected_item = item;
  _last_selected_id = item->get_instance_id();
  switch(id){
    break; case button_id_context_menu:
      _open_context_menu();
  }
}

void LuaVariableTree::_on_context_menu_clicked(int id){
  _last_context_id = id;
  switch(id){
    break; case context_menu_edit:{
      TreeItem* _parent_item = _last_selected_item->get_parent();
      if(!_parent_item)
        break;

      _variable_setter_do_popup_add_table_item(_parent_item, _last_selected_item);
    }

    break;
    case context_menu_add:
    case context_menu_copy:{
      TreeItem* _parent_item = _last_selected_item->get_parent();
      if(!_parent_item)
        break;

      uint64_t _edit_flag = PopupVariableSetter::edit_add_value_edit | PopupVariableSetter::edit_add_key_edit;
      _edit_flag |= id == context_menu_add? PopupVariableSetter::edit_clear_on_popup: PopupVariableSetter::edit_flag_none;

      _variable_setter_do_popup_add_table_item(_parent_item, _last_selected_item, _edit_flag);
    }

    break; case context_menu_add_table:{
      uint64_t _edit_flag = 
        PopupVariableSetter::edit_add_value_edit | PopupVariableSetter::edit_add_key_edit | PopupVariableSetter::edit_clear_on_popup;

      _variable_setter_do_popup_add_table_item(_last_selected_item, _edit_flag);
    }

    break; case context_menu_remove:
      _remove_tree_item(_last_selected_item);
      _last_selected_item = NULL;
      _last_selected_id = 0;

    break; default:
      this->_check_custom_context(id);
  }
}


void LuaVariableTree::_add_reference_lookup_value(TreeItem* item, const I_variant* value){
  if(!is_reference_variant(value))
    return;

  _reference_lookup_data* _data;
  auto _iter = _reference_lookup_list.find((uint64_t)get_reference_pointer(value));
  if(_iter != _reference_lookup_list.end())
    _data = _iter->second;
  else{
    _data = new _reference_lookup_data();
    _reference_lookup_list[(uint64_t)get_reference_pointer(value)] = _data;
  }

  _data->item_list.insert(item->get_instance_id());
}

void LuaVariableTree::_delete_reference_lookup_value(TreeItem* item, const I_variant* value){
  if(!is_reference_variant(value))
    return;

  auto _iter = _reference_lookup_list.find((uint64_t)get_reference_pointer(value));
  if(_iter == _reference_lookup_list.end())
    return;
  
  _iter->second->item_list.erase(item->get_instance_id());
  if(_iter->second->item_list.size() <= 0){
    delete _iter->second;
    _reference_lookup_list.erase(_iter);
  }
}

void LuaVariableTree::_clear_reference_lookup_list(){
  for(auto _pair: _reference_lookup_list)
    delete _pair.second;

  _reference_lookup_list.clear();
}


void LuaVariableTree::_update_tree_item(TreeItem* item, _item_state* state){
  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end())
    return;

  _delete_tree_item_child(item);

  _update_item_text(item, _iter->second->this_key, _iter->second->this_value);
  
  if(_iter->second->this_value->is_type(I_table_var::get_static_lua_type())){
    I_table_var* _tvar = dynamic_cast<I_table_var*>(_iter->second->this_value);

    bool _reveal_item = false;
    if(state)
      _reveal_item = state->is_revealed;
    else
      _reveal_item = _iter->second->already_revealed;

    if(_reveal_item)
      _reveal_tree_item(item, state);
    else if(_tvar->get_size() > 0){
      _variable_tree->create_item(item);
      item->set_collapsed(true);
    }
  }

  _sort_item_child(item->get_parent());
}

void LuaVariableTree::_reveal_tree_item(TreeItem* item, _item_state* state){
  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end())
    return;

  if(!_iter->second->this_value->is_type(I_table_var::get_static_lua_type()))
    return;

  bool _is_local_item = _iter->second->this_value->get_type() == I_local_table_var::get_static_lua_type();
  
  _delete_tree_item_child(item);
  _iter->second->already_revealed = true;

  I_table_var* _tvar = dynamic_cast<I_table_var*>(_iter->second->this_value);  
  _tvar->update_keys();
  const I_variant** _keys_list = _tvar->get_keys();

  // query the data first, _set_tree_item will mutate the parent table values
  struct _query_key_value{
    I_variant* key;
  };

  std::vector<_query_key_value> _queried_list;
  for(int i = 0; _keys_list[i]; i++){
    const I_variant* _key_data = _keys_list[i];
    auto _filter_iter = _filter_key.find(_key_data);
    if(_filter_iter != _filter_key.end())
      continue;
    
    if(this->_check_ignored_variable(_iter->second, _key_data))
      continue;

    I_variant* _value_data = _tvar->get_value(_key_data);

    _query_key_value _query;
      _query.key = cpplua_create_var_copy(_key_data);

    _queried_list.insert(_queried_list.end(), _query);
  }
  
  for(_query_key_value& _query: _queried_list){
{ // enclosure for using gotos
    TreeItem* _child_item = _create_tree_item(item);
    _variable_tree_item_metadata* _metadata = _vartree_map[_child_item->get_instance_id()];
      _metadata->_mflag |= metadata_valid_mutable_item;
      _metadata->_mflag |= _is_local_item? metadata_local_item: 0;
    
    _set_tree_item_from_parent(_child_item, _query.key);

    // find the state of the child
    _item_state* _child_state = NULL;
    if(state){
      auto _state_iter = state->branches.find(_query.key);
      _child_state = _state_iter != state->branches.end()? _state_iter->second: NULL;
    }
    
    _update_tree_item(_child_item, _child_state);
} // enclosure closing

    skip_adding_child:{}
    
    // free data in query data
    cpplua_delete_variant(_query.key);
  }

  _queried_list.clear();

  item->set_collapsed(false);
}

void LuaVariableTree::_sort_item_child(TreeItem* parent_item){
  if(parent_item->get_child_count() <= 1)
    return;

  std::vector<TreeItem*> _item_list;
  _item_list.resize(parent_item->get_child_count());
  for(int i = 0; i < parent_item->get_child_count(); i++)
    _item_list[i] = parent_item->get_child(i);


  std::sort(_item_list.begin(), _item_list.end(), [](TreeItem* lh, TreeItem* rh){
    return lh->get_text(0) < rh->get_text(0);
  });

  // length should be more than 1
  for(int i = _item_list.size()-1; i >= 1; i--)
    _item_list[i]->move_after(_item_list[0]);
}


void LuaVariableTree::_store_item_state(TreeItem* parent_item, _item_state* parent_state){
  for(int i = 0; i < parent_item->get_child_count(); i++){
    TreeItem* _child_item = parent_item->get_child(i);
    auto _child_iter = _vartree_map.find(_child_item->get_instance_id());
    if(_child_iter == _vartree_map.end() || !_child_iter->second->this_key)
      continue;

    _item_state* _new_state = new _item_state();
      _new_state->is_revealed = !_child_item->is_collapsed();

    _store_item_state(_child_item, _new_state);

    parent_state->branches[_child_iter->second->this_key] = _new_state;
  }
}


void LuaVariableTree::_set_tree_item_key(TreeItem* item, const I_variant* key){
  _set_tree_item_key_direct(item, key? cpplua_create_var_copy(key): NULL);
}

void LuaVariableTree::_set_tree_item_key_direct(TreeItem* item, I_variant* key){
{ // enclosure for using gotos
  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end())
    goto on_error_label;
  
  // put before replacing key value, the previous value is needed
  TreeItem* _parent_item = item->get_parent();
  if(_parent_item){
{ // enclosure for using gotos
    auto _parent_iter = _vartree_map.find(_parent_item->get_instance_id());
    if(_parent_iter == _vartree_map.end())
      goto skip_parent_checking;
      
    // guaranteed child_lookup_list if this_value is table value
    if(!_parent_iter->second->this_value || !_parent_iter->second->this_value->is_type(I_table_var::get_static_lua_type()))
      goto skip_parent_checking;

    I_table_var* _tvar = dynamic_cast<I_table_var*>(_parent_iter->second->this_value);

    // remove previous key
    if(_iter->second->this_key){
      nil_var _nvar;
      _tvar->set_value(_iter->second->this_key, &_nvar);
      _parent_iter->second->child_lookup_list->erase(_iter->second->this_key);

      // value should use the value that has been set to a table
      I_variant* _set_result = _tvar->get_value(_iter->second->this_key);
      if(_iter->second->this_value)
        cpplua_delete_variant(_iter->second->this_value);

      if(_set_result)
        _iter->second->this_value = _set_result; 
    }

    // add new key
    if(key){
      _parent_iter->second->child_lookup_list->operator[](key) = item->get_instance_id();

      // add new key with previous value
      if(_iter->second->this_value)
        _tvar->set_value(key, _iter->second->this_value);
    }
} // enclosure closing

    skip_parent_checking:{}
  }

  if(_iter->second->this_key){
    cpplua_delete_variant(_iter->second->this_key);
    _iter->second->this_key = NULL;
  }

  if(key)
    _iter->second->this_key = key;

} // enclosure closing
  return;
  
  
  on_error_label:{}

  if(key)
    cpplua_delete_variant(key);
}


void LuaVariableTree::_set_tree_item_value(TreeItem* item, const I_variant* value){
  _set_tree_item_value_direct(item, value? cpplua_create_var_copy(value): NULL);
}

void LuaVariableTree::_set_tree_item_value_direct(TreeItem* item, I_variant* value){
{ // enclosure for using gotos
  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end())
    goto on_error_label;

  TreeItem* _parent_item = item->get_parent();
  if(_parent_item){
{ // enclosure for using gotos
    auto _parent_iter = _vartree_map.find(_parent_item->get_instance_id());
    if(_parent_iter == _vartree_map.end())
      goto skip_parent_checking;

    if(!_parent_iter->second->this_value || !_parent_iter->second->this_value->is_type(I_table_var::get_static_lua_type()))
      goto skip_parent_checking;

    I_table_var* _tvar = dynamic_cast<I_table_var*>(_parent_iter->second->this_value);
{ // enclosure for using gotos
    if(!_iter->second->this_key)
      goto skip_value_set;

    // set value to the parent table, if value is NULL, set to nil (delete)
    nil_var _nvar;
    const I_variant* _set_value = value? value: &_nvar;
    _tvar->set_value(_iter->second->this_key, _set_value);

    // value should use the value that has been set to a table
    I_variant* _set_result = _tvar->get_value(_iter->second->this_key);
    if(_set_result)
      value = _set_result;
} // enclosure closing
    skip_value_set:{}
} // enclosure closing
    skip_parent_checking:{}
  }

  if(_iter->second->this_value){
    _delete_reference_lookup_value(item, _iter->second->this_value);
    cpplua_delete_variant(_iter->second->this_value);
    _iter->second->this_value = NULL;
  }

  if(value && value->is_type(I_table_var::get_static_lua_type())){
    // create child_lookup_list, if exists, clear the list
    if(_iter->second->child_lookup_list)
      _iter->second->child_lookup_list->clear();
    else
      _iter->second->child_lookup_list = new std::map<comparison_variant, uint64_t>();
  }
  else{
    // delete child_lookup_list
    if(_iter->second->child_lookup_list){
      delete _iter->second->child_lookup_list;
      _iter->second->child_lookup_list = NULL;
    }
  }
  
  if(value){
    _add_reference_lookup_value(item, value);
    _iter->second->this_value = value;
  }
} // enclosure closing
  return;

  on_error_label:{}

  if(value)
    cpplua_delete_variant(value);
}

void LuaVariableTree::_set_tree_item_value_as_copy(TreeItem* item){
  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end())
    return;

  if(!is_reference_variant(_iter->second->this_value))
    return;

  _delete_reference_lookup_value(item, _iter->second->this_value);
  switch(_iter->second->this_value->get_type()){
    break; case I_table_var::get_static_lua_type():{
      I_table_var* _tvar = dynamic_cast<I_table_var*>(_iter->second->this_value);
      _tvar->as_copy();
    }

    break; case I_function_var::get_static_lua_type():{
      I_function_var* _fvar = dynamic_cast<I_function_var*>(_iter->second->this_value);
      _fvar->as_copy();
    }
  }

  _update_tree_item(_iter->second->this_item, NULL);
}

void LuaVariableTree::_set_tree_item_from_parent(godot::TreeItem* child_item, const lua::I_variant* key){
  TreeItem* _parent_item = child_item->get_parent();
  auto _iter = _vartree_map.find(child_item->get_instance_id());
  if(_iter == _vartree_map.end())
    return;

  auto _parent_iter = _vartree_map.find(_parent_item->get_instance_id());
  if(_parent_iter == _vartree_map.end())
    return;

  if(!_parent_iter->second->this_value->is_type(I_table_var::get_static_lua_type()))
    return;

  I_table_var* _tvar = dynamic_cast<I_table_var*>(_parent_iter->second->this_value);
  I_variant* _value = _tvar->get_value(key);
  _set_tree_item_key(child_item, key);
  _set_tree_item_value_direct(child_item, _value);
}

void LuaVariableTree::_remove_tree_item(TreeItem* item){
  TreeItem* _parent_item = item->get_parent();

  _set_tree_item_key(item, NULL);
  _set_tree_item_value(item, NULL);
  
  // don't need to delete child using remove, at least current item is removed from the parent item.
  _delete_tree_item(item);

  _notify_changed_tree_item(_parent_item);
}


TreeItem* LuaVariableTree::_create_tree_item(TreeItem* parent_item){
  TreeItem* _result = _variable_tree->create_item(parent_item);
  _vartree_map[_result->get_instance_id()] = _create_vartree_metadata(_result);

  _result->add_button(0, _context_menu_button_texture, button_id_context_menu);

  this->_on_item_created(_result);
  return _result;
}

LuaVariableTree::_variable_tree_item_metadata* LuaVariableTree::_create_vartree_metadata(TreeItem* item){
  _variable_tree_item_metadata* _result = new _variable_tree_item_metadata();
  _result->this_item = item;
  _result->this_key = NULL;
  _result->this_value = NULL;

  return _result;
}


void LuaVariableTree::_clear_item_state(_item_state* node){
  for(auto _pair: node->branches)
    _delete_item_state(_pair.second);

  node->branches.clear();
}

void LuaVariableTree::_delete_item_state(_item_state* node){
  _clear_item_state(node);
  delete node;
}


void LuaVariableTree::_clear_variable_tree(){
  _clear_variable_metadata_map();
  
  if(_variable_tree)
    _variable_tree->clear();
}

void LuaVariableTree::_clear_variable_metadata_map(){
  for(auto _pair: _vartree_map)
    _delete_variable_metadata(_pair.second);

  _vartree_map.clear();
}

void LuaVariableTree::_clear_variable_metadata(_variable_tree_item_metadata* metadata){
  if(_variable_tree){
    if(metadata->this_value)
      _delete_reference_lookup_value(metadata->this_item, metadata->this_value);

{ // enclosure for using gotos
    // might already handled in _remove_tree_item
    if(!metadata->this_key)
      goto skip_remove_child_lookup_table;

    TreeItem* _parent_item = metadata->this_item->get_parent();
    if(!_parent_item)
      goto skip_remove_child_lookup_table;

    auto _parent_iter = _vartree_map.find(_parent_item->get_instance_id());
    if(_parent_iter == _vartree_map.end() || !_parent_iter->second->child_lookup_list)
      goto skip_remove_child_lookup_table;

    _parent_iter->second->child_lookup_list->erase(metadata->this_key);
} // enclosure closing
    skip_remove_child_lookup_table:{}
  }

  if(metadata->this_key){
    delete metadata->this_key;
    metadata->this_key = NULL;
  }
  
  if(metadata->this_value){
    cpplua_delete_variant(metadata->this_value);
    metadata->this_value = NULL;
  }

  if(metadata->child_lookup_list){
    delete metadata->child_lookup_list;
    metadata->child_lookup_list = NULL;
  }
}

void LuaVariableTree::_delete_variable_metadata(_variable_tree_item_metadata* metadata){
  _clear_variable_metadata(metadata);
  delete metadata;
}

void LuaVariableTree::_delete_tree_item_child(TreeItem* item){
  while(item->get_child_count() > 0){
    TreeItem* _current_item = item->get_child(0);
    _delete_tree_item(_current_item);
  }
}

void LuaVariableTree::_delete_tree_item(TreeItem* item){
  this->_on_item_deleting(item);

  _delete_tree_item_child(item);

  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter != _vartree_map.end()){
    if(item->get_instance_id() == _last_selected_id){
      _last_selected_item = NULL;
      _last_selected_id = 0;
    }

    _delete_variable_metadata(_iter->second);
    _vartree_map.erase(_iter);
  }

  TreeItem* _parent_item = item->get_parent();
  if(_parent_item)
    _parent_item->remove_child(item);
}


void LuaVariableTree::_notify_changed_tree_item(TreeItem* item){
  if(!item)
    return;

  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end())
    return;

  if(!_iter->second->this_value || !_iter->second->this_value->is_type(I_table_var::get_static_lua_type()))
    return;

  I_table_var* _tvar = dynamic_cast<I_table_var*>(_iter->second->this_value);
  if(!_tvar->is_reference())
    return;

  reference_changed_data _data;
    _data.reference_address = _tvar->get_table_pointer();
  
  emit_signal(s_on_reference_changed, convert_to_variant(&_data));

  // notify self
  std::set<uint64_t> _item_filter;
    _item_filter.insert(item->get_instance_id());
  _on_reference_data_changed_filter(_data, &_item_filter);
}


void LuaVariableTree::_bind_object(LuaVariableTree* obj){
  obj->connect(s_on_reference_changed, Callable(this, "_on_reference_data_changed"));
}


void LuaVariableTree::_variable_setter_do_popup_add_table_item(TreeItem* parent_item, uint64_t flag){
  _variable_setter_do_popup_add_table_item(parent_item, NULL, flag);
}

void LuaVariableTree::_variable_setter_do_popup_add_table_item(TreeItem* parent_item, TreeItem* value_item, uint64_t flag){
  I_variant* key = NULL;
  I_variant* value = NULL;

  if(value_item){
{ // enclosure for using gotos
    auto _viter = _vartree_map.find(value_item->get_instance_id());
    if(_viter == _vartree_map.end())
      goto skip_child_checking;

    key = _viter->second->this_key;
    value = _viter->second->this_value;
} // enclosure closing
    skip_child_checking:{}
  }

  auto _iter = _vartree_map.find(parent_item->get_instance_id());
  if(_iter == _vartree_map.end())
    return;

  bool _do_popup = true;

  _popup_variable_setter->set_variable_key("");
  if((flag & PopupVariableSetter::edit_add_key_edit) && key){
    string_store _str; key->to_string(&_str);
    _popup_variable_setter->set_variable_key(_str.data.c_str());
  }

  // check parent value (is table or not)
  if(!_iter->second->this_value || !_iter->second->this_value->is_type(I_table_var::get_static_lua_type())){
    GameUtils::Logger::print_err_static(gd_format_str("[LuaVariableTree] Parent item (ID: {0}) is not a valid Lua table.", parent_item->get_instance_id()));
    return;
  }

  // check local table
  if((flag & PopupVariableSetter::edit_add_key_edit)){
{ // enclosure for using goto
    PackedStringArray _choices;

    // skips if not local table
    if(!_iter->second->this_value->is_type(I_local_table_var::get_static_lua_type()))
      goto skip_local_block;
      
    I_table_var* _tvar = dynamic_cast<I_table_var*>(_iter->second->this_value);
    _tvar->update_keys();

    const I_variant** _key_list = _tvar->get_keys(); 
    for(int i = 0; _key_list[i]; i++){
      const I_variant* _key_var = _key_list[i];
      const I_variant* _value_var = _tvar->get_value(_key_var);
      if(_value_var->get_type() != I_nil_var::get_static_lua_type() ||
        _filter_key.find(_key_var) != _filter_key.end())
        continue;

      string_store _str; _key_var->to_string(&_str);
      if(_key_var->get_type() != I_string_var::get_static_lua_type()){
        GameUtils::Logger::print_warn_static(gd_format_str("[LuaVariableTree] Local variable '{0}' cannot be used for add/copy.", _str.data.c_str()));
        continue;
      }
      
      _choices.append(_str.data.c_str());

      _tvar->free_variant(_value_var);
    }

    _do_popup = _choices.size() > 0;
    if(!_do_popup){
      GameUtils::Logger::print_err_static("[LuaVariableTree] No empty local variables to add.");
      goto skip_local_block;
    }

    _popup_variable_setter->set_local_key_choice(_choices);
    flag |= PopupVariableSetter::edit_local_key;
} // enclosure closing
    skip_local_block:{}
  }

  if(_do_popup){
    switch(_last_context_id){
      break;
      case context_menu_add:
      case context_menu_add_table:
        break;

      break; default:
        _popup_variable_setter->set_popup_data(value);
    }

    _setter_usage_pass_data _usage_data;
      _usage_data.parent_id = parent_item->get_instance_id();
      
    _popup_variable_setter->set_edit_flag(flag);
    SignalOwnership(Signal(_popup_variable_setter, PopupVariableSetter::s_cancelled), Callable(this, "_on_setter_cancelled"))
      .replace_ownership();
    SignalOwnership(Signal(_popup_variable_setter, PopupVariableSetter::s_applied), Callable(this, "_on_setter_applied").bind(convert_to_variant(&_usage_data)))
      .replace_ownership();

    Vector2 _mouse_pos = get_tree()->get_root()->get_mouse_position();
    _popup_variable_setter->set_position(_mouse_pos);

    _on_variable_setter_popup();
    _popup_variable_setter->popup();
    _is_using_variable_setter = true;
  }
}


void LuaVariableTree::_on_reference_data_changed(const Variant& data){
  reference_changed_data _data = parse_variant_data<reference_changed_data>(data);
  _on_reference_data_changed_filter(_data, NULL);
}

void LuaVariableTree::_on_reference_data_changed_filter(const reference_changed_data& data, const std::set<uint64_t>* item_filter){
  auto _lookup_iter = _reference_lookup_list.find((uint64_t)data.reference_address);
  if(_lookup_iter == _reference_lookup_list.end())
    return;

  for(uint64_t id: _lookup_iter->second->item_list){
    if(item_filter){
      auto _filter_iter = item_filter->find(id);
      if(_filter_iter != item_filter->end())
        continue;
    }

    auto _iter = _vartree_map.find(id);
    if(_iter == _vartree_map.end())
      continue;
  
    _update_tree_item(_iter->second->this_item, NULL);
  }
}


void LuaVariableTree::_open_context_menu(){
  auto _iter = _vartree_map.find(_last_selected_id);
  if(_iter == _vartree_map.end())
    return;

  bool _value_can_be_ref = false;
  if(_iter->second->this_value){
    switch(_iter->second->this_value->get_type()){
      break;
      case I_table_var::get_static_lua_type():
      case I_function_var::get_static_lua_type():
        _value_can_be_ref = true;
    }
  }

  bool _has_table_parent = false;
  TreeItem* _parent_item = _last_selected_item->get_parent();
  if(_parent_item){
{ // enclosure for using gotos
    auto _parent_iter = _vartree_map.find(_parent_item->get_instance_id());
    if(_parent_iter == _vartree_map.end())
      goto skip_parent_checking;

    if(!_parent_iter->second->this_value || !_parent_iter->second->this_value->is_type(I_table_var::get_static_lua_type()))
      goto skip_parent_checking;

    _has_table_parent = true;
} // enclosure closing
    skip_parent_checking:{}
  }

  PopupContextMenu::MenuData _data;
  PopupContextMenu::MenuData::Part _tmp_part;
  if(_iter->second->_mflag & metadata_valid_mutable_item){
      _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
      _tmp_part.label = "Edit Variable";
      _tmp_part.id = context_menu_edit;
    _data.part_list.insert(_data.part_list.end(), _tmp_part);
      _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
      _tmp_part.label = "Delete Variable";
      _tmp_part.id = context_menu_remove;
    _data.part_list.insert(_data.part_list.end(), _tmp_part);
  }

  if(_iter->second->this_value){
    // create separator only when there's any item
    if(_data.part_list.size() > 0){
        _tmp_part.item_type = PopupContextMenu::MenuData::type_separator;
        _tmp_part.label = "";
        _tmp_part.id = -1;
      _data.part_list.insert(_data.part_list.end(), _tmp_part);
    }

    // check if table variable
    switch(_iter->second->this_value->get_type()){
      break; case I_table_var::get_static_lua_type():{
          _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
          _tmp_part.label = gd_format_str("Add Table Variable");
          _tmp_part.id = context_menu_add_table;
        _data.part_list.insert(_data.part_list.end(), _tmp_part);
      }

      break; case I_local_table_var::get_static_lua_type():{
          _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
          _tmp_part.label = gd_format_str("Add Local Variable");
          _tmp_part.id = context_menu_add_table;
        _data.part_list.insert(_data.part_list.end(), _tmp_part);
      }
    }
  }

  if(_has_table_parent){
      _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
      _tmp_part.label = gd_format_str("Add Variable{0}", (_iter->second->_mflag & metadata_local_item)? " (local)": "");
      _tmp_part.id = context_menu_add;
    _data.part_list.insert(_data.part_list.end(), _tmp_part);

    // disable copying value for local variable
    if((_iter->second->_mflag & metadata_local_item) <= 0){
        _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
        _tmp_part.label = "Copy Variable";
        _tmp_part.id = context_menu_copy;
      _data.part_list.insert(_data.part_list.end(), _tmp_part);
    }
  }

  // add custom contexts
  this->_add_custom_context(_iter->second, _data);
  
  if(_data.part_list.size() <= 0){
    GameUtils::Logger::print_warn_static(gd_format_str("Item (of ID: {0}) does not have any metadata flag?", _last_selected_id));
    return;
  }

  _global_context_menu->init_menu(_data);

  SignalOwnership(Signal(_global_context_menu, "id_pressed"), Callable(this, "_on_context_menu_clicked"))
    .replace_ownership();

  Vector2 _mouse_pos = get_tree()->get_root()->get_mouse_position();
  _global_context_menu->set_position(_mouse_pos);
  _global_context_menu->popup();
}


void LuaVariableTree::_on_variable_setter_popup(){
  ReferenceQueryMenu::ReferenceQueryFunction _query_function;
    this->_get_reference_query_function(&_query_function);

  _popup_variable_setter->set_reference_query_function_data(_query_function);
}

void LuaVariableTree::_on_variable_setter_closing(){
  ReferenceQueryMenu::ReferenceQueryFunction _query_function;
  _popup_variable_setter->set_reference_query_function_data(_query_function);
}


void LuaVariableTree::_get_data_from_variable_setter(_variable_tree_item_metadata* target_metadata, I_variant** pkey, I_variant** pvalue){
  uint32_t _mode = _popup_variable_setter->get_mode_type();
  const PopupVariableSetter::VariableData& _output = _popup_variable_setter->get_output_data();

  // get key
  string_var _key_str;
  *pkey = &_key_str; 
  switch(_last_context_id){
    break; case context_menu_edit:
    *pkey = target_metadata->this_key;

    break; default:{
      uint32_t _edit_flag = _popup_variable_setter->get_edit_flag();
      if(_edit_flag & PopupVariableSetter::edit_local_key){
        String _key_gdstr = _popup_variable_setter->get_local_key_applied();
        _key_str = GDSTR_TO_STDSTR(_key_gdstr);
      }
      else{
        String _key_gdstr = _popup_variable_setter->get_variable_key();
        _key_str = GDSTR_TO_STDSTR(_key_gdstr);
      }
    }
  }

  *pkey = cpplua_create_var_copy(*pkey);

  // parse value
  *pvalue = NULL;
  switch(_mode){
    break; case PopupVariableSetter::setter_mode_string:{
      string_var _svar = _output.string_data.c_str();
      *pvalue = cpplua_create_var_copy(&_svar);
    }

    break; case PopupVariableSetter::setter_mode_number:{
      number_var _nvar = _output.number_data;
      *pvalue = cpplua_create_var_copy(&_nvar);
    }

    break; case PopupVariableSetter::setter_mode_bool:{
      bool_var _bvar = _output.bool_data;
      *pvalue = cpplua_create_var_copy(&_bvar);
    }

    break; case PopupVariableSetter::setter_mode_add_table:{
      table_var _tvar;
      *pvalue = cpplua_create_var_copy(&_tvar);
    }

    break; case PopupVariableSetter::setter_mode_reference_list:{
      ReferenceQueryMenu::ReferenceQueryFunction _query_function = _popup_variable_setter->get_reference_query_function_data();
      PackedByteArray _fetch_value_barr = _query_function.fetch_value_item.call(_output.choosen_reference_id);
      ReferenceQueryMenu::ReferenceFetchValueResult _fetch_value = parse_variant_data<ReferenceQueryMenu::ReferenceFetchValueResult>(_fetch_value_barr);

      if(!_fetch_value.value)
        break;

      *pvalue = cpplua_create_var_copy(_fetch_value.value);

      cpplua_delete_variant(_fetch_value.value);
    }

    break; default:
      return;
  }
}


void LuaVariableTree::_update_item_text(TreeItem* item, const I_variant* key, const I_variant* value){
  const char* _prefix_string = "";
  const char* _suffix_string = "";

  // prefix and suffix changes
  switch(value->get_type()){
    break; case lua::string_var::get_static_lua_type():{
      _prefix_string = "\"";
      _suffix_string = "\"";
    }
  }

  string_store _key_str; key->to_string(&_key_str);
  string_store _value_str; value->to_string(&_value_str);
  std::string _format_val_str = format_str("%s: %s%s%s",
    _key_str.data.c_str(),
    _prefix_string,
    _value_str.data.c_str(),
    _suffix_string
  );

  item->set_text(0, _format_val_str.c_str());
}


void LuaVariableTree::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

{ // enclosure for using gotos
  _variable_tree = get_node<Tree>(_variable_tree_path);
  if(!_variable_tree){
    GameUtils::Logger::print_err_static("[VariableWatcher] Cannot get Tree for Variable Inspector.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }
  
  _gvariables = get_node<GlobalVariables>(GlobalVariables::singleton_path);
  if(!_gvariables){
    GameUtils::Logger::print_err_static("[VariableWatcher] Cannot get GlobalVariables.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  NodePath _popup_var_path = _gvariables->get_global_value(GlobalVariables::key_popup_variable_setter_path);
  _popup_variable_setter = get_node<PopupVariableSetter>(_popup_var_path);
  if(!_popup_variable_setter)
    return;

  NodePath _context_menu_path = _gvariables->get_global_value(GlobalVariables::key_context_menu_path);
  _global_context_menu = get_node<PopupContextMenu>(_context_menu_path);
  if(!_global_context_menu)
    return;

  NodePath _confirmation_dialog_path = _gvariables->get_global_value(GlobalVariables::key_confirmation_dialog_path);
  _global_confirmation_dialog = get_node<ConfirmationDialog>(_confirmation_dialog_path);
  if(!_global_confirmation_dialog)
    return;

  _variable_tree->connect("button_clicked", Callable(this, "_on_tree_button_clicked"));
  _variable_tree->connect("item_collapsed", Callable(this, "_item_collapsed_safe"));
  _variable_tree->connect("item_selected", Callable(this, "_item_selected"));
  _variable_tree->connect("nothing_selected", Callable(this, "_item_nothing_selected"));
  _variable_tree->connect("item_mouse_selected", Callable(this, "_item_selected_mouse"));
  _variable_tree->connect("empty_clicked", Callable(this, "_item_empty_clicked"));
  _variable_tree->connect("item_activated", Callable(this, "_item_activated"));
  
  _clear_variable_tree();
} // enclosure closing

  return;


  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  }
}

void LuaVariableTree::_process(double delta){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  if(_update_callable_list.size() > 0){
    for(Callable cb: _update_callable_list)
      cb.call();

    _update_callable_list.clear();
  }
}


NodePath LuaVariableTree::get_variable_tree_path() const{
  return _variable_tree_path;
}

void LuaVariableTree::set_variable_tree_path(const NodePath& path){
  _variable_tree_path = path;
}


Ref<Texture> LuaVariableTree::get_context_menu_button_texture() const{
  return _context_menu_button_texture;
}

void LuaVariableTree::set_context_menu_button_texture(Ref<Texture> texture){
  _context_menu_button_texture = texture;
}