#ifndef CHOSEN_STATE_MODULATE_HEADER
#define CHOSEN_STATE_MODULATE_HEADER

#include "godot_cpp/classes/node.hpp"

#include "vector"


class ChosenStateModulate: public godot::Node{
  GDCLASS(ChosenStateModulate, godot::Node)

  private:
    godot::Color _chosen_color;
    godot::Color _unchosen_color;

    godot::Array _modified_list;

    std::vector<godot::Callable> _modulate_cb_list;

  protected:
    static void _bind_methods();

  public:
    void _ready() override;

    void set_chosen(bool flag);

    void set_chosen_color(const godot::Color& col);
    godot::Color get_chosen_color() const;

    void set_unchosen_color(const godot::Color& col);
    godot::Color get_unchosen_color() const;

    void set_modified_list(const godot::Array& list);
    godot::Array get_modified_list() const;
};

#endif