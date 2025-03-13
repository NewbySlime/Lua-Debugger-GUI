#ifndef GDUTILS_HEADER
#define GDUTILS_HEADER

#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/variant/color.hpp"
#include "godot_cpp/variant/variant.hpp"

#include "map"


namespace gdutils{
  godot::String as_hex(const godot::Color& col, bool include_alpha = false);

  godot::Color construct_color(uint32_t hex);
  godot::Color construct_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0);

  godot::Variant parse_str_to_var(godot::Variant::Type expected_var, const godot::String& str, godot::String* fail_reason = NULL);

  template<typename... T_Vargs> godot::Array create_array(const T_Vargs&... args){
    godot::Array _result;
    ([&]{
      _result.append(args);
    }(), ...);

    return _result;
  }

  // The reason of class instead of static function approach to mitigate DLL restriction where the dynamic memory cannot be used while the DLL is initiating.
  class VariantTypeParser{
    private:
      std::map<godot::String, godot::Variant::Type> _str_type;

    public:
      VariantTypeParser();

      bool is_valid_type(const godot::String& type_str) const;
      godot::Variant::Type parse_str_to_type(const godot::String& type_str) const;
      godot::String parse_type_to_str(godot::Variant::Type type) const;
  };


  template<typename T_node> T_node* find_any_node(godot::Node* parent, bool recursive = false){
    for(int i = 0; i < parent->get_child_count(); i++){
      godot::Node* _child_node = parent->get_child(i);
      if(_child_node->is_class(T_node::get_class_static()))
        return dynamic_cast<T_node*>(_child_node);
    }

    if(recursive){
      for(int i = 0; i < parent->get_child_count(); i++){
        godot::Node* _child_node = parent->get_child(i);
        godot::Node* _result = find_any_node<T_node>(_child_node, true);
        if(_result)
          return dynamic_cast<T_node*>(_result);
      }
    }

    return NULL;
  }
}

#endif