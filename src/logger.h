#ifndef LOGGER_HEADER
#define LOGGER_HEADER

#include "I_logger.h"

#include "godot_cpp/godot.hpp"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/variant.hpp"
#include "godot_cpp/variant/array.hpp"


#include "map"

#ifdef _WIN64
#include "windows.h"
#endif



using namespace godot;


namespace GameUtils{
  class Logger: public godot::Node, public GameUtils::I_logger{
  GDCLASS(Logger, godot::Node)

    private:
#ifdef _WIN64
      HANDLE _log_mutex;
#endif

    private:
      static String _get_current_time();

    protected:
      static void _bind_methods();

    public:
      Logger();
      ~Logger();

      void _ready() override;

      static Logger *get_static_logger();

      static void print_log_static(const String &log);
      static void print_warn_static(const String &warning);
      static void print_err_static(const String &err);

      void print_log(const String &log);
      void print_warn(const String &warning);
      void print_err(const String &err);

      void print(I_logger::std_type type, const String &msg);
      void print(I_logger::std_type type, const char* msg) override;
  };
}

#endif