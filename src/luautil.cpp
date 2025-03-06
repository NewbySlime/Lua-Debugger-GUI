#include "luautil.h"


using namespace lua;


bool lua::util::is_reference_variant(const I_variant* var){
  if(!var)
    return false;

  switch(var->get_type()){
    break; case I_table_var::get_static_lua_type():{
      const I_table_var* _tvar = dynamic_cast<const I_table_var*>(var);
      return _tvar->is_reference();
    }

    break; case I_function_var::get_static_lua_type():{
      const I_function_var* _fvar = dynamic_cast<const I_function_var*>(var);
      return _fvar->is_reference();
    }

    break; case I_object_var::get_static_lua_type():{
      return true;
    }
  }

  return false;
}

const void* lua::util::get_reference_pointer(const I_variant* var){
  if(!var)
    return NULL;

  switch(var->get_type()){
    break; case I_table_var::get_static_lua_type():{
      const I_table_var* _tvar = dynamic_cast<const I_table_var*>(var);
      return _tvar->get_table_pointer();
    }

    break; case I_function_var::get_static_lua_type():{
      const I_function_var* _fvar = dynamic_cast<const I_function_var*>(var);
      if(_fvar->is_cfunction())
        return _fvar->get_function();
      else
        return _fvar->get_lua_function_pointer();
    }
  }

  return NULL;
}