#include "signal_ownership.h"
#include "logger.h"
#include "strutil.h"


using namespace gdutils;
using namespace godot;


SignalOwnership::SignalOwnership(const Signal& signal, const Callable& cb){
  _bound_signal = signal;
  _bound_cb = cb;
}


void SignalOwnership::set_bind_callback(const Callable& cb){
  _bound_cb = cb;
}

Callable SignalOwnership::get_bind_callback() const{
  return _bound_cb;
}


void SignalOwnership::replace_ownership(){
  if(!_bound_cb.is_valid())
    return;

  bool _is_connected = false;
  Array _cb_list = _bound_signal.get_connections();
  for(int i = 0; i < _cb_list.size(); i++){
    Dictionary _conn = _cb_list[i];
    Callable _cb = _conn["callable"];
    if(_cb  == _bound_cb){
      _is_connected = true;
      continue;
    }

    _bound_signal.disconnect(_cb);
  }

  if(_is_connected)
    return;

  _bound_signal.connect(_bound_cb);
}