#ifndef canidae_type_conversions_h

#define canidae_type_conversions_h

#include "value.h"

value to_str(VM *vm, value arg);
value to_num(VM *vm, value arg);
value to_bool(VM *vm, value arg);

#endif