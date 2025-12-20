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
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
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

  printf("lookup from copyString with %s\n", chars);
  Entry *interned = lookUp(&vm.strings, chars, hash, length);
  if (interned != NULL) {
    printf("interned found from copystring at %p\n", interned);
    return interned->key;
  }
  printf("nothing found at copyString\n");
  char *heapChars = malloc(sizeof(char) * length + 1);
  if (heapChars == NULL) {
    printf("NULL pointer, nothing allocated \n");
  }
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';

  return allocateString(heapChars, length, hash);
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
  }
}

StringObj *makeObjWithString(char *chars, int length) {
  uint32_t hash = hashFunc(chars, length);
  printf("lookup from makeojb with %s\n", chars);
  Entry *interned = lookUp(&vm.strings, chars, hash, length);
  if (interned != NULL) {
    free(chars);

    printf("interned found from makeObjWithString at %p\n", interned);
    return interned->key;
  }
  printf("nothing found at makeObjWithString\n");
  return allocateString(chars, length, hash);
}
