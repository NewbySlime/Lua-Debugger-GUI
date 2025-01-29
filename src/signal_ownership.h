#ifndef SIGNAL_OWNERSHIP_HEADER
#define SIGNAL_OWNERSHIP_HEADER

#include "godot_cpp/variant/variant.hpp"


namespace gdutils{
  class SignalOwnership{
    private:
      godot::Signal _bound_signal;
      godot::Callable _bound_cb;

    public:
      SignalOwnership(const godot::Signal& signal, const godot::Callable& cb);

      void set_bind_callback(const godot::Callable& cb);
      godot::Callable get_bind_callback() const;

      void replace_ownership();

  };
}

#endif