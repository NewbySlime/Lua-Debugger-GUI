#include "common_event.h"
#include "error_trigger.h"
#include "gdutils.h"
#include "logger.h"
#include "option_control.h"
#include "strutil.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/variant/array.hpp"


using namespace gdutils;
using namespace godot;


const char* OptionControl::default_option_config_file_path = "option.cfg";
const char* OptionControl::default_config_gvarname = "default_config_data";
const char* OptionControl::expected_config_vartype_gvarname = "expected_config_vartype";

const char* OptionControl::gvar_object_node_path = "option_control_path";

const char* OptionControl::s_value_set = "value_set";

const char* _show_animation = "slide_animation";
const char* _hide_animation = "slide_animation_hide";


void OptionControl::_bind_methods(){
  ClassDB::bind_method(D_METHOD("_on_value_set", "key", "value"), &OptionControl::_on_value_set);
  ClassDB::bind_method(D_METHOD("_on_option_button_pressed"), &OptionControl::_on_option_button_pressed);
  ClassDB::bind_method(D_METHOD("_on_option_focus_exited"), &OptionControl::_on_option_focus_exited);
  ClassDB::bind_method(D_METHOD("_on_option_list_menu_ready", "node"), &OptionControl::_on_option_list_menu_ready);

  ClassDB::bind_method(D_METHOD("set_logo_settings_image", "image"), &OptionControl::set_logo_settings_image);
  ClassDB::bind_method(D_METHOD("get_logo_settings_image"), &OptionControl::get_logo_settings_image);

  ClassDB::bind_method(D_METHOD("set_logo_settings_close_image", "image"), &OptionControl::set_logo_settings_close_image);
  ClassDB::bind_method(D_METHOD("get_logo_settings_close_image"), &OptionControl::get_logo_settings_close_image);

  ClassDB::bind_method(D_METHOD("set_animation_player", "path"), &OptionControl::set_animation_player);
  ClassDB::bind_method(D_METHOD("get_animation_player"), &OptionControl::get_animation_player);

  ClassDB::bind_method(D_METHOD("set_settings_button", "path"), &OptionControl::set_settings_button);
  ClassDB::bind_method(D_METHOD("get_settings_button"), &OptionControl::get_settings_button);

  ClassDB::bind_method(D_METHOD("set_settings_unfocus_area", "path"), &OptionControl::set_settings_unfocus_area);
  ClassDB::bind_method(D_METHOD("get_settings_unfocus_area"), &OptionControl::get_settings_unfocus_area);

  ClassDB::bind_method(D_METHOD("set_option_menu_path", "path"), &OptionControl::set_option_menu_path);
  ClassDB::bind_method(D_METHOD("get_option_menu_path"), &OptionControl::get_option_menu_path);

  ADD_SIGNAL(MethodInfo(OptionControl::s_value_set, PropertyInfo(Variant::STRING, "key"), PropertyInfo(Variant::NIL, "value")));

  ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "settings_logo", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), "set_logo_settings_image", "get_logo_settings_image");
  ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "close_logo", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), "set_logo_settings_close_image", "get_logo_settings_close_image");
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "animation_player"), "set_animation_player", "get_animation_player");
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "settings_button"), "set_settings_button", "get_settings_button");
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "settings_unfocus_area"), "set_settings_unfocus_area", "get_settings_unfocus_area");
  ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "option_menu_path"), "set_option_menu_path", "get_option_menu_path");
}


void OptionControl::_on_value_set(const String& key, const Variant& value){
  set_option_value(key, value, false);
}


void OptionControl::_on_option_button_pressed(){
  if(_is_showing)
    _hide_option_ui();
  else
    _show_option_ui();
}

void OptionControl::_on_option_focus_exited(){
  _hide_option_ui();
}


void OptionControl::_on_option_list_menu_ready(Node* node){
  _update_option_ui();
}


void OptionControl::_update_option_ui(){
  Array _key_list = _option_data.keys();
  for(int i = 0; i < _key_list.size(); i++){
    Variant _key = _key_list[i];
    Variant _value = _option_data[_key];

    _option_menu->set_value_data(_key, _value);
  }
}


void OptionControl::_play_show_option_ui(bool hide){
  double _blend = -1;
  if(_animation_player->is_playing()){
    _blend =
      _animation_player->get_current_animation_length() -
      _animation_player->get_current_animation_position();
  }

  if(hide)
    _animation_player->play(_hide_animation, _blend);
  else
    _animation_player->play(_show_animation, _blend);
}

void OptionControl::_show_option_ui(){
  _play_show_option_ui(false);
  _settings_button->set_button_icon(_logo_settings_close_image);
  _is_showing = true;
}

void OptionControl::_hide_option_ui(){
  _play_show_option_ui(true);
  _settings_button->set_button_icon(_logo_settings_image);
  _is_showing = false;
}


void OptionControl::_ready(){
  Engine* _engine = Engine::get_singleton();
  if(_engine->is_editor_hint())
    return;

  int _quit_code = 0;

{ // enclosure for using goto
  _gvariables = get_node<GlobalVariables>(GlobalVariables::singleton_path);
  if(!_gvariables){
    GameUtils::Logger::print_err_static("[OptionControl] Cannot get GlobalVariables singleton object. (Is it not configured?)");
    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _animation_player = get_node<AnimationPlayer>(_animation_player_path);
  if(!_animation_player){
    GameUtils::Logger::print_err_static("[OptionControl] Cannot get AnimationPlayer.");
    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _settings_button = get_node<Button>(_settings_button_path);
  if(!_settings_button){
    GameUtils::Logger::print_err_static("[OptionControl] Cannot get Show Button for Option UI.");
    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _settings_unfocus_area = get_node<Control>(_settings_unfocus_area_path);
  if(!_settings_unfocus_area){
    GameUtils::Logger::print_err_static("[OptionControl] Cannot Focus area for unfocus state.");
    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

  _option_menu = get_node<OptionListMenu>(_option_menu_path);
  if(!_option_menu){
    GameUtils::Logger::print_err_static("[OptionControl] Cannot get OptionListMenu.");
    _quit_code = ERR_UNCONFIGURED;
    goto on_error_label;
  }

{ // enclosure for checking default config data
  Variant _val = _gvariables->get_global_value(OptionControl::default_config_gvarname);
  if(_val.get_type() != Variant::DICTIONARY)
    GameUtils::Logger::print_warn_static("[OptionControl] Default config data is not a valid Dictionary type.");
} // enclosure closing

{ // enclosure for checking expected config vartype data
  const VariantTypeParser& _type_parser = _gvariables->get_type_parser();
  Dictionary _vartype_data;
  Array _key_list;
  Variant _val = _gvariables->get_global_value(OptionControl::expected_config_vartype_gvarname);
  if(_val.get_type() != Variant::DICTIONARY){
    GameUtils::Logger::print_warn_static("[OptionControl] Expected config vartype is not a valid Dictionary type.");
    goto skip_checking;
  }

  _vartype_data = _val;
  _key_list = _vartype_data.keys();
  for(int i = 0; i < _key_list.size(); i++){
    Variant _key = _key_list[i];
    Variant _value = _vartype_data[_key];

    if(!_type_parser.is_valid_type(_value)){
      GameUtils::Logger::print_warn_static(gd_format_str("[OptionControl] Value '{0}' is not a valid expected type.", _key));
    }
  }

  skip_checking:{}
} // enclosure closing

  _gvariables->set_global_value(OptionControl::gvar_object_node_path, get_path());

  _option_menu->connect(OptionListMenu::s_value_set, Callable(this, "_on_value_set"));
  _option_menu->connect(SIGNAL_ON_READY, Callable(this, "_on_option_list_menu_ready"));

  _settings_button->connect("pressed", Callable(this, "_on_option_button_pressed"));
  _settings_unfocus_area->connect("focus_entered", Callable(this, "_on_option_focus_exited"));

  Error _err_check = load_option_data(false);
  if(_err_check != OK){
    GameUtils::Logger::print_err_static("[OptionControl] Cannot load option configuration data.");
    _quit_code = _err_check;
    goto on_error_label;
  }
} // enclosure closing

  return;


  on_error_label:{
    ErrorTrigger::trigger_generic_error_message();
    get_tree()->quit(_quit_code);
  }
}


Error OptionControl::save_option_data(){
  Ref<FileAccess> _file = FileAccess::open(_config_file_path, FileAccess::WRITE);
  if(_file.is_null()){
    GameUtils::Logger::print_err_static(gd_format_str("[OptionControl] Config file: Cannot open file. Error code: {0}", FileAccess::get_open_error()));
    return FileAccess::get_open_error();
  }

  Array _key_list = _option_data.keys();
  for(int i = 0; i < _key_list.size(); i++){
    Variant _key = _key_list[i];
    Variant _value = _option_data[_key];

    _file->store_line(gd_format_str("{0}: {1}", _key, _value));
  }

  return OK;
}

Error OptionControl::load_option_data(bool update_ui){
  const VariantTypeParser& _type_parser = _gvariables->get_type_parser();

  _option_data.clear();

  Dictionary _default_data;
  Variant _default_data_val = _gvariables->get_global_value(OptionControl::default_config_gvarname);
  if(_default_data_val.get_type() != Variant::DICTIONARY){
    GameUtils::Logger::print_warn_static(gd_format_str("[OptionControl] Config file: Global variable '{0}' is not a valid Dictionary.", OptionControl::default_config_gvarname));
  }
  else
    _default_data = _default_data_val;

  _option_data = _default_data;
    
  // if config file is not present, create a new one
  Ref<FileAccess> _file = FileAccess::open(_config_file_path, FileAccess::READ);
  if(_file.is_null())
    return save_option_data();

  Dictionary _vartype_data;
  Variant _vartype_data_val = _gvariables->get_global_value(OptionControl::expected_config_vartype_gvarname);
  if(_vartype_data_val.get_type() != Variant::DICTIONARY){
    GameUtils::Logger::print_err_static(gd_format_str("[OptionControl] Config file: Global variable '{0}' is not a valid Dictionary.", OptionControl::expected_config_vartype_gvarname));
    return ERR_UNCONFIGURED;
  }

  _vartype_data = _vartype_data_val;

  // Vartype validity already checked in _ready()

  size_t _curr_line = 0;
  while(!_file->eof_reached()){
    _curr_line++;

    PackedStringArray _arr;
    String _line_data = _file->get_line();
    _arr = _line_data.split(" \t", false);
    if(_arr.size() <= 0)
      continue;

    int64_t _idx = _line_data.find(":");
    if(_idx < 0){
      GameUtils::Logger::print_err_static(gd_format_str("[OptionControl] Config file: Cannot determine end of key string at line {0}.", _curr_line));
      continue;
    }

    if((_idx+1) >= _line_data.length()){
      GameUtils::Logger::print_err_static(gd_format_str("[OptionControl] Config file: Empty value at line {0}?", _curr_line));
      continue;
    }

    String _key_str = _line_data.substr(0, _idx);
    String _value_str = _line_data.substr(_idx+1);

    if(!_vartype_data.has(_key_str)){
      GameUtils::Logger::print_err_static(gd_format_str("[OptionControl] Config file: Expected variant type for '{0}' not found.", _key_str));
      continue;
    }

    Variant _expected_type_str = _vartype_data[_key_str];
    if(!_type_parser.is_valid_type(_expected_type_str))
      continue;

    Variant::Type _expected_type = _type_parser.parse_str_to_type(_expected_type_str);
    String _err_msg;
    Variant _parsed_value = parse_str_to_var(_expected_type, _value_str, &_err_msg);
    if(_parsed_value.get_type() == Variant::NIL){
      GameUtils::Logger::print_err_static(gd_format_str("[OptionControl] Config file: Cannot parse value '{0}'. Reason; {1}", _key_str, _err_msg));
      continue;
    }

    set_option_value(_key_str, _parsed_value, update_ui);

    // NOTE: default type is store as string.
  }

  return OK;
}


void OptionControl::set_option_value(const String& key, const Variant& value, bool update_ui){
  _option_data[key] = value;
  if(update_ui)
    _option_menu->set_value_data(key, value);
  
  emit_signal(s_value_set, key, value);
  save_option_data();
}

Variant OptionControl::get_option_value(const String& key){
  if(!_option_data.has(key))
    return Variant();

  return _option_data[key];
}


void OptionControl::set_logo_settings_image(Ref<Texture> image){
  _logo_settings_image = image;
}

Ref<Texture> OptionControl::get_logo_settings_image() const{
  return _logo_settings_image;
}


void OptionControl::set_logo_settings_close_image(Ref<Texture> image){
  _logo_settings_close_image = image;
}

Ref<Texture> OptionControl::get_logo_settings_close_image() const{
  return _logo_settings_close_image;
}


void OptionControl::set_animation_player(const NodePath& path){
  _animation_player_path = path;
}

NodePath OptionControl::get_animation_player() const{
  return _animation_player_path;
}


void OptionControl::set_settings_button(const NodePath& path){
  _settings_button_path = path;
}

NodePath OptionControl::get_settings_button() const{
  return _settings_button_path;
}


void OptionControl::set_settings_unfocus_area(const NodePath& path){
  _settings_unfocus_area_path = path;
}

NodePath OptionControl::get_settings_unfocus_area() const{
  return _settings_unfocus_area_path;
}


void OptionControl::set_option_menu_path(const NodePath& path){
  _option_menu_path = path;
}

NodePath OptionControl::get_option_menu_path() const{
  return _option_menu_path;
}