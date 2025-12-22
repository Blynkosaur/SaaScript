#ifndef bryte_chunk_h

#define bryte_chunk_h
#include "../common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_NULL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_EQUAL,
  OP_GREATER,
  OP_DEFINE_GLOBAL,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_LESS,
  OP_ADD,
  OP_SUBSTRACT,
  OP_MULITPLY,
  OP_DIVIDE,
  OP_GET_GLOBAL,
  OP_SET_GLOBAL,
  OP_NEGATE,
  OP_NOT,
  OP_PRINT,
  OP_JUMP_IF_FALSE,
  OP_JUMP,
  OP_LOOP,
  OP_CALL,
  OP_RETURN,
} OpCode;

typedef struct {
  int count;
  int capacity;
  int *lines;
  ValueArray constants;
  uint8_t *code;
  bool cooked;
} Chunk;

void initChunk(Chunk *chunk);

void writeChunk(Chunk *chunk, uint8_t byte, int line);

void freeChunk(Chunk *chunk);

int addConstant(Chunk *, Value value);
#endif
