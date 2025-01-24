#ifndef DLLUTIL_HEADER
#define DLLUTIL_HEADER

namespace dynamic_library::util{
  // A helper class that can be used in a DLL to delete an object within dynamic memory location on a DLL exit.
  // This class is useful to create a data in a dynamic memory at some point in a DLL code that serves as a static variable in a code file, which is prohibited to do in a initializing point of a DLL.
  // An example for this use case, is to create an std::map static variable at some point (maybe on some object constructor) then will be deleted when this DLL is no longer used in a program. 
  class destructor_helper{
    public:
      typedef void (*destructor_cb)();

    private:
      destructor_cb _cb;

    public:
      destructor_helper(destructor_cb cb);
      ~destructor_helper();
  };
}

#endif