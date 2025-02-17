#include "directory_util.h"
#include "defines.h"
#include "error_trigger.h"
#include "gd_string_store.h"
#include "logger.h"
#include "signal_ownership.h"
#include "strutil.h"
#include "variable_watcher.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/class_db.hpp"

#include "Lua-CPPAPI/Src/luaapi_value.h"
#include "Lua-CPPAPI/Src/luaapi_variant_util.h"
#include "Lua-CPPAPI/Src/luaapi_stack.h"

#include "algorithm"


using namespace gdutils;
using namespace godot;
using namespace lua;
using namespace lua::api;
using namespace lua::debug;
using namespace lua::util;


static const char* placeholder_group_name = "placeholder_node";


void VariableWatcher::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_global_variable_changed", "key", "value"), &VariableWatcher::_on_global_variable_changed);
  ClassDB::bind_method(D_METHOD("_on_option_control_changed", "key", "value"), &VariableWatcher::_on_option_control_changed);

  ClassDB::bind_method(D_METHOD("_lua_on_thread_starting"), &VariableWatcher::_lua_on_thread_starting);
  ClassDB::bind_method(D_METHOD("_lua_on_pausing"), &VariableWatcher::_lua_on_pausing);
  ClassDB::bind_method(D_METHOD("_lua_on_resuming"), &VariableWatcher::_lua_on_resuming);
  ClassDB::bind_method(D_METHOD("_lua_on_stopping"), &VariableWatcher::_lua_on_stopping);

  ClassDB::bind_method(D_METHOD("_item_collapsed_safe", "item"), &VariableWatcher::_item_collapsed_safe);
  ClassDB::bind_method(D_METHOD("_item_collapsed", "item"), &VariableWatcher::_item_collapsed);
  ClassDB::bind_method(D_METHOD("_item_selected"), &VariableWatcher::_item_selected);
  ClassDB::bind_method(D_METHOD("_item_nothing_selected"), &VariableWatcher::_item_nothing_selected);
  ClassDB::bind_method(D_METHOD("_item_selected_mouse", "mouse_pos", "mouse_button_index"), &VariableWatcher::_item_selected_mouse);
  ClassDB::bind_method(D_METHOD("_item_empty_clicked", "mouse_pos", "mouse_button_index"), &VariableWatcher::_item_empty_clicked);
  ClassDB::bind_method(D_METHOD("_item_activated"), &VariableWatcher::_item_activated);

  ClassDB::bind_method(D_METHOD("_on_setter_applied"), &VariableWatcher::_on_setter_applied);
  ClassDB::bind_method(D_METHOD("_on_tree_button_clicked", "tree", "column", "id", "mouse_btn"), &VariableWatcher::_on_tree_button_clicked);
  ClassDB::bind_method(D_METHOD("_on_context_menu_clicked", "id"), &VariableWatcher::_on_context_menu_clicked);

  ClassDB::bind_method(D_METHOD("get_variable_tree_path"), &VariableWatcher::get_variable_tree_path);
  ClassDB::bind_method(D_METHOD("set_variable_tree_path", "path"), &VariableWatcher::set_variable_tree_path);

  ClassDB::bind_method(D_METHOD("get_variable_storage_path"), &VariableWatcher::get_variable_storage_path);
  ClassDB::bind_method(D_METHOD("set_variable_storage_path", "path"), &VariableWatcher::set_variable_storage_path);

  ClassDB::bind_method(D_METHOD("get_context_menu_button_texture"), &VariableWatcher::get_context_menu_button_texture);
  ClassDB::bind_method(D_METHOD("set_context_menu_button_texture", "texture"), &VariableWatcher::set_context_menu_button_texture);

  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "variable_tree_path"), "set_variable_tree_path", "get_variable_tree_path");
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "variable_storage_path"), "set_variable_storage_path", "get_variable_storage_path");
  ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "context_menu_button_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), "set_context_menu_button_texture", "get_context_menu_button_texture");
}


VariableWatcher::VariableWatcher(){
  _variable_tree = NULL;
}

VariableWatcher::~VariableWatcher(){
  _clear_revealed_tree();
  _clear_variable_tree();
}


void VariableWatcher::_on_global_variable_changed(const String& key, const Variant& value){
  typedef void (VariableWatcher::*key_cb_type)(const Variant& value);
  std::map<String, key_cb_type> _key_cb = {
    {OptionControl::gvar_object_node_path, &VariableWatcher::_gvar_changed_option_control_path}
  };

  auto _iter = _key_cb.find(key);
  if(_iter == _key_cb.end())
    return;
  
  (this->*_iter->second)(value);
}

void VariableWatcher::_gvar_changed_option_control_path(const Variant& value){
  if(value.get_type() != Variant::NODE_PATH)
    return;

  NodePath _opt_control_path = value;
  OptionControl* _opt_control = get_node<OptionControl>(_opt_control_path);
  if(!_opt_control){
    GameUtils::Logger::print_err_static(gd_format_str("[VariableWatcher] Path '{0}' is not a valid OptionControl object.", _opt_control_path));
    return;
  }

  _bind_object(_opt_control);
}


void VariableWatcher::_on_option_control_changed(const String& key, const Variant& value){
  typedef void (VariableWatcher::*key_cb_type)(const Variant& value);
  std::map<String, key_cb_type> _key_cb = {
    {"ignore_internal_data", &VariableWatcher::_option_changed_ignore_internal_data}
  };

  auto _iter = _key_cb.find(key);
  if(_iter == _key_cb.end())
    return;

  (this->*_iter->second)(value);
}

void VariableWatcher::_option_changed_ignore_internal_data(const Variant& value){
  set_ignore_internal_variables(value);
}


void VariableWatcher::_lua_on_thread_starting(){
  _lua_program_handle->get_variable_watcher()->ignore_internal_variables(_ignore_internal_variables);
}

void VariableWatcher::_lua_on_pausing(){
  _update_variable_tree();
}

void VariableWatcher::_lua_on_resuming(){
  _clear_variable_tree();
}

void VariableWatcher::_lua_on_stopping(){
  _clear_revealed_tree();
  _clear_variable_tree();
}


void VariableWatcher::_item_collapsed_safe(TreeItem* item){
  _update_callable_list.insert(_update_callable_list.end(), Callable(this, "_item_collapsed").bind(item));
}

void VariableWatcher::_item_collapsed(TreeItem* item){
  if(!_lua_program_handle->is_running() || item->is_collapsed())
    return;

  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end() || _iter->second->already_revealed)
    return;

  if(_iter->second->this_value->get_type() != I_table_var::get_static_lua_type())
    return;

  _reveal_tree_item(item, dynamic_cast<I_table_var*>(_iter->second->this_value));
}

void VariableWatcher::_item_selected(){
  _last_selected_item = _variable_tree->get_selected();
  _last_selected_id = _last_selected_item->get_instance_id();
}

void VariableWatcher::_item_nothing_selected(){
  _last_selected_item = NULL;
  _last_selected_id = 0;
}

void VariableWatcher::_item_selected_mouse(const Vector2 mouse_pos, int mouse_idx){
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

void VariableWatcher::_item_empty_clicked(const Vector2 mouse_pos, int mouse_idx){
  _last_selected_item = NULL;
  _last_selected_id = 0;
}

void VariableWatcher::_item_activated(){
  TreeItem* _item = _variable_tree->get_selected();
  if(_item){
    _last_selected_item = _item;
    _last_selected_id = _item->get_instance_id();
  }

  _last_context_id = context_menu_edit;
  _variable_setter_do_popup();
}


void VariableWatcher::_on_setter_applied(){
  if(!_lua_program_handle->is_running())
    return;

  TreeItem* _current_item = _last_selected_item;
  auto _current_iter = _vartree_map.find(_current_item->get_instance_id());
  if(_current_iter == _vartree_map.end())
    return;

  _variable_tree_item_metadata* _metadata = _current_iter->second;

  uint32_t _mode = _popup_variable_setter->get_mode_type();
  const PopupVariableSetter::VariableData& _output = _popup_variable_setter->get_output_data();

  // this should be a copy of the var
  I_variant* _key_var = NULL;
  // this should be a copy of the var
  I_variant* _value_var = NULL;

{ // enclosure for using goto

  // get key
  string_var _key_str;
  _key_var = &_key_str; 
  switch(_last_context_id){
    break; case context_menu_edit:
      _key_var = _metadata->this_key;

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

  _key_var = cpplua_create_var_copy(_key_var);

  // parse value
  _value_var = NULL;
  switch(_mode){
    break; case PopupVariableSetter::setter_mode_string:{
      string_var _svar = _output.string_data.c_str();
      _value_var = cpplua_create_var_copy(&_svar);
    }

    break; case PopupVariableSetter::setter_mode_number:{
      number_var _nvar = _output.number_data;
      _value_var = cpplua_create_var_copy(&_nvar);
    }

    break; case PopupVariableSetter::setter_mode_bool:{
      bool_var _bvar = _output.bool_data;
      _value_var = cpplua_create_var_copy(&_bvar);
    }

    break; default:
      goto skip_to_cleaning;
  }

  // invoke based on context
  switch(_last_context_id){
    break;
    case context_menu_add:
    case context_menu_copy:
      _on_setter_applied_add_or_copy(_current_item, _metadata, _key_var, _value_var);

    break; case context_menu_add_table:
      _on_setter_applied_add_table(_current_item, _metadata, _key_var, _value_var);

    break; case context_menu_edit:
      _on_setter_applied_edit(_current_item, _metadata, _key_var, _value_var);
  }
} // enclosure closing

  skip_to_cleaning:{}

  if(_key_var)
    cpplua_delete_variant(_key_var);

  if(_value_var)
    cpplua_delete_variant(_value_var);
}

void VariableWatcher::_on_setter_applied_add_or_copy(TreeItem* current_item, _variable_tree_item_metadata* metadata, I_variant* key_var, I_variant* value_var){
  // set the value
  bool _success = metadata->var_setter->set_value(value_var, key_var);
  if(!_success){
    I_variable_watcher* _vwatch = _lua_program_handle->get_variable_watcher();
    const I_error_var* _err_var = _vwatch->get_last_error();
    gd_string_store _err_str = "(null)"; 
    gd_string_store _key_str = "(null)";
    
    if(_err_var){
      _err_str.data = "";
      _err_var->to_string(&_err_str);
    }

    if(key_var)
      key_var->to_string(&_key_str);

    GameUtils::Logger::print_err_static(gd_format_str("Error occurred when adding '{0}' variable. Err: {1}", _key_str.data, _err_str.data));
    return;
  }
  
  uint32_t _edit_flag = _popup_variable_setter->get_edit_flag();

  // add new item
  if(_edit_flag & PopupVariableSetter::edit_local_key){
    TreeItem* _new_item = _create_tree_item(_local_item);
    _variable_tree_item_metadata* _metadata = _vartree_map[_new_item->get_instance_id()];
    _metadata->_mflag = metadata_local_item | metadata_valid_mutable_item;
    _metadata->var_setter = _local_setter;

    current_item = _new_item;
  }
  else{
    // add vsetter to table
    TreeItem* _parent_item = metadata->parent_item;
    TreeItem* _new_item = _create_tree_item(_parent_item);
    _variable_tree_item_metadata* _metadata = _vartree_map[_new_item->get_instance_id()];
    _metadata->_mflag = metadata_valid_mutable_item;
    
    current_item = _new_item;

    // find VariableSetter from its parent
    IVariableSetter* _current_vsetter = _find_setter(_new_item->get_parent());
    if(!_current_vsetter){
      I_variant* _parent_value = NULL;

{ // enclosure for using goto
      TreeItem* _parent_item = _new_item->get_parent();
      if(!_parent_item)
        goto skip_if_block1;

      auto _parent_iter = _vartree_map.find(_parent_item->get_instance_id());
      if(_parent_iter == _vartree_map.end())
        goto skip_if_block1;

      if(!_parent_iter->second->this_value || _parent_iter->second->this_value->get_type() != I_table_var::get_static_lua_type())
        goto skip_if_block1;

      _parent_value = _parent_iter->second->this_value;
} // enclosure closing

      skip_if_block1:{}

      // check if setter should be table or global
      if(_parent_value)
        _current_vsetter = new TableVariableSetter(dynamic_cast<I_table_var*>(_parent_value));
      else
        _current_vsetter = new GlobalVariableSetter(_lua_program_handle->get_variable_watcher());

      _setter_list.insert(_setter_list.end(), _current_vsetter);
    }

    _metadata->var_setter = _current_vsetter;
  }

  _update_tree_item(current_item, key_var, value_var);
}

void VariableWatcher::_on_setter_applied_add_table(TreeItem* current_item, _variable_tree_item_metadata* metadata, I_variant* key_var, I_variant* value_var){
  // find any variable setter of the table
  IVariableSetter* _vsetter = _find_setter(current_item);
  if(!_vsetter){
{ // enclosure for using goto
    if(!metadata->this_value || metadata->this_value->get_type() != I_table_var::get_static_lua_type())
      goto skip_if_block1;

    // TODO optimize, it will do a dupe (when revealed)
    _vsetter = new TableVariableSetter(dynamic_cast<I_table_var*>(metadata->this_value));

    _setter_list.insert(_setter_list.end(), _vsetter);
} // enclosure closing
    
    skip_if_block1:{}
  }

  // set the value
  bool _success = _vsetter->set_value(value_var, key_var);
  if(!_success){
    I_variable_watcher* _vwatch = _lua_program_handle->get_variable_watcher();
    const I_error_var* _err_var = _vwatch->get_last_error();
    gd_string_store _err_str = "(null)"; 
    gd_string_store _key_str = "(null)";
    
    if(_err_var){
      _err_str.data = "";
      _err_var->to_string(&_err_str);
    }

    if(key_var)
      key_var->to_string(&_key_str);

    GameUtils::Logger::print_err_static(gd_format_str("Error occurred when adding '{0}' variable to a table. Err: {1}", _key_str.data, _err_str.data));
    return;
  }
  
  // reveal the table, if already revealed, add the item as table's child
  bool _add_item = true;
  auto _parent_tree_iter = _vartree_map.find(current_item->get_instance_id());
  if(!_parent_tree_iter->second->already_revealed){
    _add_item = false;
    _reveal_tree_item(current_item, dynamic_cast<I_table_var*>(_parent_tree_iter->second->this_value));
  }

  if(_add_item){
    TreeItem* _new_item = _create_tree_item(current_item);
    _variable_tree_item_metadata* _metadata = _vartree_map[_new_item->get_instance_id()];
    _metadata->_mflag = metadata_valid_mutable_item;
    _metadata->var_setter = _vsetter;

    _update_tree_item(_new_item, key_var, value_var);
  }
}

void VariableWatcher::_on_setter_applied_edit(TreeItem* current_item, _variable_tree_item_metadata* metadata, I_variant* key_var, I_variant* value_var){
  bool _success = metadata->var_setter->set_value(value_var, key_var);
  if(!_success){
    I_variable_watcher* _vwatch = _lua_program_handle->get_variable_watcher();
    const I_error_var* _err_var = _vwatch->get_last_error();
    gd_string_store _err_str = "(null)"; 
    gd_string_store _key_str = "(null)";
    
    if(_err_var){
      _err_str.data = "";
      _err_var->to_string(&_err_str);
    }

    if(key_var)
      key_var->to_string(&_key_str);

    GameUtils::Logger::print_err_static(gd_format_str("Error occurred when editing '{0}' variable. Err: {1}", _key_str.data, _err_str.data));
    return;
  }

  _update_tree_item(current_item, key_var, value_var);
}


void VariableWatcher::_on_tree_button_clicked(TreeItem* item, int column, int id, int mouse_button){
  _last_selected_item = item;
  _last_selected_id = item->get_instance_id();
  switch(id){
    break; case button_id_context_menu:
      _open_context_menu();
  }
}

void VariableWatcher::_on_context_menu_clicked(int id){
  _last_context_id = id;
  switch(id){
    break; case context_menu_edit:
      _variable_setter_do_popup_id(_last_selected_id);

    break;
    case context_menu_add:
    case context_menu_copy:{
      auto _iter = _vartree_map.find(_last_selected_id);
      if(_iter == _vartree_map.end())
        break;

      uint64_t _edit_flag = PopupVariableSetter::edit_add_value_edit | PopupVariableSetter::edit_add_key_edit;
      _edit_flag |= id == context_menu_add? PopupVariableSetter::edit_clear_on_popup: PopupVariableSetter::edit_flag_none;
      if(_iter->second->_mflag & metadata_local_item)
        _edit_flag |= PopupVariableSetter::edit_local_key;

      _variable_setter_do_popup_id(_last_selected_id, _edit_flag);
    }

    break; case context_menu_add_table:{
      auto _iter = _vartree_map.find(_last_selected_id);
      if(_iter == _vartree_map.end())
        break;

      uint64_t _edit_flag = 
      PopupVariableSetter::edit_add_value_edit | PopupVariableSetter::edit_add_key_edit | PopupVariableSetter::edit_clear_on_popup;

      _variable_setter_do_popup_id(_last_selected_id, _edit_flag);
    }

    break; case context_menu_remove:
      _remove_item(_last_selected_item);

    break; case context_menu_add_to_storage_copy:{
      auto _iter = _vartree_map.find(_last_selected_id);
      if(_iter == _vartree_map.end())
        break;

      _vstorage->add_to_storage(_iter->second->this_value, _iter->second->this_key);
    }

    break; case context_menu_add_to_storage_reference:{
      auto _iter = _vartree_map.find(_last_selected_id);
      if(_iter == _vartree_map.end())
        break;

      _vstorage->add_to_storage(_iter->second->this_value, _iter->second->this_key, VariableStorage::sf_store_as_reference);
    }
  }
}


void VariableWatcher::_variable_setter_do_popup(uint64_t flag){
  if(!_lua_program_handle->is_running())
    return;

  _variable_setter_do_popup_id(_last_selected_id, flag);
}

void VariableWatcher::_variable_setter_do_popup_id(uint64_t id, uint64_t flag){
  if(!_lua_program_handle->is_running())
    return;

  auto _iter = _vartree_map.find(id);
  if(_iter == _vartree_map.end())
    return;

  bool _do_popup = true;

  _popup_variable_setter->set_variable_key("");
  if((flag & PopupVariableSetter::edit_add_key_edit) && _iter->second->this_key){
    string_store _str; _iter->second->this_key->to_string(&_str);
    _popup_variable_setter->set_variable_key(_str.data.c_str());
  }

  if((flag & PopupVariableSetter::edit_add_key_edit) && (flag & PopupVariableSetter::edit_local_key)){
{ // enclosure for using goto
    PackedStringArray _choices;

    I_variable_watcher* _vwatcher = _lua_program_handle->get_variable_watcher();
    _vwatcher->update_local_variables(_vwatcher->get_current_local_stack_idx());
    for(int i = 0; i < _vwatcher->get_variable_count(); i++){
      const I_variant* _key_var = _vwatcher->get_variable_key(i);
      const I_variant* _value_var = _vwatcher->get_variable_value(i);
      if(_value_var->get_type() != I_nil_var::get_static_lua_type() ||
        _filter_key.find(_key_var) != _filter_key.end())
        continue;

      string_store _str; _key_var->to_string(&_str);
      if(_key_var->get_type() != I_string_var::get_static_lua_type()){
        GameUtils::Logger::print_warn_static(gd_format_str("[VariableWatcher] Local variable '{0}' cannot be used for add/copy.", _str.data.c_str()));
        continue;
      }

      _choices.append(_str.data.c_str());
    }

    _do_popup = _choices.size() > 0;
    if(!_do_popup){
      GameUtils::Logger::print_err_static("[VariableWatcher] No empty local variables to add.");
      goto skip_if_block;
    }

    _popup_variable_setter->set_local_key_choice(_choices);

} // enclosure closing

    skip_if_block:{}
  }

  if(_do_popup){
    switch(_last_context_id){
      break;
      case context_menu_add:
      case context_menu_add_table:
        break;

      break; default:
        _popup_variable_setter->set_popup_data(_iter->second->this_value);
    }
      
    _popup_variable_setter->set_edit_flag(flag);
    SignalOwnership(Signal(_popup_variable_setter, PopupVariableSetter::s_applied), Callable(this, "_on_setter_applied"))
      .replace_ownership();

    Vector2 _mouse_pos = get_tree()->get_root()->get_mouse_position();
    _popup_variable_setter->set_position(_mouse_pos);
    _popup_variable_setter->popup();
  }
}


IVariableSetter* VariableWatcher::_find_setter(TreeItem* parent_item){
  if(!parent_item)
    return NULL;

  for(int i = 0; i < parent_item->get_child_count(); i++){
    TreeItem* _child_item = parent_item->get_child(i);
    auto _iter = _vartree_map.find(_child_item->get_instance_id());
    if(_iter == _vartree_map.end() || !_iter->second->var_setter)
      continue;

    return _iter->second->var_setter;
  }

  return NULL;
}


void VariableWatcher::_open_context_menu(){
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

  // create separator only when there's any item
  if(_data.part_list.size() > 0){
      _tmp_part.item_type = PopupContextMenu::MenuData::type_separator;
      _tmp_part.label = "";
      _tmp_part.id = -1;
    _data.part_list.insert(_data.part_list.end(), _tmp_part);
  }

  if(_iter->second->this_value && _iter->second->this_value->get_type() == I_table_var::get_static_lua_type()){
      _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
      _tmp_part.label = gd_format_str("Add Table Variable");
      _tmp_part.id = context_menu_add_table;
    _data.part_list.insert(_data.part_list.end(), _tmp_part);
  }

    _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
    _tmp_part.label = gd_format_str("Add Variable{0}", (_iter->second->_mflag & metadata_local_item)? " (local)": "");
    _tmp_part.id = context_menu_add;
  _data.part_list.insert(_data.part_list.end(), _tmp_part);

  // disable copying value for local variable
  if(!(_iter->second->_mflag & metadata_local_item) && (_iter->second->_mflag & metadata_valid_mutable_item)){
      _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
      _tmp_part.label = "Copy Variable";
      _tmp_part.id = context_menu_copy;
    _data.part_list.insert(_data.part_list.end(), _tmp_part);
  }

  // add menu for adding to storage
  if(_iter->second->this_value){
      _tmp_part.item_type = PopupContextMenu::MenuData::type_separator;
      _tmp_part.label = "";
      _tmp_part.id = -1;
    _data.part_list.insert(_data.part_list.end(), _tmp_part);

      _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
      _tmp_part.label = "Add To Storage (copy)";
      _tmp_part.id = context_menu_add_to_storage_copy;
    _data.part_list.insert(_data.part_list.end(), _tmp_part);

    if(_value_can_be_ref){
        _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
        _tmp_part.label = "Add To Storage (ref)";
        _tmp_part.id = context_menu_add_to_storage_reference;
      _data.part_list.insert(_data.part_list.end(), _tmp_part);
    }
  }

  if(_data.part_list.size() <= 0){
    GameUtils::Logger::print_warn_static(gd_format_str("Item (of ID: {0}) does not have any metadata flag?", _last_selected_id));
    return;
  }

  _global_context_menu->init_menu(_data);

  SignalOwnership _sownership(Signal(_global_context_menu, "id_pressed"), Callable(this, "_on_context_menu_clicked"));
  _sownership.replace_ownership();

  Vector2 _mouse_pos = get_tree()->get_root()->get_mouse_position();
  _global_context_menu->set_position(_mouse_pos);
  _global_context_menu->popup();
}


void VariableWatcher::_update_variable_tree(){
  _clear_variable_tree();

  _lua_program_handle->lock_object();
  if(!_lua_program_handle->is_running())
    goto skip_to_return;


{ // enclosure for using gotos
  core _lc = _lua_program_handle->get_runtime_handler()->get_lua_core_copy();
  
  _parsed_table_list.clear();

  I_variable_watcher* _variable_watcher = _lua_program_handle->get_variable_watcher();
  I_execution_flow* _execution_flow = _lua_program_handle->get_execution_flow();
  
  std::string _file_name = DirectoryUtil::strip_path(_lua_program_handle->get_current_running_file());
  TreeItem* _file_item = _variable_tree->create_item();
  _file_item->set_text(0, _file_name.c_str());

  // update local variables
  _variable_watcher->update_local_variables();
  _local_setter = new LocalVariableSetter(_variable_watcher);

  std::string _func_name = _lua_program_handle->get_current_function();
  _local_item = _create_tree_item(_file_item);
  _local_item->set_text(0, _func_name.c_str());

  _variable_tree_item_metadata* _lmetadata = _vartree_map[_local_item->get_instance_id()];
  _lmetadata->parent_item = _local_item;
  _lmetadata->_mflag |= metadata_local_item;
  _lmetadata->var_setter = _local_setter;
  string_var _lkey_var = "_L";
  _lmetadata->this_key = cpplua_create_var_copy(&_lkey_var);

  _update_tree_item(_local_item, _variable_watcher, true);

  // update global variables
  _lc.context->api_value->pushglobaltable(_lc.istate);
  I_variant* _global_table_var = _lc.context->api_varutil->to_variant(_lc.istate, -1);
  if(_global_table_var->get_type() == I_table_var::get_static_lua_type()){
    I_table_var* _global_table_tvar = dynamic_cast<I_table_var*>(_global_table_var);
    _parsed_table_list.insert(_global_table_tvar->get_table_pointer());
  }

  _lc.context->api_stack->pop(_lc.istate, 1);
  _lc.context->api_varutil->delete_variant(_global_table_var);

  _variable_watcher->update_global_variables();
  _global_setter = new GlobalVariableSetter(_variable_watcher);

  _global_item = _create_tree_item(_file_item);
  _global_item->set_text(0, "Global");
  
  _variable_tree_item_metadata* _gmetadata = _vartree_map[_global_item->get_instance_id()];
  _gmetadata->parent_item = _global_item;
  _gmetadata->var_setter = _global_setter;
  string_var _gkey_var = "_G";
  _gmetadata->this_key = cpplua_create_var_copy(&_gkey_var);

  _update_tree_item(_global_item, _variable_watcher, false);
} // enclosure closing

  skip_to_return:{}
  _lua_program_handle->unlock_object();
}


void VariableWatcher::_update_tree_item(TreeItem* parent_item, I_variable_watcher* watcher, bool as_local){
  _delete_tree_item_child(parent_item);

  IVariableSetter* _vsetter;
  if(as_local)
    _vsetter = _local_setter;
  else
    _vsetter = _global_setter;

  const core _lc = _lua_program_handle->get_runtime_handler()->get_lua_core_copy();
  for(int i = 0; i < watcher->get_variable_count(); i++){
    const I_variant* _key_var = watcher->get_variable_key(i);
    auto _iter = _filter_key.find(_key_var);
    if(_iter != _filter_key.end())
      continue;

    TreeItem* _var_title = _create_tree_item(parent_item);
    _variable_tree_item_metadata* _metadata = _vartree_map[_var_title->get_instance_id()];
    _metadata->var_setter = _vsetter;
    _metadata->_mflag |= metadata_valid_mutable_item;
    _metadata->_mflag |= as_local? metadata_local_item: 0;

    I_variant* _value_var = watcher->get_variable_value_mutable(i);
    _update_tree_item(_var_title, _key_var, _value_var);
  }
}

void VariableWatcher::_update_tree_item(TreeItem* parent_item, const I_variant* key_var, I_variant* var){
  _delete_tree_item_child(parent_item);

  const char* _prefix_string = "";
  const char* _suffix_string = "";

  // prefix and suffix changes
  switch(var->get_type()){
    break; case lua::string_var::get_static_lua_type():{
      _prefix_string = "\"";
      _suffix_string = "\"";
    }
  }

  string_store _key_str; key_var->to_string(&_key_str);
  string_store _value_str; var->to_string(&_value_str);
  std::string _format_val_str = format_str("%s: %s%s%s",
    _key_str.data.c_str(),
    _prefix_string,
    _value_str.data.c_str(),
    _suffix_string
  );

  parent_item->set_text(0, _format_val_str.c_str());

  auto _iter = _vartree_map.find(parent_item->get_instance_id());
  if(_iter != _vartree_map.end()){
    _clear_variable_metadata(_iter->second);

    _iter->second->this_key = cpplua_create_var_copy(key_var);
    _iter->second->this_value = cpplua_create_var_copy(var);
  }

  switch(var->get_type()){
    // another switch case due to this case needed a initialized metadata
    break; case I_table_var::get_static_lua_type():{
      auto _vt_iter = _vartree_map.find(parent_item->get_instance_id());
      if(_vt_iter == _vartree_map.end() || _vt_iter->second->already_revealed)
        break;

      if(_is_node_revealed(parent_item))
        _reveal_tree_item(parent_item, dynamic_cast<I_table_var*>(var));
      else{
        _variable_tree->create_item(parent_item);
        parent_item->set_collapsed(true);
      }
    }
  }

  _sort_item_child(parent_item->get_parent());
}


void VariableWatcher::_reveal_tree_item(TreeItem* parent_item, I_table_var* var){
  _delete_tree_item_child(parent_item);
  
  IVariableSetter* _vsetter = new TableVariableSetter(var);
  _setter_list.insert(_setter_list.end(), _vsetter);

  auto _vt_iter = _vartree_map.find(parent_item->get_instance_id());
  if(_vt_iter == _vartree_map.end())
    return;

  auto _pt_iter = _parsed_table_list.find(var->get_table_pointer());
  if(_pt_iter != _parsed_table_list.end())
    return;

  _vt_iter->second->already_revealed = true;
  _reveal_node_by(parent_item);

  _parsed_table_list.insert(var->get_table_pointer());

  const core _lc = _lua_program_handle->get_runtime_handler()->get_lua_core_copy();
  var->update_keys();
  const I_variant** _keys_list = var->get_keys();
  
  int _idx = 0;
  while(_keys_list[_idx]){
    const I_variant* _key_data = _keys_list[_idx];
    auto _iter = _filter_key.find(_key_data);
    if(_iter == _filter_key.end()){
      TreeItem* _var_title = _create_tree_item(parent_item);
      _variable_tree_item_metadata* _metadata = _vartree_map[_var_title->get_instance_id()];
      _metadata->var_setter = _vsetter;
      _metadata->_mflag |= metadata_valid_mutable_item;

      I_variant* _value_data = var->get_value(_key_data);
      _update_tree_item(_var_title, _key_data, _value_data);
      var->free_variant(_value_data);
    }

    _idx++;
  }

  parent_item->set_collapsed(false);
}

void VariableWatcher::_reveal_node_by(TreeItem* item){
  std::vector<comparison_variant> _step_list;
  TreeItem* _current_item = item;
  while(true){
    if(!_current_item)
      break;

    auto _iter = _vartree_map.find(_current_item->get_instance_id());
    if(_iter == _vartree_map.end() || !_iter->second->this_key)
      break;

    _step_list.insert(_step_list.begin(), _iter->second->this_key);
    _current_item = _current_item->get_parent();
  }

  _revealed_node* _current_node = &_revealed_tree;
  for(comparison_variant& cvar: _step_list){
    auto _check_iter = _current_node->branches.find(cvar);
    if(_check_iter == _current_node->branches.end()){
      _revealed_node* _new_node = new _revealed_node();
      _current_node->branches[cvar] = _new_node;

      _current_node = _new_node;
      continue;
    }

    _current_node = _check_iter->second;
  }
}


void VariableWatcher::_sort_item_child(TreeItem* parent_item){
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


bool VariableWatcher::_is_node_revealed(TreeItem* item){
  std::vector<comparison_variant> _step_list;
  TreeItem* _current_item = item;
  while(true){
    if(!_current_item)
      break;

    // TODO add this_key check to NULL
    auto _iter = _vartree_map.find(_current_item->get_instance_id());
    if(_iter == _vartree_map.end())
      break;

    _step_list.insert(_step_list.begin(), _iter->second->this_key);
    _current_item = _current_item->get_parent();
  }

  bool _result = true;
  _revealed_node* _current_node = &_revealed_tree;
  for(comparison_variant& cvar: _step_list){
    auto _check_iter = _current_node->branches.find(cvar);
    if(_check_iter == _current_node->branches.end()){
      _result = false;
      break;
    }

    _current_node = _check_iter->second;
  }

  return _result;
}


void VariableWatcher::_update_placeholder_state(){
  if(!_ginvoker)
    return;

  _ginvoker->invoke(placeholder_group_name, "set_visible", _variable_tree->get_root() == NULL);
}


TreeItem* VariableWatcher::_create_tree_item(TreeItem* parent_item){
  TreeItem* _result = _variable_tree->create_item(parent_item);
  _vartree_map[_result->get_instance_id()] = _create_vartree_metadata(parent_item);

  _result->add_button(0, _context_menu_button_texture, button_id_context_menu);

  _update_placeholder_state();
  return _result;
}

VariableWatcher::_variable_tree_item_metadata* VariableWatcher::_create_vartree_metadata(TreeItem* parent_item){
  _variable_tree_item_metadata* _result = new _variable_tree_item_metadata();
  _result->parent_item = parent_item;
  _result->this_key = NULL;
  _result->this_value = NULL;
  _result->var_setter = NULL;

  return _result;
}


void VariableWatcher::_remove_item(TreeItem* item){
  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end())
    return;

  nil_var _nvar;
  _iter->second->var_setter->set_value(&_nvar, _iter->second->this_key);
  _delete_tree_item_child(item);
  _delete_tree_item(item);
}


void VariableWatcher::_clear_revealed_tree(){
  _clear_revealed_node(&_revealed_tree);
}

void VariableWatcher::_clear_revealed_node(_revealed_node* node){
  for(auto _pair: node->branches)
    _delete_revealed_node(_pair.second);

  node->branches.clear();
}

void VariableWatcher::_delete_revealed_node(_revealed_node* node){
  _clear_revealed_node(node);
  delete node;
}


void VariableWatcher::_clear_variable_tree(){
  if(_variable_tree){
    _global_item = NULL;
    _local_item = NULL;
    _variable_tree->clear();
    
    _update_placeholder_state();
  }

  _clear_variable_metadata_map();
  _clear_vsetter_list();
}

void VariableWatcher::_clear_vsetter_list(){
  if(_global_setter){
    delete _global_setter;
    _global_setter = NULL;
  }
  
  if(_local_setter){
    delete _local_setter;
    _local_setter = NULL;
  }

  for(auto _vsetter: _setter_list)
    delete _vsetter;

  _setter_list.clear();
}

void VariableWatcher::_clear_variable_metadata_map(){
  for(auto _pair: _vartree_map)
    _delete_variable_metadata(_pair.second);

  _vartree_map.clear();
}

void VariableWatcher::_clear_variable_metadata(_variable_tree_item_metadata* metadata){
  if(metadata->this_key){
    cpplua_delete_variant(metadata->this_key);
    metadata->this_key = NULL;
  }

  if(metadata->this_value){
    cpplua_delete_variant(metadata->this_value);
    metadata->this_value = NULL;
  }
}

void VariableWatcher::_delete_variable_metadata(_variable_tree_item_metadata* metadata){
  _clear_variable_metadata(metadata);
  delete metadata;
}

void VariableWatcher::_delete_tree_item_child(TreeItem* item){
  while(item->get_child_count() > 0){
    TreeItem* _current_item = item->get_child(0);
    _delete_tree_item(_current_item);
  }
}

void VariableWatcher::_delete_tree_item(TreeItem* item){
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

  _update_placeholder_state();
}


void VariableWatcher::_bind_object(OptionControl* obj){
  obj->connect(OptionControl::s_value_set, Callable(this, "_on_option_control_changed"));
}


void VariableWatcher::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

{ // enclosure for using goto
  _lua_lib = get_node<LibLuaHandle>("/root/GlobalLibLuaHandle");
  if(!_lua_lib){
    GameUtils::Logger::print_err_static("[VariableWatcher] Cannot get Library Handle for Lua.");

    _quit_code = ERR_UNAVAILABLE;
    goto on_error_label;
  }

  _lua_program_handle = get_node<LuaProgramHandle>("/root/GlobalLuaProgramHandle");
  if(!_lua_program_handle){
    GameUtils::Logger::print_err_static("[VariableWatcher] Cannot get Program Handle for Lua.");

    _quit_code = ERR_UNAVAILABLE;
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

  _variable_tree = get_node<Tree>(_variable_tree_path);
  if(!_variable_tree){
    GameUtils::Logger::print_err_static("[VariableWatcher] Cannot get Tree for Variable Inspector.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _vstorage = get_node<VariableStorage>(_vstorage_node_path);
  if(!_vstorage){
    GameUtils::Logger::print_err_static("[VariableWatcher] Cannot get Variable Storage.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _ginvoker = find_any_node<GroupInvoker>(this);
  if(!_ginvoker)
    GameUtils::Logger::print_warn_static("[VariableWatcher] Cannot get Group Invoker.");

  _variable_tree->connect("button_clicked", Callable(this, "_on_tree_button_clicked"));
  _variable_tree->connect("item_collapsed", Callable(this, "_item_collapsed_safe"));
  _variable_tree->connect("item_selected", Callable(this, "_item_selected"));
  _variable_tree->connect("nothing_selected", Callable(this, "_item_nothing_selected"));
  _variable_tree->connect("item_mouse_selected", Callable(this, "_item_selected_mouse"));
  _variable_tree->connect("empty_clicked", Callable(this, "_item_empty_clicked"));
  _variable_tree->connect("item_activated", Callable(this, "_item_activated"));

  // just in case
  Variant _opt_control_path = _gvariables->get_global_value(OptionControl::gvar_object_node_path);
  _gvar_changed_option_control_path(_opt_control_path);

  _gvariables->connect(GlobalVariables::s_global_value_set, Callable(this, "_on_global_variable_changed"));

  _lua_lib_data = _lua_lib->get_library_store();

  _lua_program_handle->connect(LuaProgramHandle::s_thread_starting, Callable(this, "_lua_on_thread_starting"));
  _lua_program_handle->connect(LuaProgramHandle::s_pausing, Callable(this, "_lua_on_pausing"));
  _lua_program_handle->connect(LuaProgramHandle::s_resuming, Callable(this, "_lua_on_resuming"));
  _lua_program_handle->connect(LuaProgramHandle::s_stopping, Callable(this, "_lua_on_stopping"));

  _clear_variable_tree();
} // enclosure closing

  return;


  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  }
}

void VariableWatcher::_process(double delta){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  if(_update_callable_list.size() > 0){
    for(Callable cb: _update_callable_list)
      cb.call();

    _update_callable_list.clear();
  }
}


void VariableWatcher::set_ignore_internal_variables(bool flag){
  _ignore_internal_variables = flag;
  if(_lua_program_handle->is_running()){
    I_variable_watcher* _vw = _lua_program_handle->get_variable_watcher();
    _vw->ignore_internal_variables(_ignore_internal_variables);
  }

  _update_variable_tree();
}

bool VariableWatcher::get_ignore_internal_variables() const{
  return _ignore_internal_variables;
}


NodePath VariableWatcher::get_variable_tree_path() const{
  return _variable_tree_path;
}

void VariableWatcher::set_variable_tree_path(const NodePath& path){
  _variable_tree_path = path;
}


NodePath VariableWatcher::get_variable_storage_path() const{
  return _vstorage_node_path;
}

void VariableWatcher::set_variable_storage_path(const NodePath& path){
  _vstorage_node_path = path;
}


Ref<Texture> VariableWatcher::get_context_menu_button_texture() const{
  return _context_menu_button_texture;
}

void VariableWatcher::set_context_menu_button_texture(Ref<Texture> texture){
  _context_menu_button_texture = texture;
}