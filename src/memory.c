#include "../include/memory.h"
#include "../include/bytecode/object.h"
#include "../include/vm/vm.h"
#include <stdio.h>

static void freeObject(Obj *object) {
  // reminder Obj --> {.type, .next}
  // StringObj --> {.obj, .chars, .length,}
  switch (object->type) {
  case OBJ_STRING: {
    StringObj *string = (StringObj *)object;
#ifdef DEBUG_PRINT_CODE
    printf("Freed string: %s\n", string->chars);
#endif
    free(string->chars);
    free(object);
    break;
  }
  case OBJ_CLOSURE: {
    ObjClosure *closure = (ObjClosure *)object;
    for (int i = 0; i < closure->upvalueCount; i++) {
      free(closure->upvalues[i]);
    }
    free(closure);
    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    freeChunk(&function->chunk);
    StringObj *string = function->name;
#ifdef DEBUG_PRINT_CODE
    printf("Freed string in function: %s\n", string->chars);
#endif
    free(string->chars);
    free(object);
    break;
  }
  case OBJ_NATIVE: {
    free(object);
    break;
  }
  case OBJ_UPVALUE:
    free(object);
    break;
  case OBJ_ARRAY: {
    ObjArray *array = (ObjArray *)object;
    freeValueArray(&array->elements);
    free(object);
    break;
  }
  }
}
void freeObjects() {
  Obj *head = vm.objectsHead;
  while (head != NULL) {
    Obj *temp_next = head->next;
    freeObject(head);
    head = temp_next;
  }
}
