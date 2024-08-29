#include "error_trigger.h"

#include "Windows.h"


const char* _title_message = "Debugger Error";
const char* _generic_error_msg = "Something went wrong. Check logs for more information.";


void ErrorTrigger::trigger_generic_error_message(){
  trigger_error_message(_generic_error_msg);
}

void ErrorTrigger::trigger_error_message(const char* error_msg){
#if (_WIN64) || (_WIN32)
  MessageBoxA(
    NULL,
    error_msg,
    _title_message,
    MB_OK | MB_ICONERROR
  );
#endif
}