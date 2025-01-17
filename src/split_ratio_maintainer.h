#ifndef SPLIT_RATIO_MAINTAINER_HEADER
#define SPLIT_RATIO_MAINTAINER_HEADER

#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/split_container.hpp"

#include "map"


class SplitRatioMaintainer: public godot::Node{
  GDCLASS(SplitRatioMaintainer, godot::Node)

  private:
    struct split_container_metadata{
      public:
        enum split_axes{
          split_axes_x,
          split_axes_y
        };

        godot::SplitContainer* scontainer;
        split_axes axes;
        float ratio;
    };

    godot::Array _obj_list;
    bool _find_in_children = false;

    std::map<uint64_t, split_container_metadata> _container_metadata;

    void _on_split_dragged(int offset, godot::Node* node);
    void _on_size_changed(godot::Node* node);
    void _on_node_deleted(godot::Node* node);

    void _register_object(godot::SplitContainer* node);
    void _register_children(godot::Node* node);

    void _update_container_ratio(uint64_t id);
    void _resize_container(uint64_t id);

  protected:
    static void _bind_methods();

  public:
    void _ready() override;

    void set_target_object_list(const godot::Array& list);
    godot::Array get_target_object_list() const;

    void set_find_in_children(bool check);
    bool get_find_in_children() const;
};

#endif