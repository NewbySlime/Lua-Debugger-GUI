#include "defines.h"
#include "logger.h"
#include "strutil.h"

#include "chrono"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/time.hpp"

#include "godot_cpp/variant/utility_functions.hpp"

using namespace GameUtils;
using namespace godot;


// 0: Time string (HH-MM-SS)
// 1: msec string
#define TIME_STRING_FORMAT "[{0}.{1}]"

#define LOGGING_FLAG "[INF]"
#define WARNING_FLAG "[WRN]"
#define ERROR_FLAG "[ERR]"


const char* Logger::s_on_log = "msg_log";
const char* Logger::s_on_warn_log = "warn_msg_log";
const char* Logger::s_on_error_log = "error_msg_log";


Logger *_static_logger_obj = NULL;


void Logger::_bind_methods(){
  ADD_SIGNAL(MethodInfo(s_on_log, PropertyInfo(Variant::STRING, "msg")));
  ADD_SIGNAL(MethodInfo(s_on_warn_log, PropertyInfo(Variant::STRING, "msg")));
  ADD_SIGNAL(MethodInfo(s_on_error_log, PropertyInfo(Variant::STRING, "msg")));
}


Logger::Logger(){
#if (_WIN64) || (_WIN32)
  _log_mutex = CreateMutex(NULL, false, NULL);
#endif
}

Logger::~Logger(){
#if (_WIN64) || (_WIN32)
  CloseHandle(_log_mutex);
#endif
}


String Logger::_get_current_time(){
  using namespace std::chrono;

  Time *_time_obj = Time::get_singleton();
  auto _currtime = system_clock::now().time_since_epoch();

  String _gd_time_str = _time_obj->get_time_string_from_system();
  long long _time_milli = duration_cast<milliseconds>(_currtime).count() % 1000;

  return gd_format_str(TIME_STRING_FORMAT, _gd_time_str, _time_milli);
}

String Logger::_log_formatting(const char* flag, const String& msg){
  String _curr_time = _get_current_time();
  return gd_format_str("{0}{1}: {2}", flag, _curr_time, msg);
}


void Logger::_ready(){
  _static_logger_obj = this;
}

Logger* Logger::get_static_logger(){
  return _static_logger_obj;
}


void Logger::print_log_static(const String &log){
  Logger *_inst = get_static_logger();

  if(_inst)
    _inst->print_log(log);
}

void Logger::print_warn_static(const String &warning){
  Logger *_inst = get_static_logger();

  if(_inst)
    _inst->print_warn(warning);
}

void Logger::print_err_static(const String &err){
  Logger *_inst = get_static_logger();

  if(_inst)
    _inst->print_err(err);
}

void Logger::print_log(const String &log){
  __LOCK_MUTEX(_log_mutex);
  String _msg = _log_formatting(LOGGING_FLAG, log);
  UtilityFunctions::print(_msg);

  if(!_msg.ends_with("\n"))
    _msg += "\n";
  emit_signal(s_on_log, _msg);
  __RELEASE_MUTEX(_log_mutex);
}

void Logger::print_warn(const String &warning){
  __LOCK_MUTEX(_log_mutex);
  String _msg = _log_formatting(WARNING_FLAG, warning);
  UtilityFunctions::print(_msg);

  if(!_msg.ends_with("\n"))
    _msg += "\n";
  emit_signal(s_on_warn_log, _msg);
  __RELEASE_MUTEX(_log_mutex); 
}

void Logger::print_err(const String &err){
  __LOCK_MUTEX(_log_mutex);
  String _msg = _log_formatting(ERROR_FLAG, err);
  UtilityFunctions::print(_msg);

  if(!_msg.ends_with("\n"))
    _msg += "\n";
  emit_signal(s_on_error_log, _msg);
  __RELEASE_MUTEX(_log_mutex);
}


void Logger::print(std_type type, const String &msg){
  switch(type){
    break; case std_type::ST_LOG:
      print_log(msg);

    break; case std_type::ST_WARNING:
      print_warn(msg);

    break; case std_type::ST_ERROR:
      print_err(msg);
  }
}

void Logger::print(std_type type, const char* msg){
  print(type, String(msg));
}