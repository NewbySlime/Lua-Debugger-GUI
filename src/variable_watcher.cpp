#include "custom_variant.h"
#include "directory_util.h"
#include "defines.h"
#include "error_trigger.h"
#include "gdstring_store.h"
#include "gdvariant_util.h"
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
  ClassDB::bind_method(D_METHOD("_on_setter_applied_add_table_confirmed_variant"), &VariableWatcher::_on_setter_applied_add_table_confirmed_variant);
  ClassDB::bind_method(D_METHOD("_on_setter_applied_add_table_cancelled_variant"), &VariableWatcher::_on_setter_applied_add_table_cancelled_variant);
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
  _clear_item_state(&_local_item_state_tree);
  _clear_item_state(&_global_item_state_tree);
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

}

void VariableWatcher::_lua_on_pausing(){
  _update_variable_tree();
  // local item state will not be cleared as it will retain data about each functions.
  // _clear_item_state(&_local_item_state_tree);
  _clear_item_state(&_global_item_state_tree);
}

void VariableWatcher::_lua_on_resuming(){
  _update_item_state_tree();
  _clear_variable_tree();
}

void VariableWatcher::_lua_on_stopping(){
  _clear_item_state(&_local_item_state_tree);
  _clear_item_state(&_global_item_state_tree);
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

  _reveal_tree_item(item, NULL);
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
      _on_setter_applied_add_or_copy(_current_item, _key_var, _value_var);

    break; case context_menu_add_table:
      _on_setter_applied_add_table(_current_item, _key_var, _value_var);

    break; case context_menu_edit:
      _on_setter_applied_edit(_current_item, _key_var, _value_var);
  }
} // enclosure closing

  skip_to_cleaning:{}

  if(_key_var)
    cpplua_delete_variant(_key_var);

  if(_value_var)
    cpplua_delete_variant(_value_var);
}

void VariableWatcher::_on_setter_applied_add_or_copy(TreeItem* current_item, I_variant* key_var, I_variant* value_var){
  TreeItem* _parent_item = current_item->get_parent();
  if(!_parent_item)
    return;

  _on_setter_applied_add_table(_parent_item, key_var, value_var);
}


struct _setter_applied_add_table_data{
  uint64_t item_id;
  I_variant* key_var;
  I_variant* value_var;
};

void VariableWatcher::_on_setter_applied_add_table(TreeItem* parent_item, I_variant* key_var, I_variant* value_var){
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

void VariableWatcher::_on_setter_applied_add_table_confirmed_variant(const Variant& data){
  _setter_applied_add_table_data _data = parse_variant_data<_setter_applied_add_table_data>(data);
  auto _iter = _vartree_map.find(_data.item_id);
  if(_iter == _vartree_map.end())
  return;
  
  _on_setter_applied_add_table_confirmed(_iter->second, _data.key_var, _data.value_var);
  
  // delete _data values
  cpplua_delete_variant(_data.key_var);
  cpplua_delete_variant(_data.value_var);
}

void VariableWatcher::_on_setter_applied_add_table_cancelled_variant(const Variant& data){
  // delete values
  _setter_applied_add_table_data _data = parse_variant_data<_setter_applied_add_table_data>(data);
  cpplua_delete_variant(_data.key_var);
  cpplua_delete_variant(_data.value_var);
}

void VariableWatcher::_on_setter_applied_add_table_confirmed(_variable_tree_item_metadata* metadata, I_variant* key_var, I_variant* value_var){
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
  }
}

void VariableWatcher::_on_setter_applied_edit(TreeItem* current_item, I_variant* key_var, I_variant* value_var){
  // will handle adding the value to the table
  _set_tree_item_key(current_item, key_var);
  _set_tree_item_value(current_item, value_var);

  _update_tree_item(current_item, NULL);
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

      if(_iter->second->this_value->get_type() == I_local_table_var::get_static_lua_type())
        _edit_flag |= PopupVariableSetter::edit_local_key;

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

  bool _do_popup = false;

  _popup_variable_setter->set_variable_key("");
  if((flag & PopupVariableSetter::edit_add_key_edit) && _iter->second->this_key){
    string_store _str; _iter->second->this_key->to_string(&_str);
    _popup_variable_setter->set_variable_key(_str.data.c_str());
  }

  if((flag & PopupVariableSetter::edit_add_key_edit) && (flag & PopupVariableSetter::edit_local_key)){
{ // enclosure for using goto
    PackedStringArray _choices;
    
    auto _local_iter = _vartree_map.find(_local_item->get_instance_id());
    if(_local_iter == _vartree_map.end())
      goto skip_local_block;

    if(!_local_iter->second->this_value->is_type(I_local_table_var::get_static_lua_type()))
      goto skip_local_block;
      
    I_table_var* _tvar = dynamic_cast<I_table_var*>(_local_iter->second->this_value);
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
        GameUtils::Logger::print_warn_static(gd_format_str("[VariableWatcher] Local variable '{0}' cannot be used for add/copy.", _str.data.c_str()));
        continue;
      }
      
      _choices.append(_str.data.c_str());

      _tvar->free_variant(_value_var);
    }

    _do_popup = _choices.size() > 0;
    if(!_do_popup){
      GameUtils::Logger::print_err_static("[VariableWatcher] No empty local variables to add.");
      goto skip_local_block;
    }

    _popup_variable_setter->set_local_key_choice(_choices);

} // enclosure closing

    skip_local_block:{}
  }
  else
    _do_popup = true;

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


void VariableWatcher::_open_context_menu(){
  auto _iter = _vartree_map.find(_last_selected_id);
  if(_iter == _vartree_map.end())
    return;

  bool _value_can_be_ref = false;
  bool _cannot_be_stored = false;
  if(_iter->second->this_value){
    switch(_iter->second->this_value->get_type()){
      break;
      case I_table_var::get_static_lua_type():
      case I_function_var::get_static_lua_type():
        _value_can_be_ref = true;

      break;
      case I_local_table_var::get_static_lua_type():
        _cannot_be_stored = true;
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
  
  // add menu for adding to storage
  if(!_cannot_be_stored && _iter->second->this_value){
    // create separator only when there's any item
    if(_data.part_list.size() > 0){
        _tmp_part.item_type = PopupContextMenu::MenuData::type_separator;
        _tmp_part.label = "";
        _tmp_part.id = -1;
      _data.part_list.insert(_data.part_list.end(), _tmp_part);
    }

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

  SignalOwnership(Signal(_global_context_menu, "id_pressed"), Callable(this, "_on_context_menu_clicked"))
    .replace_ownership();

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

  I_thread_handle_reference* _main_thread_ref = _lua_program_handle->get_main_thread();
  core _main_lc;
    _main_lc.istate = _main_thread_ref->get_interface()->get_lua_state_interface();
    _main_lc.context = _lua_lib_data->get_function_data()->get_cc();

  I_execution_flow* _execution_flow = _lua_program_handle->get_execution_flow();
  
  std::string _file_name = DirectoryUtil::strip_path(_lua_program_handle->get_current_running_file());
  TreeItem* _file_item = _variable_tree->create_item();
  _file_item->set_text(0, _file_name.c_str());

  // update local variables
  _local_item = _create_tree_item(_file_item);
  _variable_tree_item_metadata* _lmetadata = _vartree_map[_local_item->get_instance_id()];
  _lmetadata->already_revealed = true;
  
  string_var _func_name = _lua_program_handle->get_current_function();
  local_table_var _local_data(&_main_lc, 0);
  _set_tree_item_key(_local_item, &_func_name);
  _set_tree_item_value(_local_item, &_local_data);

  auto _state_iter = _local_item_state_tree.branches.find(_func_name);
  _item_state* _local_function_state = _state_iter != _local_item_state_tree.branches.end()? _state_iter->second: NULL;
  
  _update_tree_item(_local_item, _local_function_state);
  
  // update global variables
  _global_item = _create_tree_item(_file_item);
  _variable_tree_item_metadata* _gmetadata = _vartree_map[_global_item->get_instance_id()];
  _gmetadata->already_revealed = true;
  _global_item_state_tree.is_revealed = true;
  
  string_var _global_name = "Global";
  _lc.context->api_value->pushglobaltable(_lc.istate);
  I_variant* _global_table_var = _lc.context->api_varutil->to_variant(_lc.istate, -1);
  _lc.context->api_stack->pop(_lc.istate, 1);
  _set_tree_item_key(_global_item, &_global_name);
  _set_tree_item_value(_global_item, _global_table_var);  
  
  _update_tree_item(_global_item, &_global_item_state_tree);
  
  cpplua_delete_variant(_global_table_var);
} // enclosure closing

  skip_to_return:{}
  _lua_program_handle->unlock_object();
}

void VariableWatcher::_update_tree_item(TreeItem* item, _item_state* state){
  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end())
    return;

  _delete_tree_item_child(item);

  const char* _prefix_string = "";
  const char* _suffix_string = "";

  // prefix and suffix changes
  switch(_iter->second->this_value->get_type()){
    break; case lua::string_var::get_static_lua_type():{
      _prefix_string = "\"";
      _suffix_string = "\"";
    }
  }

  string_store _key_str; _iter->second->this_key->to_string(&_key_str);
  string_store _value_str; _iter->second->this_value->to_string(&_value_str);
  std::string _format_val_str = format_str("%s: %s%s%s",
    _key_str.data.c_str(),
    _prefix_string,
    _value_str.data.c_str(),
    _suffix_string
  );

  item->set_text(0, _format_val_str.c_str());
  
  if(_iter->second->this_value->is_type(I_table_var::get_static_lua_type())){
    bool _reveal_item = false;
    if(state)
      _reveal_item = state->is_revealed;
    else
      _reveal_item = _iter->second->already_revealed;

    if(_reveal_item)
      _reveal_tree_item(item, state);
    else{
      _variable_tree->create_item(item);
      item->set_collapsed(true);
    }
  }

  _sort_item_child(item->get_parent());
}


void VariableWatcher::_set_tree_item_key(TreeItem* item, const I_variant* key){
  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end())
    return;
  
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
    _iter->second->this_key = cpplua_create_var_copy(key);
}

void VariableWatcher::_set_tree_item_value(TreeItem* item, const I_variant* value){
  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end())
    return;

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
} // enclosure closing
    skip_value_set:{}
} // enclosure closing
    skip_parent_checking:{}
  }

  if(_iter->second->this_value){
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
  
  if(value)
    _iter->second->this_value = cpplua_create_var_copy(value);
}


void VariableWatcher::_reveal_tree_item(TreeItem* item, _item_state* state){
  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end())
    return;

  if(!_iter->second->this_value->is_type(I_table_var::get_static_lua_type()))
    return;

  bool _is_local_item = _iter->second->this_value->get_type() == I_local_table_var::get_static_lua_type();
  
  _delete_tree_item_child(item);
  _iter->second->already_revealed = true;

  const core _lc = _lua_program_handle->get_runtime_handler()->get_lua_core_copy();
  I_variable_watcher* _vw = _lua_program_handle->get_variable_watcher();

  I_table_var* _tvar = dynamic_cast<I_table_var*>(_iter->second->this_value);  
  _tvar->update_keys();
  const I_variant** _keys_list = _tvar->get_keys();

  // query the data first, _set_tree_item will mutate the parent table values
  struct _query_key_value{
    I_variant* key;
    I_variant* value;
  };

  std::vector<_query_key_value> _queried_list;
  for(int i = 0; _keys_list[i]; i++){
    const I_variant* _key_data = _keys_list[i];
    auto _iter = _filter_key.find(_key_data);
    if(_iter != _filter_key.end())
      continue;
    
    // check internal variables
    if(item == _global_item && _ignore_internal_variables && _vw->is_internal_variables(_key_data))
      continue;

    I_variant* _value_data = _tvar->get_value(_key_data);

    _query_key_value _query;
      _query.key = cpplua_create_var_copy(_key_data);
      _query.value = cpplua_create_var_copy(_value_data);

    _queried_list.insert(_queried_list.end(), _query);
  }
  
  for(_query_key_value& _query: _queried_list){
{ // enclosure for using gotos
    // skip if nil value (deleted)
    if(_query.value->get_type() == I_nil_var::get_static_lua_type())
      goto skip_adding_child;

    TreeItem* _child_item = _create_tree_item(item);
    _variable_tree_item_metadata* _metadata = _vartree_map[_child_item->get_instance_id()];
      _metadata->_mflag |= metadata_valid_mutable_item;
      _metadata->_mflag |= _is_local_item? metadata_local_item: 0;
    
    _set_tree_item_key(_child_item, _query.key);
    _set_tree_item_value(_child_item, _query.value);

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
    cpplua_delete_variant(_query.value);
  }

  _queried_list.clear();

  item->set_collapsed(false);
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


void VariableWatcher::_update_placeholder_state(){
  if(!_ginvoker)
    return;

  _ginvoker->invoke(placeholder_group_name, "set_visible", _variable_tree->get_root() == NULL);
}


TreeItem* VariableWatcher::_create_tree_item(TreeItem* parent_item){
  TreeItem* _result = _variable_tree->create_item(parent_item);
  _vartree_map[_result->get_instance_id()] = _create_vartree_metadata(_result);

  _result->add_button(0, _context_menu_button_texture, button_id_context_menu);

  _update_placeholder_state();
  return _result;
}

VariableWatcher::_variable_tree_item_metadata* VariableWatcher::_create_vartree_metadata(TreeItem* item){
  _variable_tree_item_metadata* _result = new _variable_tree_item_metadata();
  _result->parent_item = item->get_parent();
  _result->this_item = item;
  _result->this_key = NULL;
  _result->this_value = NULL;

  return _result;
}


void VariableWatcher::_remove_item(TreeItem* item){
  auto _iter = _vartree_map.find(item->get_instance_id());
  if(_iter == _vartree_map.end())
    return;

  _variable_tree_item_metadata* _parent_metadata = NULL;

  TreeItem* _parent_item = item->get_parent();
  if(_parent_item){
{ // enclosure for using gotos
    auto _parent_iter = _vartree_map.find(_parent_item->get_instance_id());
    if(_parent_iter == _vartree_map.end())
      goto skip_getting_parent_metadata;

    _parent_metadata = _parent_iter->second;
} // enclosure closing

    skip_getting_parent_metadata:{}
  }

  if(_parent_metadata && _parent_metadata->this_value->is_type(I_table_var::get_static_lua_type())){
    I_table_var* _tvar = dynamic_cast<I_table_var*>(_parent_metadata->this_value);

    nil_var _nvar;
    _tvar->set_value(_iter->second->this_key, &_nvar);
  }

  _delete_tree_item_child(item);
  _delete_tree_item(item);
}


void VariableWatcher::_update_item_state_tree(){
{ // enclosure for using gotos
  auto _local_iter = _vartree_map.find(_local_item->get_instance_id());
  if(_local_iter == _vartree_map.end() || !_local_iter->second->this_key)
    goto skip_local_function_checking;

  _item_state* _local_function_state;
  auto _state_iter = _local_item_state_tree.branches.find(_local_iter->second->this_key);
  if(_state_iter != _local_item_state_tree.branches.end())
    _local_function_state = _state_iter->second;
  else{
    _local_function_state = new _item_state();
      _local_function_state->is_revealed = true;
    _local_item_state_tree.branches[_local_iter->second->this_key] = _local_function_state;
  }

  _clear_item_state(_local_function_state);
  _update_item_state_tree(_local_item, _local_function_state);
} // enclosure closing
  skip_local_function_checking:{}
  
  _clear_item_state(&_global_item_state_tree);
  _update_item_state_tree(_global_item, &_global_item_state_tree);
}

void VariableWatcher::_update_item_state_tree(TreeItem* parent_item, _item_state* parent_state){
  for(int i = 0; i < parent_item->get_child_count(); i++){
    TreeItem* _child_item = parent_item->get_child(i);
    auto _child_iter = _vartree_map.find(_child_item->get_instance_id());
    if(_child_iter == _vartree_map.end() || !_child_iter->second->this_key)
      continue;

    _item_state* _new_state = new _item_state();
      _new_state->is_revealed = !_child_item->is_collapsed();

    _update_item_state_tree(_child_item, _new_state);

    parent_state->branches[_child_iter->second->this_key] = _new_state;
  }
}

void VariableWatcher::_clear_item_state(_item_state* node){
  for(auto _pair: node->branches)
    _delete_item_state(_pair.second);

  node->branches.clear();
}

void VariableWatcher::_delete_item_state(_item_state* node){
  _clear_item_state(node);
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
}

void VariableWatcher::_clear_variable_metadata_map(){
  for(auto _pair: _vartree_map)
    _delete_variable_metadata(_pair.second);

  _vartree_map.clear();
}

void VariableWatcher::_clear_variable_metadata(_variable_tree_item_metadata* metadata){
  _set_tree_item_key(metadata->this_item, NULL);
  _set_tree_item_value(metadata->this_item, NULL);

  if(metadata->child_lookup_list){
    delete metadata->child_lookup_list;
    metadata->child_lookup_list;
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

  NodePath _confirmation_dialog_path = _gvariables->get_global_value(GlobalVariables::key_confirmation_dialog_path);
  _global_confirmation_dialog = get_node<ConfirmationDialog>(_confirmation_dialog_path);
  if(!_global_confirmation_dialog)
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