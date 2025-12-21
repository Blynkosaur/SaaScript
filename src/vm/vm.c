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

VM vm;

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

  size_t instruction = vm.ip - vm.chunk->code - 1;
  int line = vm.chunk->lines[instruction];
  fprintf(stderr, "[line %d] in script \n", line);
  resetStack();
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
    printf("vm.ip: %p\n", vm.ip);
    printf("vm.chunk->code: %p, offset: %d", vm.chunk->code,
           vm.ip - vm.chunk->code);
    disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));

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
      push(vm.stack[slot]);
      break;
    }
    case OP_SET_LOCAL: {
      // sets the slot to the top of the stack
      uint8_t slot = READ_BYTE();
      vm.stack[slot] = peek(0);
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (isFalsey(peek(0)))
        vm.ip += offset;
      break;
    }
    case OP_JUMP: {
      uint16_t offset = READ_SHORT();
      vm.ip += offset;
      break;
    }
    case OP_LOOP: {
      u_int16_t offset = READ_SHORT();
      vm.ip -= offset;
      break;
    }
    case OP_RETURN: {
      // printValue(pop());
      // printf("\n");
      return INTERPRET_OK;
    }
    }
#ifdef DEBUG_TRACE_EXECUTION
    printf("\n         ");
    printf("VM stack:");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
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
  Chunk *chunk = malloc(sizeof(Chunk));
  initChunk(chunk);
  if (!compile(source, chunk)) {
    freeChunk(chunk);
    printf("INTERPRET_COMPILE_ERROR\n");
    freeChunk(chunk);
    free(chunk);
    return INTERPRET_COMPILE_ERROR;
  }
  vm.chunk = chunk;
  vm.ip = vm.chunk->code;
  InterpretResult result = run();
  freeChunk(chunk);
  free(chunk);
  return result;
}

void initVM() {
  vm.objectsHead = NULL;
  resetStack();
  initTable(&vm.strings);
  initTable(&vm.globals);
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
