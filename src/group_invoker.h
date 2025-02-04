#ifndef GROUP_INVOKER_HEADER
#define GROUP_INVOKER_HEADER

#include "godot_cpp/classes/node.hpp"

#include "map"
#include "set"


// NOTE:
//  - group_node_data property is consists of:
//    - KEY (STRING): key identifying a group of node
//    - VALUE (ARRAY): list of path to target node


class GroupInvoker: public godot::Node{
  GDCLASS(GroupInvoker, godot::Node)

  private:
    struct _group_data{
      std::set<godot::Node*> node_list;
    };

    godot::Dictionary _group_node_data;

    std::map<godot::String, _group_data*> _group_data_map;
    std::map<uint64_t, _group_data*> _node_group_lookup;

    void _on_node_removed(godot::Node* node);    

    void _clear_group_data_map();

  protected:
    static void _bind_methods();

  public:
    ~GroupInvoker();

    void _ready() override;

    void invoke(const godot::String& group_key, const godot::String& func_name, const godot::Array& parameter);
    
    void set_group_node_data(const godot::Dictionary& data);
    godot::Dictionary get_group_node_data() const;
    
    template<typename... T_Vargs> void invoke(const godot::String& group_key, const godot::String& func_name, const T_Vargs&... args){
      godot::Array _paramarr;
      ([&]{
        _paramarr.append(args);
      }(), ...);

      invoke(group_key, func_name, _paramarr);
    }
};

#endif