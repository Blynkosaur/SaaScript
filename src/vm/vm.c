#include "../../include/vm/vm.h"
#include "../../include/bytecode/object.h"
#include "../../include/common.h"
#include "../../include/compiler/compiler.h"
#include "../../include/debug.h"
#include "../../include/memory.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
VM vm;

static Value clockNative(int argCount, Value *args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}
static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
}
static void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm.frames[i];
    ObjFunction *function = frame->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in", function->chunk.lines[instruction]);
    if (function->name == NULL) {
      fprintf(stderr, "<script> \n");

    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }
  resetStack();
}
static void defineNative(const char *name, NativeFunction function) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));
  set(&vm.globals, vm.stack[1], PAYLOAD_STRING(vm.stack[0]));
  pop();
  pop();
}
static bool call(ObjFunction *function, int argCount) {
  if (argCount != function->arity) {
    runtimeError("Expected %d arguments but got %d", function->arity, argCount);
    return false;
  }
  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack Overflow");
    return false;
  }
  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->function = function;
  frame->ip = function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}
static bool callValue(Value callee, int argCount) {

  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
    case OBJ_FUNCTION:
      return call(PAYLOAD_FUNCTION(callee), argCount);
    case OBJ_NATIVE: {
      NativeFunction native = PAYLOAD_NATIVE(callee);
      Value result = native(argCount, vm.stackTop - argCount);
      vm.stackTop -= argCount + 1;
      push(result);
      return true;
    }
    default:
      break;
    }
  }
  runtimeError("Can only call functions");
  return false;
}
static bool isFalsey(Value value) {
  return IS_NULL(value) || (IS_BOOL(value) && !PAYLOAD_BOOL(value));
}

static void concatenate() {
  StringObj *last = PAYLOAD_STRING(pop());
  StringObj *second_last = PAYLOAD_STRING(pop());
  int new_len = last->length + second_last->length;
  char *concat = malloc(sizeof(char) * new_len + 1);
  strcpy(concat, second_last->chars);
  strcat(concat, last->chars);
  concat[new_len] = '\0';

  StringObj *result = makeObjWithString(concat, new_len);
  push(OBJ_VAL(result));
}

static InterpretResult run() {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];
#define READ_BYTE()                                                            \
  (*frame->ip++) // dereferences vm.ip (index pointer) and moves the pointer
                 // more
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() PAYLOAD_STRING(READ_CONSTANT())
#define READ_SHORT()                                                           \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
  // reads the two bytes for the jump operation
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      runtimeError("Operands have to be numbers.");                            \
    }                                                                          \
    double a = PAYLOAD_NUMBER(pop());                                          \
    double b = PAYLOAD_NUMBER(pop());                                          \
    push(valueType(b op a));                                                   \
  } while (false)

  printf("\n-----INTERPRETING-----\n\n");
  for (;;) {

#ifdef DEBUG_TRACE_EXECUTION // only for debugging
    printf("vm.ip: %p\n", frame->ip);
    printf("vm.chunk->code: %p, offset: %ld\n", &frame->function->chunk.code,
           frame->ip - frame->function->chunk.code);
    disassembleInstruction(&frame->function->chunk,
                           (int)(frame->ip - frame->function->chunk.code));

#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      printf("PUSHED: ");
      printValue(constant);
      push(constant);
      printf("\n");
      break;
    }
    case OP_GREATER:
      BINARY_OP(BOOL_VAL, >);
      break;
    case OP_LESS:
      BINARY_OP(BOOL_VAL, <);
      break;
    case OP_POP:
      pop();
      break;
    case OP_ADD: {
      if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
        concatenate();

      } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
        double last = PAYLOAD_NUMBER(pop());
        double second_last = PAYLOAD_NUMBER(pop());
        push(NUMBER_VAL(last + second_last));

      } else {
        runtimeError("Operands must be two numbers (addition) or two strings "
                     "(concatenation)");
        return INTERPRET_RUNTIME_ERROR;
      }
      // BINARY_OP(NUMBER_VAL, +);
      break;
    }
    case OP_SUBSTRACT: {
      BINARY_OP(NUMBER_VAL, -);
      break;
    }
    case OP_MULITPLY: {
      BINARY_OP(NUMBER_VAL, *);
      break;
    }
    case OP_DIVIDE: {

      BINARY_OP(NUMBER_VAL, /);
      break;
    }
    case OP_NEGATE: {

      // push(-pop());]
      if (!IS_NUMBER(peek(0))) {
        runtimeError("Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      // stackTop = pointer to a value
      (*(vm.stackTop - 1)).payload.number *= -1; // goofy aaah shit
      break;
    }
    case OP_NULL: {
      push(NULL_VAL);
      break;
    }
    case OP_FALSE: {
      push(BOOL_VAL(false));
      break;
    }
    case OP_TRUE: {
      push(BOOL_VAL(true));
      break;
    }
    case OP_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(BOOL_VAL(isEqual(a, b)));
      break;
    }
    case OP_NOT: {
      Value last = pop();
      Value new = BOOL_VAL(MAKE_NOT(last));
      push(new);
      break;
    }
    case OP_PRINT: {
      printf("Printed: ");
      printValue(pop());
      printf("\n");
      break;
    }
    case OP_DEFINE_GLOBAL: {
      StringObj *name = READ_STRING();
      set(&vm.globals, peek(0), name);
      pop();
      break;
    }
    case OP_SET_GLOBAL: {
      StringObj *name = READ_STRING();
      Entry *lookup =
          lookUp(&vm.globals, name->chars, name->hash, name->length);
      if (lookup != NULL) {
        set(&vm.globals, peek(0), name);
      } else {
        runtimeError("Undefined variable %s", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_GET_GLOBAL: {
      StringObj *name = READ_STRING();
      Entry *lookup =
          lookUp(&vm.globals, name->chars, name->hash, name->length);
      if (lookup == NULL) {

        runtimeError("Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      push(lookup->value);
      break;
    }
    case OP_GET_LOCAL: {
      // duplicates the existing slot in the VM stack to the top of the stack
      // for later use
      uint8_t slot = READ_BYTE();
      push(frame->slots[slot]);
      break;
    }
    case OP_SET_LOCAL: {
      // sets the slot to the top of the stack
      uint8_t slot = READ_BYTE();
      frame->slots[slot] = peek(0);
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (isFalsey(peek(0)))
        frame->ip += offset;
      break;
    }
    case OP_JUMP: {
      uint16_t offset = READ_SHORT();
      frame->ip += offset;
      break;
    }
    case OP_LOOP: {
      u_int16_t offset = READ_SHORT();
      frame->ip -= offset;
      break;
    }
    case OP_CALL: {
      int argCount = READ_BYTE();
      if (!callValue(peek(argCount), argCount)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      frame = &vm.frames[vm.frameCount - 1];
      break;
    }
    case OP_RETURN: {
      // printValue(pop());
      // printf("\n");
      Value result = pop();
      vm.frameCount--;
      if (vm.frameCount == 0) {
        pop();
        return INTERPRET_OK;
      }
      vm.stackTop = frame->slots;
      push(result);
      frame = &vm.frames[vm.frameCount - 1];
      break;
    }
    }
#ifdef DEBUG_TRACE_EXECUTION
    printf("\n         ");
    printf("VM stack:");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      // printValue(*slot);
      printf(" ]");
    }
    printf("\n\n");
    printf("VM table: ");
    if ((&vm.strings)->entries == NULL || (&vm.strings)->capacity == 0) {
      printf("Table is empty\n");
      continue;
    }

    Entry **entries = vm.strings.entries;
    for (int i = 0; i < vm.strings.capacity; i++) {
      if (entries[i] != NULL && entries[i]->key != NULL) {
        printf("[ ");
        printf("%s %p %p", entries[i]->key->chars, entries[i]->key, entries[i]);
        printf(" ]");
      }
    }
    printf("\n");
#endif
  }
#undef READ_BYTE
#undef READ_STRING
#undef READ_CONSTANT
#undef BINARY_OP
#undef READ_SHORT
}
InterpretResult interpret(const char *source) {
  ObjFunction *function = compile(source);
  if (function == NULL)
    return INTERPRET_COMPILE_ERROR;
  push(OBJ_VAL(function));
  call(function, 0);
  return run();
}
void initVM() {
  vm.objectsHead = NULL;
  resetStack();
  initTable(&vm.strings);
  initTable(&vm.globals);
  defineNative("clock", clockNative);
}

void push(Value value) {
  *(vm.stackTop) = value;
  vm.stackTop++;
}
Value pop() {
  vm.stackTop--; // never explicitly removes the last element
  Value value = *(vm.stackTop);
  return value;
}
void freeVM() {
  freeObjects();
  freeTable(&vm.strings);
  freeTable(&vm.globals);
}
// typedef struct{
//     Chunk*chunk;
//     uint8_t* ip;
//     Value stack[STACK_MAX];
//     Value *stackTop;

// }VM;
