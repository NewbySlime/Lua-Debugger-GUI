#include "custom_variant.h"

#include "Lua-CPPAPI/Src/luaapi_debug.h"
#include "Lua-CPPAPI/Src/luaapi_stack.h"
#include "Lua-CPPAPI/Src/luaapi_thread.h"
#include "Lua-CPPAPI/Src/luaapi_value.h"
#include "Lua-CPPAPI/Src/luaapi_variant_util.h"
#include "Lua-CPPAPI/Src/luavariant_util.h"


using namespace lua;
using namespace lua::api;
using namespace ::memory;


void _code_init(){
  custom_variant_data _local_var_data;
    _local_var_data.copy_function = [](const I_variant* var, const I_dynamic_management* dm){
      return (variant*)dm->new_class_dbg<local_table_var>(DYNAMIC_MANAGEMENT_DEBUG_DATA, dynamic_cast<const I_local_table_var*>(var));
    };

  if(!cpplua_has_custom_type(LUA_TLOCALTABLE))
    cpplua_register_custom_type(LUA_TLOCALTABLE, &_local_var_data);
}


local_table_var::local_table_var(const core* lc, int stack_idx){
  _code_init();

  _lc = *lc;
  _this_init();
  
  set_stack_idx(stack_idx);
  _update_key_list();
}

local_table_var::local_table_var(const local_table_var& var){
  _code_init();
  _this_init(&var);
}

local_table_var::local_table_var(const I_local_table_var* var){
  _code_init();
  _this_init(var);
}

local_table_var::~local_table_var(){
  _this_clear();
}


void local_table_var::_update_key_list(){
  _lc.context->api_thread->lock_state(_lc.istate);
  _lc.context->api_thread->thread_dependent_enable(_lc.istate, false);

{ // enclosure for using gotos
  _clear_key_list();
  _resize_key_list(0);
  
  if(_stack_idx < 0)
    goto skip_to_return;
  
  std::vector<std::string> _key_list_data;
  for(int i = 1; ; i++){
    const char* _key_name = _lc.context->api_debug->getlocal(_lc.istate, _debug_data, i);
    if(!_key_name)
      break;
    
    _lc.context->api_stack->pop(_lc.istate, 1);
    _key_list_data.insert(_key_list_data.end(), _key_name);
  }
  
  _resize_key_list(_key_list_data.size());
  
  for(int i = 0; i < _key_list_data.size(); i++){
    string_var _str_data = _key_list_data[i];
    _value_lookup_list[_str_data] = i+1;
    _key_list[i] = cpplua_create_var_copy(&_str_data);
  }
} // enclosure closing
  
  skip_to_return:{}
  
  _lc.context->api_thread->thread_dependent_enable(_lc.istate, true);
  _lc.context->api_thread->unlock_state(_lc.istate);
}

void local_table_var::_resize_key_list(size_t size){
  size += 1;
  _key_list = (const I_variant**)realloc(_key_list, size*sizeof(I_variant*));
  _key_list[size-1] = NULL;
}

void local_table_var::_clear_key_list(){
  for(int i = 0; _key_list[i]; i++)
    cpplua_delete_variant(_key_list[i]);

  _value_lookup_list.clear();
}


I_variant* local_table_var::_get_value(const lua::I_variant* key) const{
  I_variant* _res = NULL;

  _lc.context->api_thread->lock_state(_lc.istate);
  _lc.context->api_thread->thread_dependent_enable(_lc.istate, false);

{ // enclosure for using gotos
  if(_stack_idx < 0)
    goto skip_to_return;
  
  auto _iter = _value_lookup_list.find(key);
  if(_iter == _value_lookup_list.end())
    goto skip_to_return;
  
  const char* _val_name = _lc.context->api_debug->getlocal(_lc.istate, _debug_data, _iter->second);
  if(!_val_name)
    goto skip_to_return;
  
  _res = to_variant(&_lc, -1);
  _lc.context->api_stack->pop(_lc.istate, 1);
} // enclosure closing
  
  skip_to_return:{}

  _lc.context->api_thread->thread_dependent_enable(_lc.istate, true);
  _lc.context->api_thread->unlock_state(_lc.istate);
  
  return _res;
}


void local_table_var::_this_init(){
  _debug_data = _lc.context->api_debug->create_lua_debug_obj();

  _key_list = (const I_variant**)malloc(0);
  _resize_key_list(0);
}

void local_table_var::_this_init(const I_local_table_var* var){
  _lc = *var->get_lua_core();
  _this_init();

  set_stack_idx(var->get_stack_idx());
  _update_key_list();
}

void local_table_var::_this_clear(){
  _lc.context->api_debug->delete_lua_debug_obj(_debug_data);

  _clear_key_list();
  free(_key_list);
}


int local_table_var::get_type() const{
  return LUA_TLOCALTABLE;
}

bool local_table_var::is_type(int type) const{
  return (type == LUA_TLOCALTABLE) || (type == LUA_TTABLE);
}


void local_table_var::push_to_stack(const core* lc) const{
  _lc.context->api_thread->lock_state(_lc.istate);
  _lc.context->api_thread->thread_dependent_enable(_lc.istate, false);

  lc->context->api_value->newtable(lc->istate);

  for(int i = 1; ; i++){
    const char* _local_name = _lc.context->api_debug->getlocal(_lc.istate, _debug_data, i);
    if(!_local_name)
      break;

    // get variant object
    I_variant* _value_data = _lc.context->api_varutil->to_variant(_lc.istate, -1);
    _lc.context->api_stack->pop(_lc.istate, 1);

    // copy variant to target state
    _value_data->push_to_stack(lc);
    lc->context->api_value->setfield(lc->istate, -2, _local_name);

    // delete variant object
    _lc.context->api_varutil->delete_variant(_value_data);
  }

  _lc.context->api_thread->thread_dependent_enable(_lc.istate, true);
  _lc.context->api_thread->unlock_state(_lc.istate);
}


std::string local_table_var::to_string() const{
  return "Local Table";
}

void local_table_var::to_string(I_string_store* pstring) const{
  std::string _str = to_string();
  pstring->append(_str.c_str());
}


bool local_table_var::from_state(const core* lc, int stack_idx){
  return false;
}

bool local_table_var::from_state_copy(const core* lc, int stack_idx){
  return false;
}

bool local_table_var::from_object(const I_object_var* obj){
  return false;
}


void local_table_var::push_to_stack_copy(const core* lc) const{
  return push_to_stack(lc);
}


const I_variant** local_table_var::get_keys() const{
  return _key_list;
}


void local_table_var::update_keys(){
  _update_key_list();
}


I_variant* local_table_var::get_value(const I_variant* key){
  return _get_value(key);
}

const I_variant* local_table_var::get_value(const I_variant* key) const{
  return _get_value(key);
}


void local_table_var::set_value(const I_variant* key, const I_variant* value){
  _lc.context->api_thread->lock_state(_lc.istate);
  _lc.context->api_thread->thread_dependent_enable(_lc.istate, false);

{ // enclosure for using gotos
  if(_stack_idx < 0)
    goto skip_to_return;
  
  auto _iter = _value_lookup_list.find(key);
  if(_iter == _value_lookup_list.end())
    goto skip_to_return;
  
  value->push_to_stack(&_lc);
  
  const char* _val_name = _lc.context->api_debug->setlocal(_lc.istate, _debug_data, _iter->second);
  if(!_val_name){
    // pop unpopped value
    _lc.context->api_stack->pop(_lc.istate, 1);
  }
} // enclosure closing

  skip_to_return:{}

  _lc.context->api_thread->thread_dependent_enable(_lc.istate, true);
  _lc.context->api_thread->unlock_state(_lc.istate);
}


bool local_table_var::remove_value(const I_variant* key){
  bool _res = false;
  
  _lc.context->api_thread->lock_state(_lc.istate);
  _lc.context->api_thread->thread_dependent_enable(_lc.istate, false);
  
{ // enclosure for using gotos
  if(_stack_idx < 0)
    goto skip_to_return;
  
  auto _iter = _value_lookup_list.find(key);
  if(_iter == _value_lookup_list.end())
    goto skip_to_return;
  
  _lc.context->api_value->pushnil(_lc.istate);
  
  const char* _val_name = _lc.context->api_debug->setlocal(_lc.istate, _debug_data, _iter->second);
  if(!_val_name){
    // pop unpopped value
    _lc.context->api_stack->pop(_lc.istate, 1);
    goto skip_to_return;
  }
  
  _res = true;
} // enclosure closing

  skip_to_return:{}

  _lc.context->api_thread->thread_dependent_enable(_lc.istate, true);
  _lc.context->api_thread->unlock_state(_lc.istate);

  return _res;
}


void local_table_var::clear_table(){
  if(_stack_idx < 0)
    return;

  for(int i = 0; _key_list[i]; i++)
    remove_value(_key_list[i]);
}


void local_table_var::as_copy(){

}


bool local_table_var::is_reference() const{
  return false;
}

const void* local_table_var::get_table_pointer() const{
  return NULL;
}

const core* local_table_var::get_lua_core() const{
  return &_lc;
}


void local_table_var::free_variant(const I_variant* var) const{
  cpplua_delete_variant(var);
}


bool local_table_var::set_stack_idx(int stack_idx){
  bool _res = false;

  _lc.context->api_thread->lock_state(_lc.istate);
  _lc.context->api_thread->thread_dependent_enable(_lc.istate, false);

{ // enclosure for using gotos
  _stack_idx = stack_idx;
  int _result_code = _lc.context->api_debug->getstack(_lc.istate, stack_idx, _debug_data);
  if(!_result_code){
    _stack_idx = -1;
    goto skip_to_return;
  }

  _res = true;
} // enclosure closing

  skip_to_return:{}
  _update_key_list();

  _lc.context->api_thread->thread_dependent_enable(_lc.istate, true);
  _lc.context->api_thread->unlock_state(_lc.istate);
  
  return _res;
}

int local_table_var::get_stack_idx() const{
  return _stack_idx;
}