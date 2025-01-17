#ifndef STDUTILS_HEADER
#define STDUTILS_HEADER

#include "map"

namespace stdutils{
  template<typename T_key, typename T_value> std::map<T_value, T_key> reverse_map(const std::map<T_key, T_value>& data){
    std::map<T_value, T_key> _res;
    for(auto _pair: data)
      _res[_pair.second] = _pair.first;

    return _res;
  }
}

#endif