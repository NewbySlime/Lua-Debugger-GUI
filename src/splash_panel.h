#ifndef SPLASH_PANEL_HEADER
#define SPLASH_PANEL_HEADER

#include "godot_cpp/classes/animation_player.hpp"
#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/scene_tree_timer.hpp"


class SplashPanel: public godot::Control{
  GDCLASS(SplashPanel, godot::Control)

  private:
    float _inactivity_time = 3;
    godot::NodePath _reset_control_node_path;

    godot::AnimationPlayer* _anim_player;
    godot::Ref<godot::SceneTreeTimer> _inactivity_timer;

    bool _inactivity_timed_out = false;
    bool _mouse_entered_flag = false;

    void _mouse_entered();
    void _mouse_exited();

    void _inactivity_timer_timeout();

  protected:
    static void _bind_methods();

  public:
    void _ready() override;
    void _process(double delta) override;

    void set_inactivity_time(float time);
    float get_inactivity_time() const;

    void set_reset_control_node_path(godot::NodePath path);
    godot::NodePath get_reset_control_node_path() const;
};

#endif