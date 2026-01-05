# SaaScript
# 🧠 Tech Buzzword Interpreter

An interpreted programming language that uses tech industry buzzwords as its keywords. Built with a bytecode-based virtual machine, this language compiles source code to bytecode and executes it efficiently. Perfect for when you need to ship fast without getting bogged down in design patterns.

## 🚀 How It Works

SaaScript is an **interpreted language** that follows a traditional compiler pipeline:

1. **Scanner** - Tokenizes source code, recognizing tech buzzwords as keywords
2. **Compiler** - Parses tokens and generates bytecode instructions
3. **Virtual Machine** - Executes bytecode instructions on a stack-based VM

The language uses a **buzzword-to-keyword mapping** system where common programming constructs are represented by tech industry terminology. The complete mapping can be found in `src/compiler/buzzwords.txt`.

## 📝 Keyword Mapping

Instead of traditional keywords like `if`, `while`, or `var`, SaaScript uses tech buzzwords:

| Buzzword | Maps To | Traditional Keyword | Description |
|----------|---------|---------------------|-------------|
| `disrupt` | `TOKEN_IF` | `if` | Conditional statements |
| `b2b` | `TOKEN_WHILE` | `while` | Loops |
| `bootstrap` | `TOKEN_VAR` | `var` | Variable declarations |
| `mvp` | `TOKEN_FUN` | `fun` | Function definitions |
| `leverage` | `TOKEN_PRINT` | `print` | Output/printing |
| `saas` | `TOKEN_RETURN` | `return` | Return statements |
| `pivot` | `TOKEN_ELSE` | `else` | Else clauses |
| `agentic` | `TOKEN_FOR` | `for` | For loops |
| `synergy` | `TOKEN_AND` | `and` | Logical AND |
| `scale` | `TOKEN_OR` | `or` | Logical OR |
| `unicorn` | `TOKEN_TRUE` | `true` | Boolean true |
| `burnout` | `TOKEN_FALSE` | `false` | Boolean false |
| `blockchain` | `TOKEN_NULL` | `null` | Null value |

See `src/compiler/buzzwords.txt` for the complete mapping with explanations.

## 🚀 Features

- 🧮 Arithmetic operations (`+`, `-`, `*`, `/`)
- 📦 Variable declarations and assignments (`bootstrap`)
- 🔁 Conditionals and control flow (`disrupt`, `b2b`, `pivot`)
- 🧰 Function definitions and calls (`mvp`)
- 🐞 Basic error handling and debugging output
- 📄 Optional REPL or script execution mode
- 🚫 **NO OOP** - When you ship, there's no time for design patterns or classes

## 💡 Philosophy

**No Classes, No OOP** - This language intentionally omits object-oriented programming features. Why? Because when you're shipping fast, there's no time for design patterns, inheritance hierarchies, or abstract interfaces. Keep it simple, keep it functional, keep it shipping.

**No Arrays** - Arrays require you to know how many things you'll have *before* you have them. That's planning. That's architecture. That's thinking ahead. When you're shipping, you don't know how many users you'll have, how many features you'll add, or how many bugs you'll ship. Why should your data structures be any different? Arrays are for people who plan. We're for people who ship.
