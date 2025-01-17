#include "defines.h"
#include "gdutils.h"
#include "logger.h"
#include "strutil.h"

#include "godot_cpp/classes/reg_ex.hpp"
#include "godot_cpp/classes/reg_ex_match.hpp"

using namespace gdutils;
using namespace godot;


String gdutils::as_hex(const Color& col, bool include_alpha){
  uint32_t _col =
    ((uint32_t)(col.r*255) << 16) |
    ((uint32_t)(col.g*255) << 8) |
    ((uint32_t)(col.b*255))
  ;

  if(include_alpha){
    _col = (_col << 2) | (uint32_t)(col.a*255);
    return format_str("%08x", _col).c_str();
  }
  
  return format_str("%06x", _col).c_str();
}


Variant gdutils::parse_str_to_var(Variant::Type expected_var, const String& str, String* fail_reason){
  switch(expected_var){
    break; case Variant::BOOL:{
      Ref<RegEx> _regex = RegEx::create_from_string("[^\t ]+");
      TypedArray<RegExMatch> _regex_match = _regex->search_all(str);

      if(_regex_match.size() <= 0)
        return false;

      RegExMatch& _match = ((RegExMatch&)_regex_match[0]);
      PackedStringArray _str_list = _match.get_strings();
      if(_str_list.size() <= 0)
        return false;

      return _str_list[0].to_lower() == "true";
    }
  }

  if(fail_reason)
    *fail_reason = gd_format_str("Parsing type {0} is not supported.", Variant::get_type_name(expected_var));
  
  return Variant();
}


// MARK: VariantTypeParser def
VariantTypeParser::VariantTypeParser(){
  for(int i = 0; i < Variant::VARIANT_MAX; i++){
    Variant::Type _type = (Variant::Type)i;
    _str_type[Variant::get_type_name(_type).to_lower()] = _type;
  }
}


bool VariantTypeParser::is_valid_type(const String& type_str) const{
  String _type_str = type_str.to_lower();
  auto _iter = _str_type.find(_type_str);
  return _iter != _str_type.end();
}

Variant::Type VariantTypeParser::parse_str_to_type(const String& type_str) const{
  String _type_str = type_str.to_lower();
  auto _iter = _str_type.find(_type_str);
  if(_iter == _str_type.end())
    return Variant::NIL;

  return _iter->second;
}

String VariantTypeParser::parse_type_to_str(Variant::Type type) const{
  return Variant::get_type_name(type);
}