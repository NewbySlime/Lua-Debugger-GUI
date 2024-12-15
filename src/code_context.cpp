#include "code_context.h"
#include "error_trigger.h"
#include "logger.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/math.hpp"

#include "algorithm"

#pragma comment(lib, "comdlg32.lib")

using namespace godot;


void CodeContext::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_breakpoint_toggled_cb", "line"), &CodeContext::_breakpoint_toggled_cb);

  ClassDB::bind_method(D_METHOD("set_code_edit_path", "path"), &CodeContext::set_code_edit_path);
  ClassDB::bind_method(D_METHOD("get_code_edit_path"), &CodeContext::get_code_edit_path);
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "code_edit_node_path"), "set_code_edit_path", "get_code_edit_path");


  ADD_SIGNAL(MethodInfo(SIGNAL_CODE_CONTEXT_FILE_LOADED, PropertyInfo(Variant::STRING, "file_path")));
  ADD_SIGNAL(MethodInfo(SIGNAL_CODE_CONTEXT_BREAKPOINT_ADDED, PropertyInfo(Variant::INT, "line"), PropertyInfo(Variant::INT, "id")));
  ADD_SIGNAL(MethodInfo(SIGNAL_CODE_CONTEXT_BREAKPOINT_REMOVED, PropertyInfo(Variant::INT, "line"), PropertyInfo(Variant::INT, "id")));
}


CodeContext::CodeContext(){
  
}

CodeContext::~CodeContext(){

}


void CodeContext::_breakpoint_toggled_cb(int line){
  if(_code_edit->is_line_breakpointed(line)){
    _breakpointed_list.insert(_breakpointed_list.end(), line);
    std::sort(_breakpointed_list.begin(), _breakpointed_list.end());

    emit_signal(SIGNAL_CODE_CONTEXT_BREAKPOINT_ADDED, Variant(line), Variant(get_instance_id()));
  }
  else{
    auto _iter = std::search_n(_breakpointed_list.begin(), _breakpointed_list.end(), 1, line);
    if(_iter != _breakpointed_list.end())
      _breakpointed_list.erase(_iter);

    emit_signal(SIGNAL_CODE_CONTEXT_BREAKPOINT_REMOVED, Variant(line), Variant(get_instance_id()));
  }
}


void CodeContext::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

  _code_edit = get_node<CodeEdit>(_code_edit_path);
  if(!_code_edit){
    GameUtils::Logger::print_err_static("[CodeContext] Cannot get CodeEdit node.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _code_edit->set_text(_tmp_file_data); _tmp_file_data = "";
  _code_edit->connect("breakpoint_toggled", Callable(this, "_breakpoint_toggled_cb"));

  _initialized = true;
  return;


  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  return;}
}


godot::Error CodeContext::load_file(const std::string& file_path){
  _current_file_path = file_path;

  Ref<FileAccess> _file_access = FileAccess::open(file_path.c_str(), FileAccess::READ);
  if(_file_access.ptr() == NULL)
    return FileAccess::get_open_error();

  if(_initialized)
    _code_edit->set_text(_file_access->get_as_text());
  else
    _tmp_file_data = _file_access->get_as_text();

  _file_access->close();

  emit_signal(SIGNAL_CODE_CONTEXT_FILE_LOADED, String(_current_file_path.c_str()));
  return godot::OK;
}

godot::Error CodeContext::reload_file(){
  return load_file(_current_file_path);
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