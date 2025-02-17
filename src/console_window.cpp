#include "console_window.h"
#include "defines.h"
#include "error_trigger.h"
#include "gdutils.h"
#include "logger.h"
#include "strutil.h"

#include "memory"
 
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/math.hpp"

#define CONSOLE_WINDOW_OUTPUT_BUFFER_SIZE 4096



using namespace GameUtils;
using namespace godot;
using namespace lua::global;


const char* ConsoleWindow::s_input_entered = "input_entered";
Color ConsoleWindow::placeholder_text_color = gdutils::construct_color(0x7F7F7FFF);


void ConsoleWindow::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_input_entered", "input_data"), &ConsoleWindow::_on_input_entered);

  ClassDB::bind_method(D_METHOD("get_output_text_path"), &ConsoleWindow::get_output_text_path);
  ClassDB::bind_method(D_METHOD("set_output_text_path", "path"), &ConsoleWindow::set_output_text_path);
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "output_text_path"), "set_output_text_path", "get_output_text_path");

  ClassDB::bind_method(D_METHOD("get_input_text_path"), &ConsoleWindow::get_input_text_path);
  ClassDB::bind_method(D_METHOD("set_input_text_path"), &ConsoleWindow::set_input_text_path);
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "input_text_path"), "set_input_text_path", "get_input_text_path");

  ClassDB::bind_method(D_METHOD("_on_log", "msg_info", "msg"), &ConsoleWindow::_on_log);
  ClassDB::bind_method(D_METHOD("_on_log_warn", "msg_info", "msg"), &ConsoleWindow::_on_log_warn);
  ClassDB::bind_method(D_METHOD("_on_log_err", "msg_info", "msg"), &ConsoleWindow::_on_log_err);

  ClassDB::bind_method(D_METHOD("get_log_color"), &ConsoleWindow::get_log_color);
  ClassDB::bind_method(D_METHOD("set_log_color", "color"), &ConsoleWindow::set_log_color);
  ClassDB::bind_method(D_METHOD("get_warn_color"), &ConsoleWindow::get_warn_color);
  ClassDB::bind_method(D_METHOD("set_warn_color", "color"), &ConsoleWindow::set_warn_color);
  ClassDB::bind_method(D_METHOD("get_err_color"), &ConsoleWindow::get_err_color);
  ClassDB::bind_method(D_METHOD("set_err_color", "color"), &ConsoleWindow::set_err_color);
  ClassDB::bind_method(D_METHOD("get_placeholder_text"), &ConsoleWindow::get_placeholder_text);
  ClassDB::bind_method(D_METHOD("set_placeholder_text", "text"), &ConsoleWindow::set_placeholder_text);

  ADD_PROPERTY(PropertyInfo(Variant::COLOR, "log_color"), "set_log_color", "get_log_color");
  ADD_PROPERTY(PropertyInfo(Variant::COLOR, "warn_color"), "set_warn_color", "get_warn_color");
  ADD_PROPERTY(PropertyInfo(Variant::COLOR, "err_color"), "set_err_color", "get_err_color");
  ADD_PROPERTY(PropertyInfo(Variant::STRING, "placeholder_text"), "set_placeholder_text", "get_placeholder_text");

  ADD_SIGNAL(MethodInfo(s_input_entered, PropertyInfo(Variant::STRING, "input_data")));
}


ConsoleWindow::ConsoleWindow(){
  // Added null character termination for debugging purposes
  _output_buffer = (char*)std::malloc(CONSOLE_WINDOW_OUTPUT_BUFFER_SIZE+1);
  _output_buffer[CONSOLE_WINDOW_OUTPUT_BUFFER_SIZE] = '\0';
}

ConsoleWindow::~ConsoleWindow(){
  std::free(_output_buffer);
}


void ConsoleWindow::_on_log(const String& msg_info, const String& str){
  String _str = str;
  if(!_str.ends_with("\n"))
    _str += "\n";

  String _newstr = _wrap_color(_str, _log_color);
  append_output_buffer(GDSTR_TO_STDSTR(_newstr));
}

void ConsoleWindow::_on_log_warn(const String& msg_info, const String& str){
  String _str = gd_format_str("{0}: {1}", msg_info, str);
  if(!_str.ends_with("\n"))
    _str += "\n";

  String _newstr = _wrap_color(_str, _warn_color);
  append_output_buffer(GDSTR_TO_STDSTR(_newstr));
}

void ConsoleWindow::_on_log_err(const String& msg_info, const String& str){
  String _str = gd_format_str("{0}: {1}", msg_info, str);
  if(!_str.ends_with("\n"))
    _str += "\n";

  String _newstr = _wrap_color(_str, _err_color);
  append_output_buffer(GDSTR_TO_STDSTR(_newstr));
}


String ConsoleWindow::_wrap_color(const String& str, godot::Color col){
  String _col_hex = gdutils::as_hex(col);
  return gd_format_str("[color=#{0}]{1}[/color]", _col_hex, str);
}


void ConsoleWindow::_on_input_entered(String str){
  str += "\n";

  append_output_buffer("> " + GDSTR_TO_STDSTR(str));
  emit_signal(s_input_entered, str);

  _input_text->set_text("");
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

  if(_output_str.is_empty())
    _output_str = _wrap_color(_placeholder_text, placeholder_text_color);

  _output_text->set_text(_output_str);
}


void ConsoleWindow::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code;

  _program_handle = get_node<LuaProgramHandle>("/root/GlobalLuaProgramHandle");
  if(!_program_handle){
    GameUtils::Logger::print_err_static("[ConsoleWindow] Cannot get Program Handle for Lua.");

    _quit_code = ERR_UNAVAILABLE;
    goto on_error_label;
  }

  _output_text = get_node<RichTextLabel>(_output_text_path);
  if(!_output_text){
    GameUtils::Logger::print_err_static("[ConsoleWindow] Cannot get TextEdit for Console Output.");

    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _input_text = get_node<LineEdit>(_input_text_path);
  if(!_input_text){
    GameUtils::Logger::print_err_static("[ConsoleWindow] Cannot get LineEdit for Console Input.");
    
    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  // Get a logger object
  Logger* _logger = get_node<Logger>("/root/GlobalLogger");
  if(_logger){
    _logger->connect(Logger::s_on_log, Callable(this, "_on_log"));
    _logger->connect(Logger::s_on_warn_log, Callable(this, "_on_log_warn"));
    _logger->connect(Logger::s_on_error_log, Callable(this, "_on_log_err"));
  }

  connect(s_input_entered, Callable(_program_handle, "append_input"));
  _input_text->connect("text_submitted", Callable(this, "_on_input_entered"));

  _output_text->set_text("");
  _input_text->set_text("");

  // update output (maybe set to placeholder)
  _write_to_output_text();

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


NodePath ConsoleWindow::get_input_text_path() const{
  return _input_text_path;
}

void ConsoleWindow::set_input_text_path(NodePath path){
  _input_text_path = path;
}


Color ConsoleWindow::get_log_color() const{
  return _log_color;
}

void ConsoleWindow::set_log_color(Color col){
  _log_color = col;
}


Color ConsoleWindow::get_warn_color() const{
  return _warn_color;
}

void ConsoleWindow::set_warn_color(Color col){
  _warn_color = col;
}


Color ConsoleWindow::get_err_color() const{
  return _err_color;
}

void ConsoleWindow::set_err_color(Color col){
  _err_color = col;
}


String ConsoleWindow::get_placeholder_text() const{
  return _placeholder_text;
}

void ConsoleWindow::set_placeholder_text(const String& str){
  _placeholder_text = str;
}