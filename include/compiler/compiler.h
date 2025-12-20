#ifndef bryte_compiler_h

#define bryte_compiler_h
#include "../bytecode/chunk.h"
#include "../bytecode/object.h"
#include "../common.h"

ObjFunction *compile(const char *source);

#endif
