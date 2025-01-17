#ifndef GDUTILS_HEADER
#define GDUTILS_HEADER

#include "godot_cpp/variant/color.hpp"
#include "godot_cpp/variant/variant.hpp"

#include "map"


namespace gdutils{
  godot::String as_hex(const godot::Color& col, bool include_alpha = false);

  godot::Variant parse_str_to_var(godot::Variant::Type expected_var, const godot::String& str, godot::String* fail_reason = NULL);

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
}

#endif