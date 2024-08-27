#ifndef NODE_UTILS_HEADER
#define NODE_UTILS_HEADER

#include "godot_cpp/classes/node.hpp"


// Finds any node based on the type T_Node.
// The function uses BFS to search.
template<typename T_Node> godot::Node* get_any_node(godot::Node* parent, bool recursive = false){
  for(int i = 0; i < parent->get_child_count(); i++){
    godot::Node* _child = parent->get_child(i);
    if(_child->get_class() == T_Node::get_class_static())
      return _child;
  }

  for(int i = 0; i < parent->get_child_count(); i++)
    get_any_node<T_Node>(parent->get_child());
} 


#endif