#include "godot_cpp/variant/color.hpp"


namespace gdutils{
  godot::String as_hex(const godot::Color& col, bool include_alpha = false);
}