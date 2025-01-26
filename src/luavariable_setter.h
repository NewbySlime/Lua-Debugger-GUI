#ifndef LUA_VARIABLE_SETTER_HEADER
#define LUA_VARIABLE_SETTER_HEADER

#include "Lua-CPPAPI/Src/luadebug_variable_watcher.h"
#include "Lua-CPPAPI/Src/luavariant.h"

#include "vector"


namespace lua::util{
  class IVariableSetter{
    public:
      virtual ~IVariableSetter(){}

      virtual bool set_value(const lua::I_variant* value, const lua::I_variant* key = NULL) = 0;
  };


  class GlobalVariableSetter: public IVariableSetter{
    private:
      lua::debug::I_variable_watcher* _vwatch;

    public:
      GlobalVariableSetter(lua::debug::I_variable_watcher* variable_watcher);
      ~GlobalVariableSetter();

      bool set_value(const lua::I_variant* value, const lua::I_variant* key = NULL) override;
  };

  class LocalVariableSetter: public IVariableSetter{
    private:
      lua::debug::I_variable_watcher* _vwatch;

    public:
      LocalVariableSetter(lua::debug::I_variable_watcher* variable_watcher);
      ~LocalVariableSetter();

      bool set_value(const lua::I_variant* value, const lua::I_variant* key = NULL) override;
  };

  class TableVariableSetter: public IVariableSetter{
    private:
      lua::I_table_var* _tvar;

    public:
      TableVariableSetter(lua::I_table_var* var);
      ~TableVariableSetter();

      bool set_value(const lua::I_variant* value, const lua::I_variant* key = NULL) override;
  };
}


#endif