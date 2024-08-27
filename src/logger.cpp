#include "defines.h"
#include "logger.h"

#include "chrono"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/time.hpp"

#include "godot_cpp/variant/utility_functions.hpp"


// 0: Time string (HH-MM-SS)
// 1: msec string
#define TIME_STRING_FORMAT String("[{0}.{1}]")


#define LOGGING_FLAG String("[INF]")
#define WARNING_FLAG String("[WRN]")
#define ERROR_FLAG String("[ERR]")



using namespace GameUtils;


Logger *_static_logger_obj = NULL;



void Logger::_bind_methods(){

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

  auto _time_str = _time_obj->get_time_string_from_system();
  auto _time_milli = duration_cast<milliseconds>(_currtime).count() % 1000;

  Array _paramarr;{
    _paramarr.append(_time_str);
    _paramarr.append(_time_milli);
  }
  return TIME_STRING_FORMAT.format(_paramarr);
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
  UtilityFunctions::print(LOGGING_FLAG, _get_current_time(), log);
  __RELEASE_MUTEX(_log_mutex);
}

void Logger::print_warn(const String &warning){
  __LOCK_MUTEX(_log_mutex);
  UtilityFunctions::print(WARNING_FLAG, _get_current_time(), warning);
  __RELEASE_MUTEX(_log_mutex); 
}

void Logger::print_err(const String &err){
  __LOCK_MUTEX(_log_mutex);
  UtilityFunctions::printerr(ERROR_FLAG, _get_current_time(), err);
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