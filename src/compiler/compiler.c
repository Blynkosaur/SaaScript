#include "../../include/compiler/compiler.h"
#include "../../include/compiler/scanner.h"
#include "../../include/debug.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  Token current;
  Token previous;
  bool cooked;
  bool hadError;

} Parser;
Chunk *compilingChunk;

Parser parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR,         // or
  PREC_AND,        // and
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . ()
  PREC_PRIMARY
} Precedence;
typedef void (*Parsefunc)(bool canAssign);
typedef struct {
  Parsefunc prefix;
  Parsefunc infix;
  Precedence precedence;

} ParseRule;
typedef struct {
  Token name;
  int depth;
} Local;
typedef enum {
  TYPE_FUNCTION,
  TYPE_SCRIPT,
} FunctionType;
typedef struct {
  // compiler compiles to functions with chunks
  // instead of straight up chunks
  ObjFunction *function;
  FunctionType type;
  Local locals[UINT8_COUNT];
  int localCount;
  int scopeDepth;
  // local variables are stored in array "locals"
} Compiler;

Compiler *current = NULL;
static Chunk *currentChunk() { return &current->function->chunk; }

static void expression();

static void statement();
static void declaration();
static bool check(TokenType type);
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static u_int8_t makeConstant(Value value);
static int writeLoop(int loopStart);
static bool match(TokenType type);
static void defineVariable(uint8_t global);
static void consume(TokenType type, const char *message);
static void advance();
static uint8_t parseVariable(const char *errorMessage);
static void error(const char *message);
static void writeByte(uint8_t byte);
static void whileLoop();
static void forLoop();
static void patchJump(int offset);
static int writeJump(uint8_t instruction);
static void markInitialized();
static ObjFunction *endCompiler();
static void initCompiler(Compiler *compiler, FunctionType type);
static void endScope();
static void beginScope();
static bool identifierEquals(Token *a, Token *b);
static void writeBytes(uint8_t byte1, uint8_t byte2);
static void parsePrecedence(Precedence precedence) {
  advance();
  Parsefunc prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    error("Expression expected.");
    return;
  }
  bool canAssign = precedence <= PREC_ASSIGNMENT;
  printf("crashed here\n");
  prefixRule(canAssign);
  while (precedence <= getRule(parser.current.type)->precedence) {
    advance(); // --> parses the expression until finds something with lower
               // precedence
    Parsefunc infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }
  // if(canAssign && match(TOKEN_EQUAL)){
  //     error("invalid assignment target.");
  // }
}
// parser scans the tokens and transforms it into the tree/ syntax bullshit,
// just keep reading
//  do not read these comments please
static void errorAt(Token *token, const char *message) {
  if (parser.cooked)
    return;
  fprintf(stderr, "[line %d] Error ", token->line);
  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Why is this nothing?? idk
  } else {
    fprintf(stderr, "at '%.*s'", token->length, token->start);
  }
  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
  parser.cooked = true;
}
static void error(const char *message) { errorAt(&parser.previous, message); }
static void errorAtCurrent(const char *message) {
  errorAt(&parser.current, message);
}
static void advance() {
  parser.previous = parser.current;
  while (1) {
    parser.current = scanToken(); // moves the current token forward
    if (parser.current.type !=
        TOKEN_ERROR) // lwk retarded, only loops if there is an error

      break;

    errorAtCurrent(parser.current.start);
  }
}
static void literal(bool canAssign) {
  switch (parser.previous.type) {
  case TOKEN_FALSE: {
    writeByte(OP_FALSE);
    break;
  }
  case TOKEN_TRUE: {
    writeByte(OP_TRUE);
    break;
  }
  case TOKEN_NULL: {
    writeByte(OP_NULL);
    break;
  }
  default:
    return;
  } // no need to advance anywhere since the prefix is automatically
  // consumed in "parsePrecendence"
}
static void expression() { parsePrecedence(PREC_ASSIGNMENT); }
static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}
static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expected '(' after function name.");
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after function name.");
  consume(TOKEN_LEFT_BRACE, "Expected '{' after function name.");
  block();
  ObjFunction *function = endCompiler();
  writeBytes(OP_CONSTANT, makeConstant(OBJ_VAL(function)));
}
static void functionDeclaration() {
  uint8_t global = parseVariable("Expected function name.");
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global);
}
static uint8_t identifierConstant(Token *token) {
  return makeConstant(OBJ_VAL(copyString(
      token->start,
      token->length))); // stores the variable name to the chunk value array
}
static bool identifierEquals(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return strncmp(a->start, b->start, a->length) == 0;
}
static int resolveLocal(Compiler *compiler, Token *name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];
    if (identifierEquals(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer");
      }
      return i;
    }
  }
  return -1;
}
static void addLocal(Token name) {
  if (current->localCount == UINT8_COUNT) {
    error("Too many local variables in function.");
    return;
  }
  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
}

static void declareVariable() {
  if (current->scopeDepth == 0)
    return;
  Token *name = &parser.previous;
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }
    if (identifierEquals(name, &local->name)) {
      error("variable already declared/existing in this scope");
    }
  }
  addLocal(*name);
}
static void defineVariable(uint8_t global) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }
  writeBytes(OP_DEFINE_GLOBAL, global); // OP_DEFINE_GLOBAL
}
static void and_(bool canAssign) {
  int endJump = writeJump(OP_JUMP_IF_FALSE);
  writeByte(OP_POP);
  parsePrecedence(PREC_AND);
  patchJump(endJump);
}
static uint8_t
parseVariable(const char *errorMessage) { // stores the variable name
  consume(TOKEN_IDENTIFIER, errorMessage);
  declareVariable();
  if (current->scopeDepth > 0)
    return 0;
  return identifierConstant(
      &parser.previous); // returns the index of the of the variable name on
                         // the value array
}
static void markInitialized() {
  if (current->scopeDepth == 0) // if a function is called at the top level
    return;
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}
static void varDeclaration() {
  uint8_t global =
      parseVariable("Expected variable name"); // index of name on value array
  if (match(TOKEN_EQUAL)) {
    expression(); // stores the variable value
  } else {
    writeByte(OP_NULL);
  } // allows simple variable declaration or declaration with assignment
  consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration");

  defineVariable(global);
}
static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  writeByte(OP_POP);
}
static void ifStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition");
  int thenJump = writeJump(OP_JUMP_IF_FALSE);
  writeByte(OP_POP);                 // pop bool of the condition if true
  statement();                       // then clause
  int elseJump = writeJump(OP_JUMP); // no need to worry bout conditional
                                     // since included in the if statement
  patchJump(thenJump);
  if (match(TOKEN_ELSE))
    statement();
  patchJump(elseJump);
  writeByte(OP_POP); // pop bool if false
}
static void whileLoop() {
  int loop_start = currentChunk()->count;
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression");

  int loop_exit = writeJump(OP_JUMP_IF_FALSE);
  writeByte(OP_POP); // pops the condition if true
  statement();
  writeLoop(loop_start); // go back to the beginning of the loop
  patchJump(loop_exit);
  writeByte(OP_POP);
}
static void forLoop() {
  beginScope();
  consume(TOKEN_LEFT_PAREN, "'(' expected after for keyword");
  if (match(TOKEN_SEMICOLON)) {
    // no initializer
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    expressionStatement();
  }

  int loopStart = currentChunk()->count;
  // condition clause
  int exitJump = -1;
  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after loop condition");

    // exit loop if false
    exitJump = writeJump(OP_JUMP_IF_FALSE);
    writeByte(OP_POP);
  }
  if (!match(TOKEN_RIGHT_PAREN)) {
    int bodyJump = writeJump(OP_JUMP);
    int increment = currentChunk()->count;
    expression();
    writeByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after for clauses");

    writeLoop(loopStart);
    loopStart = increment;
    patchJump(bodyJump);
  }
  statement();
  writeLoop(loopStart);
  if (exitJump != -1) {
    patchJump(exitJump);
    writeByte(OP_POP);
  }
  endScope();
}
static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  writeByte(OP_PRINT);
}
static void synchronize() {
  parser.cooked = false;
  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON)
      return;
    switch (parser.current.type) {
      // basically safe to start scanning at the beginning of a statement
    case TOKEN_PRINT:
    case TOKEN_CLASS:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_RETURN:
    case TOKEN_FUN:
      return;

    default:;
    }
  }
}
static void declaration() {
  if (match(TOKEN_FUN)) {
    functionDeclaration();
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {

    statement();
  }
  // if (parser.cooked)
  //     synchronize();
}
static void statement() {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_WHILE)) {
    whileLoop();
  } else if (match(TOKEN_FOR)) {
    forLoop();
  } else {
    expressionStatement();
  }
}
static void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }
  errorAtCurrent(message);
}
static bool match(TokenType type) {
  if (!check(type))
    return false;
  advance();
  return true;
}
static bool check(TokenType type) { return parser.current.type == type; }

static void writeByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}
static void writeReturn() { writeByte(OP_RETURN); }
static u_int8_t makeConstant(Value value) {
  int constant_index = addConstant(currentChunk(), value);
  // gives back the index on the constant array for the chunk
  if (constant_index > UINT8_MAX) {
    // since all bytecode are all 8 bits, max is 256 soo..... yeah the max
    // array size is also 256 bytes all bytecode is 8/bits one byte --> so the
    // maximum index for the value array is also 256-1
    error("Too many constants in one chunk.");
    return 0;
  }
  return (uint8_t)constant_index;
}

static void writeBytes(uint8_t byte1, uint8_t byte2) {
  writeByte(byte1);
  writeByte(byte2);
}
static int writeLoop(int loopStart) {
  writeByte(OP_LOOP);
  int jump = currentChunk()->count - loopStart + 2;
  if (jump > UINT16_MAX)
    error("Too big a loop");

  writeByte((jump >> 8) & 0xff);
  writeByte(jump & 0xff);
}
static int writeJump(uint8_t instruction) {
  writeByte(instruction);
  writeByte(0xff);
  writeByte(0xff);
  return currentChunk()->count - 2; // -2 cuz it's: "instruction" "byte" "byte",
                                    // so 2 bytes are taken as operands
  // returns the postion of the instruction within the chunk
}

static void writeConstant(Value value) {
  writeBytes(OP_CONSTANT, makeConstant(value));
}
static void initCompiler(Compiler *compiler, FunctionType type) {
  compiler->function = NULL;
  compiler->type = type;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->function = newFunction();
  current = compiler;
  Local *local = &current->locals[current->localCount++];
  local->depth = 0;
  local->name.start = "";
  local->name.length = 0;
}
static ObjFunction *endCompiler() {
  writeReturn();
  ObjFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
  disassembleChunk(currentChunk(),
                   function->name != NULL ? function->name->chars : "<script>");
#endif
  return function;
}
static void beginScope() { current->scopeDepth++; }
static void endScope() {
  current->scopeDepth--;
  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > current->scopeDepth) {
    writeByte(OP_POP);
    current->localCount--;
  }
}
static void patchJump(int offset) {
  int jump = currentChunk()->count - offset - 2;
  //-2 cuz jump operation has 2 operands that have already been consumed by
  // runtime
  if (jump > UINT16_MAX) {
    error("Too much code to jump over.");
  }
  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  // should have paid more attention during bitwise operations
  currentChunk()->code[offset + 1] = jump & 0xff;
  // here too
}
static void grouping(bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}
static void unary(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  // expression();
  parsePrecedence(PREC_UNARY); // maybe take this off idk what this is
  switch (operatorType) {
  case TOKEN_MINUS:
    writeByte(OP_NEGATE);
    break;
  case TOKEN_BANG:
    writeByte(OP_NOT);
    break;
  default:
    return;
  }
}

static void binary(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  ParseRule *rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1));
  switch (operatorType) {
  case TOKEN_PLUS:
    writeByte(OP_ADD);
    break;
  case TOKEN_MINUS:
    writeByte(OP_SUBSTRACT);
    break;
  case TOKEN_STAR:
    writeByte(OP_MULITPLY);
    break;
  case TOKEN_SLASH:
    writeByte(OP_DIVIDE);
    break;
  case TOKEN_BANG_EQUAL:
    writeBytes(OP_EQUAL, OP_NOT);
    break;
  case TOKEN_EQUAL_EQUAL:
    writeByte(OP_EQUAL);
    break;
  case TOKEN_GREATER:
    writeByte(OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    writeBytes(OP_LESS, OP_NOT);
    break;
  case TOKEN_LESS:
    writeByte(OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    writeBytes(OP_GREATER, OP_NOT);
    break;

  /* Just a small comment here, the reason why there are only 3 instructions:
  GREATER LESS AND EQUAL is cuz we can make all the other logic operators with
  them
  >= is the same as. !(<), >= the same as !(<) and != same as !(=)
  WHAAAAAT BLOWS MY MIND yeah this is just De Morganz law crazy*/
  default:
    return;
  }
}
static void number(bool canAssign) {
  double value = strtod(parser.previous.start, NULL);
  writeConstant(NUMBER_VAL(value));
}
static void or_(bool canAssign) {
  int if_false = writeJump(OP_JUMP_IF_FALSE);
  // if false, then parses the right hand side with parsePrecedence(or)
  int jump_if_true = writeJump(OP_JUMP);
  // if left hand side is true, then jump over the right and side
  patchJump(if_false);
  writeByte(OP_POP);
  parsePrecedence(PREC_OR);
  patchJump(jump_if_true);
}

static void string(bool canAssign) {
  writeConstant(OBJ_VAL(
      copyString(parser.previous.start + 1, parser.previous.length - 2)));
  // parser.previous is a token
  /*" a b c "
      |   |
  yeah idk "a" is start +1 and "c" is lenght -2 since not 0 indexed*/
}
static void namedVariable(Token name, bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else {
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }
  if (match(TOKEN_EQUAL) && canAssign)

  {
    expression();
    writeBytes(setOp, (uint8_t)arg);
  } else {

    writeBytes(getOp, (uint8_t)arg);
  }
}
static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}
ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NULL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};
static ParseRule *getRule(TokenType type) { return &rules[type]; }

ObjFunction *compile(const char *source) {
  initScanner(source);
  // compilingChunk = chunk;
  parser.hadError = false;
  parser.hadError = false;
  advance();
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);
  while (!match(TOKEN_EOF)) {
    if (parser.cooked) {
      parser.cooked = false; // reset to ok
      return false;
    }
    declaration();
  }
  ObjFunction *function = endCompiler();
  return parser.hadError ? NULL : function;
}
