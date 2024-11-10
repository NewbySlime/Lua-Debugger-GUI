#include "console_window.h"
#include "error_trigger.h"
#include "logger.h"

#include "memory"
 
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/math.hpp"

#define CONSOLE_WINDOW_OUTPUT_BUFFER_SIZE 4096


using namespace godot;
using namespace lua::global;


void ConsoleWindow::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_output_written"), &ConsoleWindow::_on_output_written);

  ClassDB::bind_method(D_METHOD("get_output_text_path"), &ConsoleWindow::get_output_text_path);
  ClassDB::bind_method(D_METHOD("set_output_text_path", "path"), &ConsoleWindow::set_output_text_path);
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "output_text_path"), "set_output_text_path", "get_output_text_path");
}


ConsoleWindow::ConsoleWindow(){
  // Added null character termination for debugging purposes
  _output_buffer = (char*)std::malloc(CONSOLE_WINDOW_OUTPUT_BUFFER_SIZE+1);
  _output_buffer[CONSOLE_WINDOW_OUTPUT_BUFFER_SIZE] = '\0';
}

ConsoleWindow::~ConsoleWindow(){
  std::free(_output_buffer);
}


void ConsoleWindow::_on_output_written(){
  _program_handle->lock_object();

{ // enclosure for using gotos
  I_print_override* _print_override = _program_handle->get_print_override();
  if(!_print_override)
    goto skip_to_return;
    
  string_store _str; _print_override->read_all(&_str);

  _add_string_to_output_buffer(_str.data);
  _write_to_output_text();
} // enclosure closing

  skip_to_return:{}
  _program_handle->unlock_object();
}


void ConsoleWindow::_add_string_to_output_buffer(const std::string& str){
  int _str_idx = 0;
  while(_str_idx < str.size()){
    int _write_available = CONSOLE_WINDOW_OUTPUT_BUFFER_SIZE - _ob_index_top;
    if(_write_available <= 0){
      _ob_index_top = 0;
      _ob_index_bottom = 0;
      _ob_is_repeating = true;

      continue;
    }

    int _write_size = str.size() - _str_idx;
    
    int _cpy_len = Math::min(_write_size, _write_available);
    std::memcpy(&_output_buffer[_ob_index_top], &str.c_str()[_str_idx], _cpy_len);

    _ob_index_top += _cpy_len;
    _str_idx += _cpy_len;

    if(_ob_is_repeating)
      _ob_index_bottom = _ob_index_top;
  }
  
  if(_output_buffer[CONSOLE_WINDOW_OUTPUT_BUFFER_SIZE])
    GameUtils::Logger::print_warn_static("[ConsoleWindow] Algorithm Error: Output Buffer is written beyond its supposed length.");

  // just in case
  _output_buffer[CONSOLE_WINDOW_OUTPUT_BUFFER_SIZE] = '\0';
}

void ConsoleWindow::_write_to_output_text(){
  godot::String _output_str;

  if(_ob_is_repeating){
    // create cstr buffer
    std::string _top_str = std::string(&_output_buffer[0], _ob_index_top);
    std::string _bottom_str = std::string(&_output_buffer[_ob_index_bottom], CONSOLE_WINDOW_OUTPUT_BUFFER_SIZE - _ob_index_bottom);

    _output_str += _bottom_str.c_str();
    _output_str += _top_str.c_str(); 
  }
  else{
    // create cstr buffer
    std::string _str = std::string(_output_buffer, _ob_index_top);
    _output_str += _str.c_str();
  }

  _output_text->set_text(_output_str);
  _output_text->set_v_scroll(_output_text->get_line_count());
}


void ConsoleWindow::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

  _program_handle = get_node<LuaProgramHandle>("/root/GlobalLuaProgramHandle");
  if(!_program_handle){
    GameUtils::Logger::print_err_static("[LuaProgramHandle] Cannot get Program Handle for Lua.");

    _quit_code = ERR_UNAVAILABLE;
    goto on_error_label;
  }

  _output_text = get_node<TextEdit>(_output_text_path);
  if(!_output_text){
    GameUtils::Logger::print_err_static("[LuaProgramHandle] Cannot get TextEdit for Console Output.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _program_handle->connect(SIGNAL_LUA_ON_OUTPUT_WRITTEN, Callable(this, "_on_output_written"));

  _output_text->set_text("");

  return;


  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();

    get_tree()->quit(_quit_code);
  return;}
}


void ConsoleWindow::clear_output_buffer(){
  _ob_index_top = 0;
  _ob_index_bottom = 0;
  _ob_is_repeating = false;
}

void ConsoleWindow::append_output_buffer(const std::string& msg){
  _add_string_to_output_buffer(msg);
  _write_to_output_text();
}


NodePath ConsoleWindow::get_output_text_path() const{
  return _output_text_path;
}

void ConsoleWindow::set_output_text_path(NodePath path){
  _output_text_path = path;
}