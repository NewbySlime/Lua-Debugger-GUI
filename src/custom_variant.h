#ifndef CUSTOM_VARIANT_HEADER
#define CUSTOM_VARIANT_HEADER

#include "Lua-CPPAPI/Src/luavariant.h"

#include "map"

#define LUA_TLOCALTABLE 0x10001


class I_local_table_var: virtual public lua::I_table_var{
  public:
    constexpr static int get_static_lua_type(){return LUA_TLOCALTABLE;}

    // Might return false if changed to an invalid stack.
    virtual bool set_stack_idx(int idx) = 0;
    // Might return -1 if current stack is invalid.
    virtual int get_stack_idx() const = 0;
};

class local_table_var: public lua::variant, public I_local_table_var{
  private:
    lua::api::core _lc;

    const lua::I_variant** _key_list;
    std::map<lua::comparison_variant, int> _value_lookup_list;

    void* _debug_data = NULL;
    int _stack_idx = -1;

    void _update_key_list();
    void _resize_key_list(size_t size);
    void _clear_key_list();

    lua::I_variant* _get_value(const lua::I_variant* key) const;

    void _this_init();
    void _this_init(const I_local_table_var* var);
    void _this_clear();

  public:
    local_table_var(const lua::api::core* lua_core, int stack_idx);
    local_table_var(const local_table_var& var);
    local_table_var(const I_local_table_var* var);
    ~local_table_var();

    int get_type() const override;
    bool is_type(int type) const override;

    void push_to_stack(const lua::api::core* lua_core) const override;

    std::string to_string() const;
    void to_string(I_string_store* pstring) const override;

    // from_statexx functions will not be used.
    bool from_state(const lua::api::core* lua_core, int stack_idx) override;
    bool from_state_copy(const lua::api::core* lua_core, int stack_idx, bool recursive = true) override;
    bool from_object(const lua::I_object_var* obj) override;

    // This function wil reuse push_to_stack.
    void push_to_stack_copy(const lua::api::core* lua_core) const override;

    const lua::I_variant** get_keys() const override;

    void update_keys() override;

    lua::I_variant* get_value( const lua::I_variant* key) override;
    const lua::I_variant* get_value(const lua::I_variant* key) const override;

    void set_value(const lua::I_variant* key, const I_variant* data) override;

    bool remove_value(const lua::I_variant* key) override;

    // This will replace all local values with NIL values.
    void clear_table() override;

    size_t get_size() const override;

    // Will not be used.
    void as_copy(bool recursive = true) override;
    // Will not be used.
    void remove_reference_values(bool recursive = true) override;

    // Always returns false.
    bool is_reference() const override;
    // Always returns NULL.
    const void* get_table_pointer() const override;
    const lua::api::core* get_lua_core() const override;

    void free_variant(const lua::I_variant* var) const override;
  
    bool set_stack_idx(int stack_idx) override;
    int get_stack_idx() const override;
};

#endif