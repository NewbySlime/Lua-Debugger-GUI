#ifndef NODE_DATA_HEADER
#define NODE_DATA_HEADER

#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/variant/variant.hpp"


class NodeData: public godot::Node{
  GDCLASS(NodeData, godot::Node)

  private:
    godot::Dictionary _user_data;

  protected:
    static void _bind_methods();

  public:
    godot::Dictionary get_user_data_dict() const;
    void set_user_data_dict(const godot::Dictionary& user_data);

    godot::Variant get_user_data(const godot::Variant& key) const;
    void set_user_data(const godot::Variant& key, const godot::Variant& value);
    
};

#endif