#ifndef SLIDE_ANIMATION_CONTROL_HEADER
#define SLIDE_ANIMATION_CONTROL_HEADER

#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/variant/node_path.hpp"


class SlideAnimationControl: public godot::Node{
  GDCLASS(SlideAnimationControl, godot::Node)

  public:
    enum slide_direction{
      slide_direction_up,
      slide_direction_down,
      slide_direction_right,
      slide_direction_left
    };

    enum slide_pivot{
      slide_pivot_top,
      slide_pivot_top_left,
      slide_pivot_left,
      slide_pivot_bottom_left,
      slide_pivot_bottom,
      slide_pivot_bottom_right,
      slide_pivot_right,
      slide_pivot_top_right
    };

  private:
    godot::NodePath _target_node;
    int _direction = slide_direction_up;

    int _pivot = slide_pivot_top_left;

    float _slide_value = 0;

    void _on_update_size();
    void _update_node();

    void _try_bind_node();
    void _try_unbind_node();

  protected:
    static void _bind_methods();

  public:
    void _ready() override;

    void set_slide_value(float value);
    float get_slide_value() const;

    void set_target_node(const godot::NodePath& target_node);
    godot::NodePath get_target_node() const;

    void set_slide_direction(int dir);
    int get_slide_direction() const;

    void set_slide_pivot(int pivot);
    int get_slide_pivot() const;
};

#endif