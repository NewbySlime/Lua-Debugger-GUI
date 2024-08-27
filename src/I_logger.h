#ifndef GU_I_LOGGER_HEADER
#define GU_I_LOGGER_HEADER

#include "string"


namespace GameUtils{
  class I_logger{
    public:
      enum std_type{
        ST_LOG,
        ST_WARNING,
        ST_ERROR
      };

      virtual void print(std_type type, const char *msg){};
  };
}

#endif