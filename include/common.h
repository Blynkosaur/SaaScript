#if !defined bryte_common_h
#define bryte_common_h
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXECUTION
#define UINT8_COUNT                                                            \
  (UINT8_MAX + 1) // since the byte goes from 0 to uintmax so the total amount
// is uint8max + 1
#define SAAS_MODE
#endif
