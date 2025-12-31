#ifndef bryte_vm_h
#define bryte_vm_h
#include "../../include/bytecode/object.h"
#include "../../include/bytecode/value.h"
#include "../bytecode/chunk.h"
#include "../datastructures/hashmap.h"
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)
typedef struct {
  ObjClosure *closure;
  uint8_t *ip;
  Value *slots;
} CallFrame;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frameCount;
  Value stack[STACK_MAX];
  Value *stackTop;
  Obj *objectsHead;
  Table strings; // for string objects to be interned
  Table globals; // for global variables

} VM;

extern VM vm;

void push(Value value);
Value pop();

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR

} InterpretResult;

void initVM();
void freeVM();
InterpretResult interpret(const char *source);

#endif
