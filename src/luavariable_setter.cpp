#include "luavariable_setter.h"


using namespace lua;
using namespace lua::debug;
using namespace lua::util;



// MARK: GlobalVariableSetter def

GlobalVariableSetter::GlobalVariableSetter(I_variable_watcher* vwatch){
  _vwatch = vwatch;
}

GlobalVariableSetter::~GlobalVariableSetter(){

}


bool GlobalVariableSetter::set_value(const I_variant* value, const I_variant* key){
  nil_var _tmp;
  if(!key)
    key = &_tmp;
  
  return _vwatch->set_global_variable(key, value);
}



// MARK: LocalVariableSetter def

LocalVariableSetter::LocalVariableSetter(I_variable_watcher* vwatch){
  _vwatch = vwatch;
}

LocalVariableSetter::~LocalVariableSetter(){

}


bool LocalVariableSetter::set_value(const I_variant* value, const I_variant* key){
  nil_var _tmp;
  if(!key)
    key = &_tmp;

  return _vwatch->set_local_variable(key, value);
}



// MARK: TableVariableSetter

TableVariableSetter::TableVariableSetter(I_table_var* var){
  _tvar = dynamic_cast<lua::I_table_var*>(cpplua_create_var_copy(var));
}

TableVariableSetter::~TableVariableSetter(){
  cpplua_delete_variant(_tvar);
}


bool TableVariableSetter::set_value(const I_variant* value, const I_variant* key){
  nil_var _tmp;
  if(!key)
    key = &_tmp;

  _tvar->set_value(key, value);
  return true;
}