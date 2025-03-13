#ifndef REFERENCE_QUERY_MENU_HEADER
#define REFERENCE_QUERY_MENU_HEADER

#include "godot_cpp/classes/button.hpp"
#include "godot_cpp/classes/control.hpp"
#include "godot_cpp/classes/label.hpp"
#include "godot_cpp/classes/packed_scene.hpp"

#include "Lua-CPPAPI/Src/luavariant.h"

#include "map"


class ReferenceQueryMenu: public godot::Control{
  GDCLASS(ReferenceQueryMenu, godot::Control)

  public:
    struct ReferenceQueryFunction{
      // Callback parameter:
      //  - INT: query item offset
      //  - INT: query item length
      //  - PACKED_BYTE_ARRAY: data of ReferenceQueryResult (result data for caller)
      godot::Callable query_data;
      // Callback result:
      //  - INT: item count
      godot::Callable query_item_count;
      // Callback parameter:
      //  - INT: item ID
      // Callback result:
      //  - PACKED_BYTE_ARRAY: data of ReferenceFetchValueResult
      godot::Callable fetch_value_item;
    };

    struct ReferenceQueryResult{
      // NOTE: This object should be filled before calling certain functions
      // This dictionary a list of:
      //  [KEY] (INT): The ID of the content
      //  [VALUE] (DICT): A dictionary of:
      //    ["content_preview"] (STRING): Preview of the context
      godot::Dictionary* query_list;
    };

    struct ReferenceFetchValueResult{
      // This can result in NULL if the item ID is not valid.
      // NOTE: Don't forget to free the variant
      lua::I_variant* value = NULL;
    };

  private:
    enum _button_type{
      button_type_first,
      button_type_before,
      button_type_next,
      button_type_last
    };

    struct _content_metadata{
      godot::Callable group_invoke;
    };

    godot::NodePath _label_page_info_path;
    godot::Label* _label_page_info;

    godot::NodePath _first_button_path;
    godot::Button* _first_button;

    godot::NodePath _before_button_path;
    godot::Button* _before_button;

    godot::NodePath _next_button_path;
    godot::Button* _next_button;

    godot::NodePath _last_button_path;
    godot::Button* _last_button;

    godot::NodePath _content_preview_pivot_path;
    godot::Node* _content_preview_pivot;

    // This should contains:
    //  GroupInvoker, that has a group of
    //    "content_text": holds preview contents, which should have function of:
    //      "set_text": to set a text of a content
    //      "set_tooltip_text": to set a tooltip text
    //    "content_button": holds button contents, which should have signal of:
    //      "pressed": when the button is pressed
    //    "content_state": set configuration of the state of the content, which should have function of:
    //      "set_choosen": to set if the content is being chosen
    godot::Ref<godot::PackedScene> _content_preview_pckscene;

    std::map<uint64_t, _content_metadata> _content_metadata_map;
    uint64_t _chosen_content_id;

    size_t _items_per_page = 10;

    // starts with 0
    int _current_page = 0;

    ReferenceQueryFunction _query_func_data;

    void _on_content_button_pressed(uint64_t id);
    void _on_button_pressed(int type);

    void _update_reference_page();


  protected:
    static void _bind_methods();

  public:
    void _ready() override;

    void set_page(int page);
    int get_page() const;

    void set_reference_query_function_data(const ReferenceQueryFunction& data);
    ReferenceQueryFunction get_reference_query_function_data() const;

    void choose_reference(uint64_t content_id);
    uint64_t get_chosen_reference_id();

    void update_reference_page();

    godot::NodePath get_label_page_info_path() const;
    void set_label_page_info_path(const godot::NodePath& path);

    godot::NodePath get_first_button_path() const;
    void set_first_button_path(const godot::NodePath& path);

    godot::NodePath get_before_button_path() const;
    void set_before_button_path(const godot::NodePath& path);

    godot::NodePath get_next_button_path() const;
    void set_next_button_path(const godot::NodePath& path);

    godot::NodePath get_last_button_path() const;
    void set_last_button_path(const godot::NodePath& path);

    size_t get_items_per_page() const;
    void set_items_per_page(size_t item_count);

    godot::NodePath get_content_preview_pivot_path() const;
    void set_content_preview_pivot_path(const godot::NodePath& path);

    godot::Ref<godot::PackedScene> get_content_preview_pckscene() const;
    void set_content_preview_pckscene(godot::Ref<godot::PackedScene> scene);
};

#endif