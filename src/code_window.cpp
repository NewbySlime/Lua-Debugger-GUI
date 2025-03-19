#include "defines.h"

#include "code_window.h"
#include "common_event.h"
#include "directory_util.h"
#include "error_trigger.h"
#include "logger.h"
#include "node_utils.h"
#include "strutil.h"


#include "Lua-CPPAPI/Src/luaapi_thread.h"
#include "Lua-CPPAPI/Src/luaapi_debug.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/os.hpp"
#include "godot_cpp/classes/project_settings.hpp"
#include "godot_cpp/classes/resource_loader.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/variant/utility_functions.hpp"


using namespace godot;


#ifdef DEBUG_ENABLED
#pragma optimize("", off)
#endif


const char* CodeWindow::s_file_loaded = "file_loaded";
const char* CodeWindow::s_file_closed = "file_closed";
const char* CodeWindow::s_focus_switched = "focus_switched";
const char* CodeWindow::s_breakpoint_added = "breakpoint_added";
const char* CodeWindow::s_breakpoint_removed = "breakpoint_removed";
const char* CodeWindow::s_code_opened = "code_opened";
const char* CodeWindow::s_code_cannot_open = "code_cannot_open";


void CodeWindow::_bind_methods(){
  ClassDB::bind_method(D_METHOD("get_code_context_scene_path"), &CodeWindow::get_code_context_scene_path);
  ClassDB::bind_method(D_METHOD("set_code_context_scene_path", "scene"), &CodeWindow::set_code_context_scene_path);
  ADD_PROPERTY(PropertyInfo(Variant::STRING, "code_context_scene", PROPERTY_HINT_FILE), "set_code_context_scene_path", "get_code_context_scene_path");

  ClassDB::bind_method(D_METHOD("get_context_menu_path"), &CodeWindow::get_context_menu_path);
  ClassDB::bind_method(D_METHOD("set_context_menu_path", "path"), &CodeWindow::set_context_menu_path);
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "context_menu_path"), "set_context_menu_path", "get_context_menu_path");

  ClassDB::bind_method(D_METHOD("_on_file_loaded", "file_path"), &CodeWindow::_on_file_loaded);
  ClassDB::bind_method(D_METHOD("_on_breakpoint_added", "line", "id"), &CodeWindow::_on_breakpoint_added);
  ClassDB::bind_method(D_METHOD("_on_breakpoint_removed", "line", "id"), &CodeWindow::_on_breakpoint_removed);
  ClassDB::bind_method(D_METHOD("_on_file_cannot_open", "file_path", "error_code"), &CodeWindow::_on_file_cannot_open);

  ClassDB::bind_method(D_METHOD("_lua_on_started"), &CodeWindow::_lua_on_started);
  ClassDB::bind_method(D_METHOD("_lua_on_paused"), &CodeWindow::_lua_on_paused);
  ClassDB::bind_method(D_METHOD("_lua_on_stopped"), &CodeWindow::_lua_on_stopped);

  ClassDB::bind_method(D_METHOD("_on_context_menu_button_pressed", "button_type"), &CodeWindow::_on_context_menu_button_pressed);

  ClassDB::bind_method(D_METHOD("_on_code_context_menu_ready_event", "obj"), &CodeWindow::_on_code_context_menu_ready_event);

  ClassDB::bind_method(D_METHOD("_on_thread_initialized"), &CodeWindow::_on_thread_initialized);

  ADD_SIGNAL(MethodInfo(s_file_loaded, PropertyInfo(Variant::STRING, "file_path")));
  ADD_SIGNAL(MethodInfo(s_file_closed, PropertyInfo(Variant::STRING, "file_path")));
  ADD_SIGNAL(MethodInfo(s_focus_switched));
  ADD_SIGNAL(MethodInfo(s_breakpoint_added, PropertyInfo(Variant::STRING, "file_path"), PropertyInfo(Variant::INT, "line")));
  ADD_SIGNAL(MethodInfo(s_breakpoint_removed, PropertyInfo(Variant::STRING, "file_path"), PropertyInfo(Variant::INT, "line")));
  ADD_SIGNAL(MethodInfo(s_code_opened, PropertyInfo(Variant::STRING, "file_path")));
}


CodeWindow::CodeWindow(){
  _path_code_root = new _path_node();
}

CodeWindow::~CodeWindow(){
  _recursive_delete(_path_code_root);
}


void CodeWindow::_recursive_delete(_path_node* node){
  for(auto pair: node->_branches)
    _recursive_delete(pair.second);

  delete node;
}


void CodeWindow::_on_file_loaded(String file_path){
  std::string _std_file_path = GDSTR_TO_STDSTR(file_path);
  _path_node* _node = _get_path_node(_std_file_path);
  if(!_node)
    return;

  emit_signal(s_file_loaded, String(file_path));

  int _tab_index = _node->_code_node->get_index();
  set_current_tab(_tab_index);
  set_tab_title(_tab_index, DirectoryUtil::strip_path(_std_file_path).c_str());
  emit_signal(s_focus_switched);

  _update_context_button_visibility();
}

void CodeWindow::_on_breakpoint_added(int line, uint64_t id){
  auto _iter = _context_map.find(id);
  if(_iter == _context_map.end())
    return;

  std::string _file_path = _iter->second->get_current_file_path();
  if(_program_handle->is_running()){
    lua::I_thread_handle* _tref = _program_handle->get_main_thread()->get_interface();
    _tref->get_execution_flow_interface()->add_breakpoint(_file_path.c_str(), line+1);
  }
    
  emit_signal(s_breakpoint_added, String(_file_path.c_str()), Variant(line));
}

void CodeWindow::_on_breakpoint_removed(int line, uint64_t id){
  auto _iter = _context_map.find(id);
  if(_iter == _context_map.end())
    return;

  std::string _file_path = _iter->second->get_current_file_path();
  if(_program_handle->is_running()){
    lua::I_thread_handle* _tref = _program_handle->get_main_thread()->get_interface();
    _tref->get_execution_flow_interface()->remove_breakpoint(_file_path.c_str(), line+1);
  }

  emit_signal(s_breakpoint_removed, String(_file_path.c_str()), Variant(line));
}

void CodeWindow::_on_file_cannot_open(String file_path, int error_code){
  std::string _std_file_path = GDSTR_TO_STDSTR(file_path);
  std::string _err_msg = format_str("[CodeWindow] Cannot open file. Error Code: %d", error_code);
  GameUtils::Logger::print_err_static(_err_msg.c_str());

  close_code_context(_std_file_path);
}


void CodeWindow::_lua_on_started(){
  _context_menu_node->disable_button(CodeContextMenu::be_running, true);
}

void CodeWindow::_lua_on_paused(){
  std::string _fname = _program_handle->get_current_running_file();
  int _line_code = _program_handle->get_current_running_line();

  if(_fname.size() <= 0)
    return;

  _path_node* _node = _get_path_node(_fname);
  if(!_node){
    open_code_context(_fname);
    return;
  }

  set_current_tab(_node->_code_node->get_index());

  _node->_code_node->clear_executing_lines();
  _node->_code_node->set_executing_line(_line_code, true);

  _node->_code_node->focus_at_line(_line_code);
}

void CodeWindow::_lua_on_stopped(){
  // clear each execution line
  for(int i = 0; i < get_child_count(); i++){
    CodeContext* _code_context = (CodeContext*)get_child(i);
    _code_context->clear_executing_lines();
  }

  _context_menu_node->disable_button(CodeContextMenu::be_running, false);
}


void CodeWindow::_on_context_menu_button_pressed(int button_type){
  switch(button_type){
    break; case CodeContextMenu::be_opening:{
      open_code_context();
    }

    break; case CodeContextMenu::be_closing:{
      close_current_code_context();
    }

    break; case CodeContextMenu::be_running:{
      run_current_code_context();
    }

    break; case CodeContextMenu::be_refresh:{
      CodeContext* _code_context = get_current_code_context();
      if(_code_context)
        _code_context->reload_file();
    }
  }
}


void CodeWindow::_update_context_button_visibility(){
  _context_menu_node->show_button((CodeContextMenu::button_enum)(CodeContextMenu::be_closing | CodeContextMenu::be_running | CodeContextMenu::be_refresh), get_tab_count() > 0);
}


CodeWindow::_path_node* CodeWindow:: _get_path_node(const std::string& file_path){
  std::string _mfile_path = DirectoryUtil::get_absolute_path(file_path);
  std::vector<std::string> _split_data; DirectoryUtil::split_directory_string(_mfile_path, _split_data);

  _path_node* _node = _path_code_root;
  for(int i = 0; i < _split_data.size(); i++){
    if(!_node)
      break;

    auto _iter = _node->_branches.find(_split_data[i]);
    if(_iter != _node->_branches.end())
      _node = _iter->second;
    else
      _node = NULL;
  }

  return _node;
}


CodeWindow::_path_node* CodeWindow::_create_path_node(const std::string& file_path){
  std::string _mfile_path = DirectoryUtil::get_absolute_path(file_path);
  std::vector<std::string> _split_data; DirectoryUtil::split_directory_string(_mfile_path, _split_data);

  _path_node* _node = _path_code_root;
  for(int i = 0; i < _split_data.size(); i++){
    auto _iter = _node->_branches.find(_split_data[i]);
    if(_iter != _node->_branches.end())
      _node = _iter->second;
    else{
      _path_node* _new_node = new _path_node();
      _new_node->_path_name = _split_data[i];
      _new_node->_parent = _node;

      _node->_branches[_split_data[i]] = _new_node;
      
      _node = _new_node;
    }
  }

  return _node;
}

bool CodeWindow::_delete_path_node(const std::string& file_path){
  _path_node* _node = _get_path_node(file_path);
  if(!_node)
    return false;

  // deleting all nodes related to file_path until a branch has another node
  while(_node->_parent){
    _path_node* _parent_node = _node->_parent;
    _parent_node->_branches.erase(_node->_path_name);

    delete _node;
    _node = _parent_node;

    if(_parent_node->_branches.size() > 0)
      break;
  }

  return true;
}


void CodeWindow::_on_code_context_menu_ready(CodeContextMenu* obj){
  obj->connect(CodeContextMenu::s_button_pressed, Callable(this, "_on_context_menu_button_pressed"));
  _update_context_button_visibility();

  _context_menu_node_init = true;
}

void CodeWindow::_on_code_context_menu_ready_event(Object* obj){
  if(!obj->is_class(CodeContextMenu::get_class_static()))
    return;

  _on_code_context_menu_ready((CodeContextMenu*)obj);
}


void CodeWindow::_on_thread_initialized(){
  lua::I_thread_handle* _tref = _program_handle->get_main_thread()->get_interface();
  lua::debug::I_execution_flow* _exec_flow = _tref->get_execution_flow_interface();
  for(auto _pair: _context_map){
    std::string _file_path = _pair.second->get_current_file_path();
    const std::vector<int>* _breakpoint_list = _pair.second->get_breakpoint_list();
    for(auto _biter: *_breakpoint_list)
      _exec_flow->add_breakpoint(_file_path.c_str(), _biter+1);
  }
}


void CodeWindow::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

  std::string _exe_path;{
    String _tmp_str = OS::get_singleton()->get_executable_path();
    _exe_path = std::string(GDSTR_AS_PRIMITIVE(_tmp_str), _tmp_str.length());
  }

  _initial_prompt_path = DirectoryUtil::strip_filename(_exe_path);

  _program_handle = get_node<LuaProgramHandle>("/root/GlobalLuaProgramHandle");
  if(!_program_handle){
    GameUtils::Logger::print_err_static("[CodeWindow] Cannot get Node for Program Handle for Lua.");

    _quit_code = ERR_UNAVAILABLE;
    goto on_error_label;
  }

  _context_menu_node = get_node<CodeContextMenu>(_context_menu_path);
  if(!_context_menu_node){
    GameUtils::Logger::print_err_static("[CodeWindow] Cannot get Node for Context Menu.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _context_menu_node->connect(SIGNAL_ON_READY, Callable(this, "_on_code_context_menu_ready_event"));
  if(_context_menu_node->is_initialized())
    _on_code_context_menu_ready(_context_menu_node);

  _code_context_scene = ResourceLoader::get_singleton()->load(_code_context_scene_path);
  if(_code_context_scene == NULL){
    GameUtils::Logger::print_err_static("[CodeWindow] Scene for CodeContext cannot be find.");

    _quit_code = ERR_DOES_NOT_EXIST;
    goto on_error_label;
  }

  {// testing _code_context_scene
    Node* _test_node = _code_context_scene->instantiate();
    String _node_class = _test_node->get_class();
    
    _test_node->queue_free();
    if(_node_class != CodeContext::get_class_static()){
      GameUtils::Logger::print_err_static("[CodeWindow] Scene for CodeContext does not contain CodeContext node.");

      _quit_code = ERR_UNCONFIGURED;
      goto on_error_label;
    }
  }

  // delete all node that still as its child
  while(get_child_count() > 0){
    Node* _child_node = get_child(0);
    
    remove_child(_child_node);
    _child_node->queue_free();
  }

  _program_handle->connect(LuaProgramHandle::s_thread_starting, Callable(this, "_on_thread_initialized"));
  _program_handle->connect(LuaProgramHandle::s_starting, Callable(this, "_lua_on_started"));
  _program_handle->connect(LuaProgramHandle::s_pausing, Callable(this, "_lua_on_paused"));
  _program_handle->connect(LuaProgramHandle::s_stopping, Callable(this, "_lua_on_stopped"));

  _initialized = true;
  return;


  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  return;}
}


void CodeWindow::change_focus_code_context(const std::string& file_path){
  _path_node* _node = _get_path_node(file_path);
  if(!_node){
    open_code_context(file_path);
    return;
  }

  if(!_node->_code_node){
    GameUtils::Logger::print_warn_static(gd_format_str("[CodeWindow] No CodeContext object for '{0}' file.", String(file_path.c_str())));
    return;
  }

  set_current_tab(_node->_code_node->get_index());
  emit_signal(s_focus_switched);
}


std::string CodeWindow::get_current_focus_code() const{
  std::string _res = get_current_focus_code_path();
  if(_res.length() > 0)
    _res = DirectoryUtil::strip_path(_res);
  
  return _res;
}

std::string CodeWindow::get_current_focus_code_path() const{
  if(get_tab_count() <= 0)
    return "";

  CodeContext* _current_node = (CodeContext*)get_current_tab_control();
  return _current_node->get_current_file_path();
}


void CodeWindow::open_code_context(){
  const char* _window_title = "Open Lua File";
  const size_t _file_path_buffer_size = 256;
  char* _file_path_buffer = new char[_file_path_buffer_size]; ZeroMemory(_file_path_buffer, _file_path_buffer_size);

#if (_WIN64) || (_WIN32)
  OPENFILENAMEA _file_config; ZeroMemory(&_file_config, sizeof(OPENFILENAMEA));
  _file_config.lStructSize = sizeof(OPENFILENAMEA);
  _file_config.lpstrFilter = "*.LUA;.TXT";
  _file_config.lpstrFile = _file_path_buffer;
  _file_config.nMaxFile = _file_path_buffer_size;
  _file_config.lpstrInitialDir = _initial_prompt_path.c_str();
  _file_config.lpstrTitle = _window_title;
  _file_config.Flags = OFN_DONTADDTORECENT | OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_NOLONGNAMES | OFN_NONETWORKBUTTON | OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST;

  bool _success = GetOpenFileNameA(&_file_config);
#endif

  if(!_success)
    return;

  open_code_context(_file_path_buffer);
}

void CodeWindow::open_code_context(const std::string& file_path){
  {CodeContext* _test_node = get_code_context(file_path);
    if(_test_node){
      change_focus_code_context(file_path);
      emit_signal(s_code_opened);
      return;
    }
  }

  CodeContext* _inst_node = (CodeContext*)_code_context_scene->instantiate();

  _path_node* _pnode = _create_path_node(file_path);
  _pnode->_code_node = _inst_node;

  _context_map[_inst_node->get_instance_id()] = _inst_node;

  _inst_node->connect(CodeContext::s_file_loaded, Callable(this,"_on_file_loaded"));
  _inst_node->connect(CodeContext::s_breakpoint_added, Callable(this, "_on_breakpoint_added"));
  _inst_node->connect(CodeContext::s_breakpoint_removed, Callable(this, "_on_breakpoint_removed"));
  _inst_node->connect(CodeContext::s_cannot_load, Callable(this, "_on_file_cannot_open"));

  add_child(_inst_node);

  _inst_node->load_file(file_path);
}


bool CodeWindow::close_current_code_context(){
  CodeContext* _code = get_current_code_context();
  if(!_code)
    return false;

  _delete_path_node(_code->get_current_file_path());

  emit_signal(s_file_closed, String(_code->get_current_file_path().c_str()));

  // tab switching handled by godot
  remove_child(_code);
  _code->queue_free();

  _context_map.erase(_code->get_instance_id());

  _update_context_button_visibility();

  return true;
}

bool CodeWindow::close_code_context(const std::string& file_path){
  CodeContext* _code = get_code_context(file_path);
  if(!_code)
    return false;

  _delete_path_node(file_path);

  emit_signal(s_file_closed, String(file_path.c_str()));

  // tab switching handled by godot
  remove_child(_code);
  _code->queue_free();

  _context_map.erase(_code->get_instance_id());

  _update_context_button_visibility();

  return true;
}


void CodeWindow::run_current_code_context(){
  CodeContext* _code = get_current_code_context();
  _program_handle->start_lua(_code->get_current_file_path());
}


CodeContext* CodeWindow::get_current_code_context(){
  if(get_tab_count() <= 0)
    return NULL;

  return (CodeContext*)get_current_tab_control();
}

CodeContext* CodeWindow::get_code_context(const std::string& file_path){
  _path_node* _node = _get_path_node(file_path);
  return _node? _node->_code_node: NULL;
}


String CodeWindow::get_code_context_scene_path() const{
  return _code_context_scene_path;
}

void CodeWindow::set_code_context_scene_path(String scene_path){
  _code_context_scene_path = scene_path;
}


NodePath CodeWindow::get_context_menu_path() const{
  return _context_menu_path;
}

void CodeWindow::set_context_menu_path(NodePath path){
  _context_menu_path = path;
}