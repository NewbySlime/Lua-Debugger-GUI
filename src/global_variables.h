#ifndef GLOBAL_VARIABLES_HEADER
#define GLOBAL_VARIABLES_HEADER

#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/string.hpp"

#include "gdutils.h"


class GlobalVariables: public godot::Node{
  GDCLASS(GlobalVariables, godot::Node)

  public:
    static const char* singleton_path;

    // Param:
    //  - STRING: key
    //  - ANY: value
    static const char* s_global_value_set;

  private:
    godot::Dictionary _variable_data;
    gdutils::VariantTypeParser _type_parser;

  protected:
    static void _bind_methods();

  public:
    void set_global_value(const godot::String& key, const godot::Variant& value);
    // returns a nil variant if no variable for certain key.
    godot::Variant get_global_value(const godot::String& key) const;
    bool has_global_value(const godot::String& key) const;

    const gdutils::VariantTypeParser& get_type_parser() const;

    void set_global_data(const godot::Dictionary& data);
    godot::Dictionary get_global_data() const;
};

#endif