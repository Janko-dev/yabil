#ifndef _COMMON_H
#define _COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define NAN_BOXING
// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXECUTION
#define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC

#define UNUSED(val) (void)(val)
#define UINT24_COUNT ((size_t)1 << 12)

#endif //_COMMON_H