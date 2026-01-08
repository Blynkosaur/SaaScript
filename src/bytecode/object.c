#include <stdio.h>
#include <string.h>

#include "../../include/bytecode/object.h"
#include "../../include/bytecode/value.h"
#include "../../include/datastructures/hashmap.h"
#include "../../include/memory.h"
#include "../../include/vm/vm.h"

// #define ALLOCATE_OBJ(type, objectType) ((type*) allocateObject(sizeof(type),
// objectType))

// static Obj * allocateObject (size_t size, ObjType type){
//     Obj* object = malloc(size);
//     object->type = type;
//     return object;
// }
ObjFunction *newFunction() {
  ObjFunction *function = malloc(sizeof(ObjFunction));
  function->arity = 0;
  function->obj.type = OBJ_FUNCTION;
  function->upvalueCount = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}
ObjNative *newNative(NativeFunction function) {
  ObjNative *native = malloc(sizeof(ObjNative));
  native->obj.type = OBJ_NATIVE;
  native->function = function;
  return native;
}
ObjClosure *newClosure(ObjFunction *function) {
  ObjUpvalue **upvalues = malloc(sizeof(ObjUpvalue *));
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }
  ObjClosure *closure = malloc(sizeof(ObjClosure));
  closure->obj.type = OBJ_CLOSURE;
  closure->function = function;
  closure->upvalueCount = function->upvalueCount;
  closure->upvalues = upvalues;
  return closure;
}
// FOR THE RECORD, IDK WHY THE CODE ABOVE IS EVEN THERE WHY SO COMPLICATED, JUST
// CALL MALLOC: IT AIN'T THAT DEEP
static StringObj *allocateString(char *chars, int length, uint32_t hash) {
  StringObj *string = malloc(sizeof(StringObj));
  string->obj.next = vm.objectsHead;
  vm.objectsHead = (Obj *)string;
  string->chars = chars;
  string->length = length;
  string->hash = hash;
  set(&vm.strings, NULL_VAL, string);
  return string;
}

static uint32_t hashFunc(const char *key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

StringObj *copyString(const char *chars, int length) {

  uint32_t hash = hashFunc(chars, length);

#ifdef DEBUG_PRINT_CODE
  printf("lookup from copyString with %s\n", chars);
#endif
  Entry *interned = lookUp(&vm.strings, chars, hash, length);
  if (interned != NULL) {
#ifdef DEBUG_PRINT_CODE
    printf("interned found from copystring at %p\n", interned);
#endif
    return interned->key;
  }
#ifdef DEBUG_PRINT_CODE
  printf("nothing found at copyString\n");
#endif
  char *heapChars = malloc(sizeof(char) * length + 1);
  if (heapChars == NULL) {
    printf("NULL pointer, nothing allocated \n");
  }
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';

  return allocateString(heapChars, length, hash);
}
ObjUpvalue *newUpvalue(Value *slot) {
  ObjUpvalue *upvalue = malloc(sizeof(ObjUpvalue));
  upvalue->obj.type = OBJ_UPVALUE;
  upvalue->location = slot;
  upvalue->next = NULL;
  upvalue->closed = NULL_VAL;
  return upvalue;
}

ObjArray *newArray() {
  ObjArray *array = malloc(sizeof(ObjArray));
  array->obj.type = OBJ_ARRAY;
  array->obj.next = vm.objectsHead;
  vm.objectsHead = (Obj *)array;
  initValueArray(&array->elements);
  return array;
}

void arrayPush(Value arrayVal, Value element) {
  ObjArray *array = PAYLOAD_ARRAY(arrayVal);
  writeValueArray(&array->elements, element);
}

Value arrayPop(Value arrayVal) {
  ObjArray *array = PAYLOAD_ARRAY(arrayVal);
  if (array->elements.count == 0) {
    return NULL_VAL;
  }
  Value popped = array->elements.values[array->elements.count - 1];
  array->elements.count--;
  return popped;
}

Value arrayLength(Value arrayVal) {
  ObjArray *array = PAYLOAD_ARRAY(arrayVal);
  return NUMBER_VAL((double)array->elements.count);
}

Value arrayGet(Value arrayVal, int index) {
  ObjArray *array = PAYLOAD_ARRAY(arrayVal);
  if (index < 0 || index >= array->elements.count) {
    return NULL_VAL;
  }
  return array->elements.values[index];
}

void arraySet(Value arrayVal, int index, Value value) {
  ObjArray *array = PAYLOAD_ARRAY(arrayVal);
  if (index < 0 || index >= array->elements.count) {
    return; // Out of bounds, do nothing
  }
  array->elements.values[index] = value;
}

static void printFunction(ObjFunction *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s", function->name->chars);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING: {
    printf("%s", PAYLOAD_CSTRING(value));
    break;
  }
  case OBJ_FUNCTION: {
    printFunction(PAYLOAD_FUNCTION(value));
    break;
  }
  case OBJ_NATIVE: {
    printf("<native function");
    break;
  }
  case OBJ_CLOSURE: {
    printFunction(PAYLOAD_CLOSURE(value)->function);
    break;
  }
  case OBJ_UPVALUE: {
    printf("upvalue");
    break;
  }
  case OBJ_ARRAY: {
    ObjArray *array = PAYLOAD_ARRAY(value);
    printf("[");
    for (int i = 0; i < array->elements.count; i++) {
      printValue(array->elements.values[i]);
      if (i < array->elements.count - 1) {
        printf(", ");
      }
    }
    printf("]");
    break;
  }
  }
}

StringObj *makeObjWithString(char *chars, int length) {
  uint32_t hash = hashFunc(chars, length);
  printf("lookup from makeojb with %s\n", chars);
  Entry *interned = lookUp(&vm.strings, chars, hash, length);
  if (interned != NULL) {
    free(chars);

#ifdef DEBUG_PRINT_CODE
    printf("interned found from makeObjWithString at %p\n", interned);
#endif
    return interned->key;
  }
  printf("nothing found at makeObjWithString\n");
  return allocateString(chars, length, hash);
}
