#include "../../include/compiler/scanner.h"
#include "../../include/common.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  const char *start;
  const char *current;
  int line;

} Scanner;

Scanner scanner;

void initScanner(const char *source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

static bool isAtEnd() { return *(scanner.current) == '\0'; }
static Token makeToken(TokenType type) {
  Token token;
  token.type = type;
  token.length = (int)(scanner.current - scanner.start);
  token.start = scanner.start;
  token.line = scanner.line;
  return token;
}
static Token errorToken(const char *message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;
  return token;
}
static char advance() {
  char current_char = scanner.current[0];
  scanner.current++;
  return current_char;
}
static bool match_next(char expected) {
  if (isAtEnd())
    return false;
  if (*(scanner.current) == expected) {
    scanner.current++;
    return true;
  }
  return false;
}
static char peek() { return *(scanner.current); }
static char peekNext() {
  if (isAtEnd())
    return '\0'; // in case already at the end of file so can't peek forward
                 // anymore
  return *(scanner.current + 1);
}
static void skipWhitespace() {
  while (true) {
    char c = peek();
    switch (c) {
    case ' ':
    case '\r':
    case '\t':
      advance();
      break;
    case '\n':
      scanner.line++;
      advance();
      break;
    case '#': // for comments
      advance();
      while (peek() != '\n' && !isAtEnd()) {
        advance();
      }
      break;

    default:
      return;
    }
  }
}
static TokenType checkKeyword(int start, int length, const char *rest,
                              TokenType type) {
  if (scanner.current - scanner.start == start + length &&
      memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }
  return TOKEN_IDENTIFIER;
}
static TokenType saasType() {
  switch (scanner.start[0]) {
  case 'a':
    return checkKeyword(1, 6, "gentic", TOKEN_FOR);
  case 'b': {
    if ((scanner.current - scanner.start) > 1) {
      switch (scanner.start[1]) {
      case '2':
        return checkKeyword(2, 1, "b", TOKEN_WHILE);
      case 'l':
        return checkKeyword(2, 8, "ockchain", TOKEN_NULL);
      case 'o':
        return checkKeyword(2, 7, "otstrap", TOKEN_VAR);
      case 'u':
        return checkKeyword(2, 5, "rnout", TOKEN_FALSE);
      default:
        return TOKEN_IDENTIFIER;
      }
      return TOKEN_IDENTIFIER;
    }
  }

  case 'd':
    return checkKeyword(1, 6, "isrupt", TOKEN_IF);
  case 'l':
    return checkKeyword(1, 7, "everage", TOKEN_PRINT);
  case 'm':
    return checkKeyword(1, 2, "vp", TOKEN_FUN);
  case 'p':
    return checkKeyword(1, 4, "ivot", TOKEN_ELSE);
  case 's':

    if ((scanner.current - scanner.start) > 1) {
      switch (scanner.start[1]) {
      case 'a':
        return checkKeyword(2, 2, "as", TOKEN_RETURN);
      case 'c':
        return checkKeyword(2, 3, "ale", TOKEN_OR);
      case 'y':
        return checkKeyword(2, 5, "nergy", TOKEN_AND);
      deafult:
        return TOKEN_IDENTIFIER;
      }
    }
    return TOKEN_IDENTIFIER;

  case 'u':
    return checkKeyword(1, 6, "nicorn", TOKEN_TRUE);
  default:
    return TOKEN_IDENTIFIER;
  }
}
static TokenType identifierType() {
  switch (scanner.start[0]) {
  case 'a':
    return checkKeyword(1, 2, "nd", TOKEN_AND);
  case 'c':
    return checkKeyword(1, 4, "lass", TOKEN_CLASS);
  case 'e':
    return checkKeyword(1, 3, "lse", TOKEN_ELSE);
  case 'i':
    return checkKeyword(1, 1, "f", TOKEN_IF);
  case 'n':
    return checkKeyword(1, 3, "ull", TOKEN_NULL);
  case 'o':
    return checkKeyword(1, 1, "r", TOKEN_OR);
  case 'p':
    return checkKeyword(1, 4, "rint", TOKEN_PRINT);
  case 'r':
    return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
  case 's':
    return checkKeyword(1, 4, "uper", TOKEN_SUPER);
  case 'v':
    return checkKeyword(1, 2, "ar", TOKEN_VAR);
  case 'w':
    return checkKeyword(1, 4, "hile", TOKEN_WHILE);
  case 'f': {
    if ((scanner.current - scanner.start) > 1) {
      switch (scanner.start[1]) {
      case 'a':
        return checkKeyword(2, 3, "lse", TOKEN_FALSE);
      case 'o':
        return checkKeyword(2, 1, "r", TOKEN_FOR);
      case 'u':
        return checkKeyword(2, 1, "n", TOKEN_FUN);
      default:
        return TOKEN_IDENTIFIER;
      }
      return TOKEN_IDENTIFIER;
    }
    return TOKEN_IDENTIFIER;
  }
  case 't': {
    if ((scanner.current - scanner.start) > 1) {
      switch (scanner.start[1]) {
      case 'h':
        return checkKeyword(2, 2, "is", TOKEN_THIS);
      case 'r':
        return checkKeyword(2, 2, "ue", TOKEN_TRUE);
      default:
        return TOKEN_IDENTIFIER;
      }
    }
    return TOKEN_IDENTIFIER;
  }
  default:
    return TOKEN_IDENTIFIER;
  }
}
static Token string() {
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n')
      scanner.line++;
    advance();
  }
  if (isAtEnd())
    return errorToken("Unterminated string.");
  // for the closing quote.
  advance();
  return makeToken(TOKEN_STRING);
}
static bool isDigit(char c) { return c >= '0' && c <= '9'; }
static Token number() {
  while (isDigit(peek()))
    advance();

  // fractions fuck me why is that even a thing

  if (peek() == '.' && isDigit(peekNext())) {
    advance();

    while (isDigit(peek()))
      advance();
  }
  return makeToken(TOKEN_NUMBER);
}

static bool isAlpha(char c) {
  return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_');
}

static Token identifier() {
  while (isAlpha(peek()) || isDigit(peek()))
    advance();
#ifdef SAAS_MODE
  return makeToken(saasType());
#endif
#ifndef SAAS_MODE
  return makeToken(identifierType());
#endif
  // => what the fuck idk why not just return TOKEN_IDENTIFIER
  // maybe for reserved words or user made identifiers(i.e. variables)
}

Token scanToken() {
  skipWhitespace(); // before even getting to a token just skip all ts
  // cooked
  scanner.start = scanner.current;
  if (isAtEnd())
    return makeToken(TOKEN_EOF);

  char c = advance();
  if (isDigit(c)) {
    return number();
  }
  if (isAlpha(c)) {
    return identifier();
  }
  switch (c) {
  case '(':
    return makeToken(TOKEN_LEFT_PAREN);
  case ')':
    return makeToken(TOKEN_RIGHT_PAREN);
  case '{':
    return makeToken(TOKEN_LEFT_BRACE);
  case '}':
    return makeToken(TOKEN_RIGHT_BRACE);
  case '[':
    return makeToken(TOKEN_LEFT_BRACKET);
  case ']':
    return makeToken(TOKEN_RIGHT_BRACKET);
  case ';':
    return makeToken(TOKEN_SEMICOLON);
  case ',':
    return makeToken(TOKEN_COMMA);
  case '.':
    return makeToken(TOKEN_DOT);
  case '-':
    return makeToken(TOKEN_MINUS);
  case '+':
    return makeToken(TOKEN_PLUS);
  case '/':
    return makeToken(TOKEN_SLASH);
  case '*':
    return makeToken(TOKEN_STAR);
  case '!':
    return makeToken(match_next('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
  case '=':
    return makeToken(match_next('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
  case '<':
    return makeToken(match_next('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
  case '>':
    return makeToken(match_next('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);

  case '"':
    return string();
  }

  return errorToken("Unexpected character.");
}
