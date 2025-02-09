#include "code_context.h"
#include "common_event.h"
#include "error_trigger.h"
#include "logger.h"
#include "strutil.h"

#include "Lua-CPPAPI/Src/luaapi_compilation_context.h"
#include "Lua-CPPAPI/Src/luaapi_debug.h"
#include "Lua-CPPAPI/Src/luadebug_file_info.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/math.hpp"

#include "algorithm"

#pragma comment(lib, "comdlg32.lib")

using namespace godot;
using namespace lua;
using namespace lua::api;
using namespace lua::debug;


const char* CodeContext::s_file_loaded = "file_loaded";
const char* CodeContext::s_cannot_load = "cannot_load";
const char* CodeContext::s_breakpoint_added = "breakpoint_added";
const char* CodeContext::s_breakpoint_removed = "breakpoint_removed";


void CodeContext::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_breakpoint_toggled_cb", "line"), &CodeContext::_breakpoint_toggled_cb);

  ClassDB::bind_method(D_METHOD("set_code_edit_path", "path"), &CodeContext::set_code_edit_path);
  ClassDB::bind_method(D_METHOD("get_code_edit_path"), &CodeContext::get_code_edit_path);
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "code_edit_node_path"), "set_code_edit_path", "get_code_edit_path");


  ADD_SIGNAL(MethodInfo(s_file_loaded, PropertyInfo(Variant::STRING, "file_path")));
  ADD_SIGNAL(MethodInfo(s_cannot_load, PropertyInfo(Variant::STRING, "file_path"), PropertyInfo(Variant::INT, "error_code")));
  ADD_SIGNAL(MethodInfo(s_breakpoint_added, PropertyInfo(Variant::INT, "line"), PropertyInfo(Variant::INT, "id")));
  ADD_SIGNAL(MethodInfo(s_breakpoint_removed, PropertyInfo(Variant::INT, "line"), PropertyInfo(Variant::INT, "id")));
  ADD_SIGNAL(MethodInfo(SIGNAL_ON_READY, PropertyInfo(Variant::OBJECT, "this_obj")));
}


CodeContext::CodeContext(){
  
}

CodeContext::~CodeContext(){
  
}


void CodeContext::_breakpoint_toggled_cb(int line){
  if(_skip_breakpoint_toggled_event)
    return;

  if(_code_edit->is_line_breakpointed(line)){
    long _check_line = _check_valid_line(line);
    if(_check_line < 0){
      _skip_breakpoint_toggled_event = true;
      _code_edit->set_line_as_breakpoint(line, false);
      _skip_breakpoint_toggled_event = false;
      return;
    }

    if(_check_line != line){
      _skip_breakpoint_toggled_event = true;
      _code_edit->set_line_as_breakpoint(line, false);
      _code_edit->set_line_as_breakpoint(_check_line, true);
      _skip_breakpoint_toggled_event = false;
    }

    _breakpointed_list.insert(_breakpointed_list.end(), _check_line);
    std::sort(_breakpointed_list.begin(), _breakpointed_list.end());

    emit_signal(s_breakpoint_added, Variant(line), Variant(get_instance_id()));
  }
  else{
    auto _iter = std::search_n(_breakpointed_list.begin(), _breakpointed_list.end(), 1, line);
    if(_iter != _breakpointed_list.end())
      _breakpointed_list.erase(_iter);

    emit_signal(s_breakpoint_removed, Variant(line), Variant(get_instance_id()));
  }
}


long CodeContext::_check_valid_line(long check_line){
  long _res_line = -1;
  for(int i = 0; i < _valid_lines.size(); i++){
    long _curr_line = _valid_lines[i];
    if(_curr_line < check_line)
      _res_line = _curr_line;
    
    if(_curr_line >= check_line){
      _res_line = _curr_line;
      break;
    }
  }

  return _res_line;
}


void CodeContext::_load_file_check(bool retain_breakpoints){
  if(!_initialized || _current_file_path.size() <= 0)
    return;

  godot::Error _result = godot::OK;

{ // enclosure for using gotos
  Ref<FileAccess> _file_access = FileAccess::open(_current_file_path.c_str(), FileAccess::READ);
  if(_file_access.ptr() == NULL){
    _result = FileAccess::get_open_error();
    goto skip_to_return;
  }

  std::vector<int> _bp_lines = _breakpointed_list;
  clear_breakpoints();
  clear_executing_lines();

  _code_edit->set_text(_file_access->get_as_text());
  _file_access->close();
  
  _valid_lines.clear();
  if(!_lib_store.get()){
    GameUtils::Logger::print_warn_static("[CodeContext] LibLuaStore not loaded. Skipping file debug info.");
    goto skip_to_return;
  }

  const compilation_context* _func_data = _lib_store->get_function_data()->get_cc();
  I_file_info* _fi = _func_data->api_debug->create_file_info(_current_file_path.c_str());
  if(_fi->get_last_error()){
    string_store _err_obj_str; _fi->get_last_error()->to_string(&_err_obj_str);
    std::string _err_msg = format_str("[CodeContext] Cannot load debug info. Reason: %s", _err_obj_str.data.c_str());
    GameUtils::Logger::print_warn_static(_err_msg.c_str());
    goto skip_to_return;
  }

  for(int i = 0; i < _fi->get_line_count(); i++){
    if(!_fi->is_line_valid(i))
      continue;

    _valid_lines.insert(_valid_lines.begin(), i);
  }

  std::sort(_valid_lines.begin(), _valid_lines.end());

  if(retain_breakpoints){
    for(auto _iter: _bp_lines)
      _code_edit->set_line_as_breakpoint(_iter, true);
  }
} // enclosure closure

  skip_to_return:{}
  if(_result == godot::OK)
    emit_signal(s_file_loaded, String(_current_file_path.c_str()));
  else
    emit_signal(s_cannot_load, String(_current_file_path.c_str()), (int)_result);
}


void CodeContext::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

{ // enclosure for using gotos
  _code_edit = get_node<CodeEdit>(_code_edit_path);
  if(!_code_edit){
    GameUtils::Logger::print_err_static("[CodeContext] Cannot get CodeEdit node.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _code_edit->set_text("");
  _code_edit->connect("breakpoint_toggled", Callable(this, "_breakpoint_toggled_cb"));


  LibLuaHandle* _lib_handle = get_node<LibLuaHandle>("/root/GlobalLibLuaHandle");
  if(!_lib_handle){
    GameUtils::Logger::print_err_static("[CodeContext] Cannot get LibLuaHandle for library information.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _lib_store = _lib_handle->get_library_store();

  _initialized = true;
  _load_file_check();
  emit_signal(SIGNAL_ON_READY, this);
} // enclosure closing

  return;

  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  return;}
}


void CodeContext::load_file(const std::string& file_path){
  bool _retain_bp = _current_file_path == file_path;
  _current_file_path = file_path;
  _load_file_check(_retain_bp);
}

void CodeContext::reload_file(){
  _load_file_check(true);
}

std::string CodeContext::get_current_file_path() const{
  return _current_file_path;
}


String CodeContext::get_line_at(int line) const{
  return _code_edit->get_line(line);
}

int CodeContext::get_line_count() const{
  return _code_edit->get_line_count();
}


void CodeContext::focus_at_line(int line){
  if(line < 0)
    return;

  _code_edit->set_v_scroll(line - 3);
}


void CodeContext::set_breakpoint_line(int line, bool flag){
  if(_code_edit->is_line_breakpointed(line) == flag)
    return;

  _code_edit->set_line_as_breakpoint(line, flag);
}

void CodeContext::clear_breakpoints(){
  PackedInt32Array _int_array = _code_edit->get_breakpointed_lines();
  for(int i = 0; i < _int_array.size(); i++)
    set_breakpoint_line(_int_array[i], false);
}


int CodeContext::get_breakpoint_line(int idx){
  if(idx < 0 || _breakpointed_list.size() >= idx)
    return -1;

  return _breakpointed_list[idx];
}

long CodeContext::get_breakpoint_counts(){
  return _breakpointed_list.size();
}

const std::vector<int>* CodeContext::get_breakpoint_list(){
  return &_breakpointed_list;
}


void CodeContext::set_executing_line(int line, bool flag){
  // ???
  _code_edit->set_line_as_executing(line-1, flag);
}

void CodeContext::clear_executing_lines(){
  _code_edit->clear_executing_lines();
}


void CodeContext::set_code_edit_path(NodePath path){
  _code_edit_path = path;
}

NodePath CodeContext::get_code_edit_path() const{
  return _code_edit_path;
}


bool CodeContext::is_initialized() const{
  return _initialized;
}