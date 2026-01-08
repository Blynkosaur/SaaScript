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
  vm.openUpvalues = NULL;
}
static void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm.frames[i];
    ObjFunction *function = frame->closure->function;
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
static bool call(ObjClosure *closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d arguments but got %d", closure->function->arity,
                 argCount);
    return false;
  }
  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack Overflow");
    return false;
  }
  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}
static bool callValue(Value callee, int argCount) {

  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
    case OBJ_CLOSURE:
      return call(PAYLOAD_CLOSURE(callee), argCount);
    case OBJ_FUNCTION:
      return call(PAYLOAD_CLOSURE(callee), argCount);
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

// Forward declarations - these are in object.c

static Value arrayPushNative(int argCount, Value *args) {
  // args[0] is the array (receiver), args[1] is the element to push
  if (argCount != 2) {
    runtimeError("push expects 1 argument, got %d", argCount - 1);
    return NULL_VAL;
  }
  Value arrayVal = args[0];
  Value element = args[1];
  if (!IS_ARRAY(arrayVal)) {
    runtimeError("push called on non-array");
    return NULL_VAL;
  }
  arrayPush(arrayVal, element);
  return arrayVal;
}

static Value arrayPopNative(int argCount, Value *args) {
  // args[0] is the array (receiver)
  if (argCount != 1) {
    runtimeError("pop expects 0 arguments, got %d", argCount - 1);
    return NULL_VAL;
  }
  Value arrayVal = args[0];
  if (!IS_ARRAY(arrayVal)) {
    runtimeError("pop called on non-array");
    return NULL_VAL;
  }
  return arrayPop(arrayVal);
}
static ObjUpvalue *captureUpvalue(Value *local) {
  ObjUpvalue *prev = NULL;
  ObjUpvalue *current = vm.openUpvalues;
  while (current != NULL && current->location > local) {
    prev = current;
    current = current->next;
  }
  if (current != NULL && current->location == local) {
    return current;
  }
  ObjUpvalue *createdUpvalue = newUpvalue(local);
  createdUpvalue->next = current;
  if (prev == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prev->next = createdUpvalue;
  }
  return createdUpvalue;
}
static void closeUpvalues(Value *last) {
  while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
    ObjUpvalue *upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
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
#define READ_CONSTANT()                                                        \
  (frame->closure->function->chunk.constants.values[READ_BYTE()])
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

#ifdef DEBUG_PRINT_CODE
  printf("\n-----INTERPRETING-----\n\n");
#endif
  for (;;) {

#ifdef DEBUG_TRACE_EXECUTION // only for debugging
    printf("vm.ip: %p\n", frame->ip);
    printf("vm.chunk->code: %p, offset: %ld\n",
           &frame->closure->function->chunk.code,
           frame->ip - frame->closure->function->chunk.code);
    disassembleInstruction(
        &frame->closure->function->chunk,
        (int)(frame->ip - frame->closure->function->chunk.code));

#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
#ifdef DEBUG_PRINT_CODE
      printf("PUSHED: ");
      printValue(constant);

#endif
      push(constant);
#ifdef DEBUG_PRINT_CODE
      printf("\n");
#endif
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
      // printf(">> ");
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
    case OP_GET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      push(*frame->closure->upvalues[slot]->location);
      break;
    }
    case OP_SET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      *frame->closure->upvalues[slot]->location = peek(0);
      break;
    }
    case OP_CLOSE_UPVALUE: {
      closeUpvalues(vm.stackTop - 1);
      pop();
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
      Value callee = peek(argCount);
      // For method calls on arrays: stack is [array, native_function, ...args]
      // We need to rearrange to [native_function, array, ...args] before
      // calling
      if (IS_NATIVE(callee) && argCount >= 0) {
        // Check if the value before the callee is an array (the receiver)
        Value *stackBeforeCallee = vm.stackTop - argCount - 2;
        if (stackBeforeCallee >= vm.stack && IS_ARRAY(*stackBeforeCallee)) {
          // Rearrange stack: move array to be first argument
          Value array = *stackBeforeCallee;
          // Shift everything down
          for (Value *p = stackBeforeCallee; p < vm.stackTop - 1; p++) {
            *p = *(p + 1);
          }
          // Put array as first argument (right before callee)
          vm.stackTop[-argCount - 1] = array;
          // Now call with array as first arg
          if (!callValue(callee, argCount + 1)) {
            return INTERPRET_RUNTIME_ERROR;
          }
        } else {
          if (!callValue(callee, argCount)) {
            return INTERPRET_RUNTIME_ERROR;
          }
        }
      } else {
        if (!callValue(callee, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
      }
      frame = &vm.frames[vm.frameCount - 1];
      break;
    }
    case OP_CLOSURE: {
      ObjFunction *function = PAYLOAD_FUNCTION(READ_CONSTANT());
      ObjClosure *closure = newClosure(function);
      push(OBJ_VAL(closure));
      for (int i = 0; i < closure->upvalueCount; i++) {
        uint8_t isLocal = READ_BYTE();
        uint8_t index = READ_BYTE();
        if (isLocal) {
          closure->upvalues[i] = captureUpvalue(frame->slots + index);

        } else {
          closure->upvalues[i] = frame->closure->upvalues[index];
        }
      }
      break;
    }
    case OP_RETURN: {
      // printValue(pop());
      // printf("\n");
      Value result = pop();
      closeUpvalues(frame->slots);
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
    case OP_ARRAY: {
      int elementCount = READ_BYTE();
      ObjArray *array = newArray();
      // Pop elements from stack and store them temporarily
      // Stack has elements in order: [elem0, elem1, elem2, ...]
      // We need to reverse them when popping, then add in correct order
      Value *tempElements = malloc(sizeof(Value) * elementCount);
      for (int i = elementCount - 1; i >= 0; i--) {
        tempElements[i] = pop();
      }
      // Now add them to array in correct order
      for (int i = 0; i < elementCount; i++) {
        arrayPush(OBJ_VAL(array), tempElements[i]);
      }
      free(tempElements);
      push(OBJ_VAL(array));
      break;
    }
    case OP_GET_INDEX: {
      Value indexVal = pop();
      Value arrayVal = pop();
      if (!IS_ARRAY(arrayVal)) {
        runtimeError("Index operation on non-array");
        return INTERPRET_RUNTIME_ERROR;
      }
      if (!IS_NUMBER(indexVal)) {
        runtimeError("Array index must be a number");
        return INTERPRET_RUNTIME_ERROR;
      }
      int index = (int)PAYLOAD_NUMBER(indexVal);
      Value element = arrayGet(arrayVal, index);
      push(element);
      break;
    }
    case OP_SET_INDEX: {
      Value value = pop();
      Value indexVal = pop();
      Value arrayVal = pop();
      if (!IS_ARRAY(arrayVal)) {
        runtimeError("Index assignment on non-array");
        return INTERPRET_RUNTIME_ERROR;
      }
      if (!IS_NUMBER(indexVal)) {
        runtimeError("Array index must be a number");
        return INTERPRET_RUNTIME_ERROR;
      }
      int index = (int)PAYLOAD_NUMBER(indexVal);
      arraySet(arrayVal, index, value);
      push(value);
      break;
    }
    case OP_GET_PROPERTY: {
      StringObj *name = READ_STRING();
      Value object = peek(0);
      if (!IS_ARRAY(object)) {
        runtimeError("Property access on non-array");
        return INTERPRET_RUNTIME_ERROR;
      }
      // Handle array methods - create bound native functions
      if (strncmp(name->chars, "fund", name->length) == 0 &&
          name->length == 4) {
        // For method calls, we need to bind the array to the native function
        // Store array on stack, then replace with native function
        // When called, the native will receive the array as first argument
        ObjNative *native = newNative(arrayPushNative);
        // Keep array on stack, push native function
        push(OBJ_VAL(native));
      } else if (strncmp(name->chars, "churn", name->length) == 0 &&
                 name->length == 3) {
        ObjNative *native = newNative(arrayPopNative);
        push(OBJ_VAL(native));
      } else if (strncmp(name->chars, "arr", name->length) == 0 &&
                 name->length == 6) {
        Value length = arrayLength(object);
        pop(); // Remove array from stack
        push(length);
      } else {
        runtimeError("Unknown property '%.*s'", name->length, name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
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
  if (function == NULL) {
    if (vm.repl) {
      freeVM();
      initVM();
    }
    return INTERPRET_COMPILE_ERROR;
  }
  push(OBJ_VAL(function));
  ObjClosure *closure = newClosure(function);
  pop();
  push(OBJ_VAL(closure));

  call(closure, 0);
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
