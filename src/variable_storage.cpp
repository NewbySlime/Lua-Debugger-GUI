#include "defines.h"
#include "error_trigger.h"
#include "gdutils.h"
#include "logger.h"
#include "signal_ownership.h"
#include "variable_storage.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"

#include "vector"

using namespace gdutils;
using namespace godot;
using namespace lua;


static const char* placeholder_group_name = "placeholder_node";

static const int alias_hint_max_len = 8;
static const char* text_clipping_placement = "...";


void VariableStorage::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_lua_on_stopping"), &VariableStorage::_lua_on_stopping);
  
  ClassDB::bind_method(D_METHOD("_on_context_menu_clicked"), &VariableStorage::_on_context_menu_clicked);
  ClassDB::bind_method(D_METHOD("_on_setter_applied"), &VariableStorage::_on_setter_applied);

  ClassDB::bind_method(D_METHOD("_item_collapsed_safe", "item"), &VariableStorage::_item_collapsed_safe);
  ClassDB::bind_method(D_METHOD("_item_collapsed", "item"), &VariableStorage::_item_collapsed);
  ClassDB::bind_method(D_METHOD("_item_selected"), &VariableStorage::_item_selected);
  ClassDB::bind_method(D_METHOD("_item_nothing_selected"), &VariableStorage::_item_nothing_selected);
  ClassDB::bind_method(D_METHOD("_item_selected_mouse", "mouse_pos", "mouse_idx"), &VariableStorage::_item_selected_mouse);
  ClassDB::bind_method(D_METHOD("_item_empty_clicked", "mouse_pos", "mouse_idx"), &VariableStorage::_item_empty_clicked);
  ClassDB::bind_method(D_METHOD("_item_activated"), &VariableStorage::_item_activated);

  
  ClassDB::bind_method(D_METHOD("set_variable_tree_path", "path"), &VariableStorage::set_variable_tree_path);
  ClassDB::bind_method(D_METHOD("get_variable_tree_path"), &VariableStorage::get_variable_tree_path);

  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "variable_tree_path"), "set_variable_tree_path", "get_variable_tree_path");
}


VariableStorage::~VariableStorage(){
  _clear_variable_tree();
}


void VariableStorage::_lua_on_stopping(){
  std::vector<uint64_t> _removed_list;

  for(auto _pair: _metadata_map){
    switch(_pair.second->value->get_type()){
      break; case I_table_var::get_static_lua_type():{
        I_table_var* _tvar = dynamic_cast<I_table_var*>(_pair.second->value);
        if(!_tvar->is_reference())
          break;

        _removed_list.insert(_removed_list.end(), _pair.first);
      }

      break; case I_function_var::get_static_lua_type():{
        I_function_var* _fvar = dynamic_cast<I_function_var*>(_pair.second->value);
        if(!_fvar->is_reference())
          break;

        _removed_list.insert(_removed_list.end(), _pair.first);
      }
    }
  }

  for(uint64_t _id: _removed_list){
    auto _iter = _metadata_map.find(_id);
    if(_iter == _metadata_map.end())
      continue;

    _delete_tree_item(_iter->second->_this);
  }
}


void VariableStorage::_on_context_menu_clicked(int id){
  _last_context_enum = id;
  switch(id){
    break; case context_menu_change_alias:{
      _on_context_menu_change_alias();
    }

    break; case context_menu_as_copy:{
      _on_context_menu_as_copy();
    }
  }
}

void VariableStorage::_on_context_menu_change_alias(){
  auto _iter = _metadata_map.find(_last_selected_id);
  if(_iter == _metadata_map.end())
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
  auto _iter = _metadata_map.find(_last_selected_id);
  if(_iter == _metadata_map.end())
    return;

  TreeItem* _item = _variable_tree->get_selected();
  _item_metadata* _metadata = _iter->second;
  _update_tree_item(_metadata->_this, NULL, sf_skip_alias_replacement);
}


void VariableStorage::_on_setter_applied(){
  const PopupVariableSetter::VariableData& _output_data = _variable_setter->get_output_data();

  switch(_last_context_enum){
    break; case context_menu_change_alias:{
      String _key = _variable_setter->get_variable_key();
      TreeItem* _item = _variable_tree->get_selected();

      string_var _str = GDSTR_TO_STDSTR(_key);
      _update_tree_item(_item, &_str, sf_skip_checks);
    }
  }
}


void VariableStorage::_add_new_tree_item(const I_variant* var, const I_variant* alias, uint32_t flags, TreeItem* parent_item){
  if(!parent_item)
    parent_item = _root_item;

  TreeItem* _new_item = _create_item(parent_item);
  _item_metadata* _metadata = _metadata_map[_new_item->get_instance_id()];
  _clear_metadata(_metadata);

  _metadata->value = cpplua_create_var_copy(var);
  _update_tree_item(_new_item, alias, flags);
}

void VariableStorage::_reveal_tree_item(TreeItem* parent_item){
  auto _iter = _metadata_map.find(parent_item->get_instance_id());
  if(_iter == _metadata_map.end())
    return;

  _iter->second->already_revealed = true;
  _update_tree_item(parent_item, NULL, sf_skip_alias_replacement | sf_skip_checks);

  parent_item->set_collapsed(false);
}

void VariableStorage::_update_tree_item(TreeItem* item, const I_variant* alias, uint32_t flags){
  auto _iter = _metadata_map.find(item->get_instance_id()); 
  if(_iter == _metadata_map.end())
    return;

  _item_metadata* _metadata = _iter->second;

  // set up value (do checks)
  if(!(flags & sf_skip_checks)){
    switch(_metadata->value->get_type()){
      break; case I_table_var::get_static_lua_type():{
        I_table_var* _tvar = dynamic_cast<I_table_var*>(_metadata->value);
        if((flags & sf_store_as_reference) == 0)
          _tvar->as_copy();

        _delete_tree_item_child(item);
        if(!_metadata->already_revealed){
          item->create_child();
          item->set_collapsed(true);
          break;
        }

        _update_tree_item_child(item);
      }

      break; case I_function_var::get_static_lua_type():{
        I_function_var* _fvar = dynamic_cast<I_function_var*>(_metadata->value);
        if((flags & sf_store_as_reference) == 0)
          _fvar->as_copy();
      }
    }
  }

  // get flags checks
  bool _value_is_ref = false;
  switch(_metadata->value->get_type()){
    break; case I_table_var::get_static_lua_type():{
      I_table_var* _tvar = dynamic_cast<I_table_var*>(_metadata->value);
      _value_is_ref = _tvar->is_reference();
    }

    break; case I_function_var::get_static_lua_type():{
      I_function_var* _fvar = dynamic_cast<I_function_var*>(_metadata->value);
      _value_is_ref = _fvar->is_reference();
    }
  }

  string_store _str;
  if(flags & sf_skip_alias_replacement){
    String _tooltip = item->get_tooltip_text(0);
    _str.data += GDSTR_TO_STDSTR(_tooltip);
  }
  else if(alias)
    alias->to_string(&_str);

  if(_str.data.size() > alias_hint_max_len){
    _str.data = _str.data.substr(0, alias_hint_max_len);
    _str.data += text_clipping_placement;
  }

  _str.data = "\"" + _str.data + "\"";
  _str.data += " : ";

  if(_value_is_ref)
    _str.data += "(Ref) ";

  _metadata->value->to_string(&_str);
  item->set_text(0, _str.data.c_str());

  // set up tooltip
  if(alias){
    string_store _str; alias->to_string(&_str);
    item->set_tooltip_text(0, _str.data.c_str());
  }
}

void VariableStorage::_update_tree_item_child(TreeItem* item){
  auto _iter = _metadata_map.find(item->get_instance_id());
  if(_iter == _metadata_map.end())
    return;

  _item_metadata* _metadata = _iter->second;
  switch(_metadata->value->get_type()){
    break; case I_table_var::get_static_lua_type():{
      I_table_var* _tvar = dynamic_cast<I_table_var*>(_metadata->value);
      _tvar->update_keys();

      const I_variant** _key_list = _tvar->get_keys();
      for(int i = 0; _key_list[i]; i++){
        const I_variant* _key = _key_list[i];
        const I_variant* _value = _tvar->get_value(_key);

        _add_new_tree_item(_value, _key, 0, item);

        cpplua_delete_variant(_value);
      }
    }
  }
}

void VariableStorage::_update_placeholder_state(){
  if(!_ginvoker)
    return;

  _ginvoker->invoke(placeholder_group_name, "set_visible", _root_item->get_child_count() <= 0);
}


void VariableStorage::_open_context_menu(){
  PopupContextMenu::MenuData _menu_data;
  PopupContextMenu::MenuData::Part _tmp_part;

    _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
    _tmp_part.label = "Change Alias/Note";
    _tmp_part.id = context_menu_change_alias;
  _menu_data.part_list.insert(_menu_data.part_list.begin(), _tmp_part);

  auto _iter = _metadata_map.find(_last_selected_id);
  if(_iter != _metadata_map.end()){
    bool _is_value_reference = false;
    switch(_iter->second->value->get_type()){
      break; case I_table_var::get_static_lua_type():{
        I_table_var* _tvar = dynamic_cast<I_table_var*>(_iter->second->value);
        _is_value_reference = _tvar->is_reference();
      }

      break; case I_function_var::get_static_lua_type():{
        I_function_var* _fvar = dynamic_cast<I_function_var*>(_iter->second->value);
        _is_value_reference = _fvar->is_reference();
      }
    }

    if(_is_value_reference){
        _tmp_part.item_type = PopupContextMenu::MenuData::type_normal;
        _tmp_part.label = "To Copy";
        _tmp_part.id = context_menu_as_copy;
      _menu_data.part_list.insert(_menu_data.part_list.begin(), _tmp_part);
    }
  }

  _context_menu->init_menu(_menu_data);

  SignalOwnership(Signal(_context_menu, "id_pressed"), Callable(this, "_on_context_menu_clicked"))
    .replace_ownership();

  Vector2 _mouse_pos = get_tree()->get_root()->get_mouse_position();
  _context_menu->set_position(_mouse_pos);
  _context_menu->popup();
}


void VariableStorage::_item_collapsed_safe(TreeItem* item){
  _safe_callable_list.insert(_safe_callable_list.end(), Callable(this, "_item_collapsed").bind(item));
}

void VariableStorage::_item_collapsed(TreeItem* item){
  if(item->is_collapsed())
    return;

  auto _iter = _metadata_map.find(item->get_instance_id());
  if(_iter == _metadata_map.end() || _iter->second->already_revealed)
    return;

  if(_iter->second->value->get_type() != I_table_var::get_static_lua_type())
    return;

  _reveal_tree_item(item);
}

void VariableStorage::_item_selected(){
  _last_selected_id = _variable_tree->get_selected()->get_instance_id();
}

void VariableStorage::_item_nothing_selected(){
  _last_selected_id = 0;
}

void VariableStorage::_item_selected_mouse(const Vector2 mouse_pos, int mouse_idx){
  TreeItem* _selected_item = _variable_tree->get_selected();
  if(!_selected_item)
    return;

  auto _iter = _metadata_map.find(_selected_item->get_instance_id());
  if(_iter == _metadata_map.end())
    return;

  switch(mouse_idx){
    break; case MOUSE_BUTTON_RIGHT:{
      _open_context_menu();
    }
  }
}

void VariableStorage::_item_empty_clicked(const Vector2 mouse_pos, int mouse_idx){
  _last_selected_id = 0;
}

void VariableStorage::_item_activated(){
  _last_selected_id = _variable_tree->get_selected()->get_instance_id();
}


VariableStorage::_item_metadata* VariableStorage::_create_metadata(TreeItem* item){
  _item_metadata* _result = new _item_metadata();
  _result->_this = item;

  return _result;
}

TreeItem* VariableStorage::_create_item(TreeItem* parent_item){
  TreeItem* _result = _variable_tree->create_item(parent_item, 0);
  _item_metadata* _metadata = _create_metadata(_result);
  _metadata_map[_result->get_instance_id()] = _metadata;

  _update_placeholder_state();
  return _result;
}


void VariableStorage::_delete_metadata(_item_metadata* metadata){
  _clear_metadata(metadata);
  delete metadata;
}

void VariableStorage::_delete_tree_item(TreeItem* item){
  _delete_tree_item_child(item);
  
  auto _iter = _metadata_map.find(item->get_instance_id());
  if(_iter != _metadata_map.end()){
    _delete_metadata(_iter->second);
    _metadata_map.erase(_iter);
  }

  TreeItem* _parent_item = item->get_parent();
  if(_parent_item)
    _parent_item->remove_child(item);

  _update_placeholder_state();
}

void VariableStorage::_delete_tree_item_child(TreeItem* item){
  while(item->get_child_count() > 0){
    TreeItem* _current_item = item->get_child(0);
    _delete_tree_item(_current_item);
  }
}


void VariableStorage::_clear_metadata(_item_metadata* metadata){
  if(metadata->value){
    cpplua_delete_variant(metadata->value);
    metadata->value = NULL;
  }
}

void VariableStorage::_clear_metadata_map(){
  for(auto _pair: _metadata_map)
    _delete_metadata(_pair.second);

  _metadata_map.clear();
}

void VariableStorage::_clear_variable_tree(){
  if(_variable_tree)
    _variable_tree->clear();

  _clear_metadata_map();
}


void VariableStorage::_ready(){
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

  _variable_tree = get_node<Tree>(_variable_tree_path);
  if(!_variable_tree){
    GameUtils::Logger::print_err_static("[VariableWatcher] Cannot get Tree for Variable Viewer.");
  
    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _gvariables = get_node<GlobalVariables>(GlobalVariables::singleton_path);
  if(!_gvariables){
    GameUtils::Logger::print_err_static("[VariableWatcher] Cannot get Global Variables node.");

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

  _variable_tree->connect("item_collapsed", Callable(this, "_item_collapsed_safe"));
  _variable_tree->connect("item_selected", Callable(this, "_item_selected"));
  _variable_tree->connect("nothing_selected", Callable(this, "_item_nothing_selected"));
  _variable_tree->connect("item_mouse_selected", Callable(this, "_item_selected_mouse"));
  _variable_tree->connect("empty_clicked", Callable(this, "_item_empty_clicked"));
  _variable_tree->connect("item_activated", Callable(this, "_item_activated"));

  _root_item = _variable_tree->create_item();
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
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;
  
  if(_safe_callable_list.size() > 0){
    for(const Callable& cb: _safe_callable_list)
      cb.call();

    _safe_callable_list.clear();
  }
}


void VariableStorage::add_to_storage(const I_variant* var, const I_variant* alias, uint32_t flags){
  _add_new_tree_item(var, alias, flags);
}


void VariableStorage::set_variable_tree_path(const NodePath& path){
  _variable_tree_path = path;
}

NodePath VariableStorage::get_variable_tree_path() const{
  return _variable_tree_path;
}


void VariableStorage::set_context_menu_button_icon(Ref<Texture> image){
  _context_menu_button_icon = image;
}

Ref<Texture> VariableStorage::get_context_menu_button_icon() const{
  return _context_menu_button_icon;
}