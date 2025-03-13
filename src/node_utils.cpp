#include "node_utils.h"


using namespace godot;


void clear_child(Node* parent_node){
  while(parent_node->get_child_count() > 0){
    Node* _child_node = parent_node->get_child(0);
    parent_node->remove_child(_child_node);
  }
}