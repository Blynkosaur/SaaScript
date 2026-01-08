# SaaScript

# 🧠 Tech Buzzword Interpreter

An interpreted programming language that uses tech industry buzzwords as its keywords. Built with a bytecode-based virtual machine, this language compiles source code to bytecode and executes it efficiently. Perfect for when you need to ship fast without getting bogged down in design patterns.

## 📦 Installation

To build SaaScript from source:

```bash
make all
```

This will create the executable at `build/saas`.

### Adding to PATH (Optional)

To use SaaScript without running the executable directly, you can add it to your `$PATH`:

**For bash/zsh:**

```bash
export PATH="$PATH:/path/to/SaaScript/build"
```

**To make it permanent**, add the above line to your `~/.bashrc` or `~/.zshrc` file.

After adding to PATH, you can run SaaScript from anywhere using:

```bash
saas
```

## 🚀 How It Works

SaaScript is an **interpreted language** that follows a traditional compiler pipeline:

1. **Scanner** - Tokenizes source code, recognizing tech buzzwords as keywords
2. **Compiler** - Parses tokens and generates bytecode instructions
3. **Virtual Machine** - Executes bytecode instructions on a stack-based VM

The language uses a **buzzword-to-keyword mapping** system where common programming constructs are represented by tech industry terminology. The complete mapping can be found in `src/compiler/buzzwords.txt`.

## 📝 Keyword Mapping

Instead of traditional keywords like `if`, `while`, or `var`, SaaScript uses tech buzzwords:

| Buzzword     | Maps To        | Traditional Keyword | Description            |
| ------------ | -------------- | ------------------- | ---------------------- |
| `disrupt`    | `TOKEN_IF`     | `if`                | Conditional statements |
| `b2b`        | `TOKEN_WHILE`  | `while`             | Loops                  |
| `bootstrap`  | `TOKEN_VAR`    | `var`               | Variable declarations  |
| `mvp`        | `TOKEN_FUN`    | `fun`               | Function definitions   |
| `leverage`   | `TOKEN_PRINT`  | `print`             | Output/printing        |
| `saas`       | `TOKEN_RETURN` | `return`            | Return statements      |
| `pivot`      | `TOKEN_ELSE`   | `else`              | Else clauses           |
| `agentic`    | `TOKEN_FOR`    | `for`               | For loops              |
| `synergy`    | `TOKEN_AND`    | `and`               | Logical AND            |
| `scale`      | `TOKEN_OR`     | `or`                | Logical OR             |
| `unicorn`    | `TOKEN_TRUE`   | `true`              | Boolean true           |
| `burnout`    | `TOKEN_FALSE`  | `false`             | Boolean false          |
| `blockchain` | `TOKEN_NULL`   | `null`              | Null value             |
| `arr`        | `TOKEN_LENGTH` | `.length`           | Array length property  |
| `fund`       | `TOKEN_PUSH`   | `.push()`           | Add element to array   |
| `churn`      | `TOKEN_POP`    | `.pop()`            | Remove element from array |

See `src/compiler/buzzwords.txt` for the complete mapping with explanations.

## 🚀 Features

- 🧮 Arithmetic operations (`+`, `-`, `*`, `/`)
- 📦 Variable declarations and assignments (`bootstrap`)
- 🔁 Conditionals and control flow (`disrupt`, `b2b`, `pivot`)
- 🧰 Function definitions and calls (`mvp`)
- 📊 Arrays with indexing, length, push, and pop operations
- 💬 Comments using `#` for single-line comments
- 🐞 Basic error handling and debugging output
- 📄 Optional REPL or script execution mode
- 🚫 **NO OOP** - When you ship, there's no time for design patterns or classes

## 💻 Usage

### REPL Mode

Run SaaScript without any arguments to start an interactive REPL (Read-Eval-Print Loop):

```bash
./build/saas
# or if added to PATH:
saas
```

You'll see a `>>>` prompt where you can type SaaScript code interactively. Type `quit` or `exit` to exit the REPL.

### Running Scripts

To run a SaaScript file (`.saas` extension), pass the file path as an argument:

```bash
./build/saas script.saas
# or if added to PATH:
saas script.saas
```

Example `.saas` file:

```saas
# This is a comment - use # for single-line comments
bootstrap x = 10;
leverage x;
```

## 📊 Arrays

SaaScript supports arrays with the following operations:

### Creating Arrays

Arrays are created using square brackets with comma-separated values:

```saas
bootstrap arr = [1, 2, 3];
bootstrap names = ["Alice", "Bob", "Charlie"];
```

### Accessing Elements

Access array elements using zero-based indexing:

```saas
bootstrap arr = [10, 20, 30];
leverage arr[0];  # Prints 10
leverage arr[1];  # Prints 20
arr[0] = 100;     # Modify element at index 0
```

### Array Length

Get the length of an array using the `.arr` property:

```saas
bootstrap arr = [1, 2, 3, 4, 5];
leverage arr.arr;  # Prints 5
```

### Adding Elements

Add elements to the end of an array using `.fund()` (push):

```saas
bootstrap arr = [1, 2, 3];
arr.fund(4);      # Add 4 to the end
leverage arr;      # Prints [1, 2, 3, 4]
```

### Removing Elements

Remove and return the last element using `.churn()` (pop):

```saas
bootstrap arr = [1, 2, 3, 4];
bootstrap last = arr.churn();  # Remove and get last element
leverage last;                  # Prints 4
leverage arr;                   # Prints [1, 2, 3]
```

### Complete Array Example

```saas
# Create an array
bootstrap numbers = [5, 10, 15];

# Access and modify elements
numbers[0] = 20;
leverage numbers[0];  # Prints 20

# Get length
leverage numbers.arr;  # Prints 3

# Add element
numbers.fund(25);
leverage numbers;      # Prints [5, 10, 15, 25]

# Remove element
bootstrap popped = numbers.churn();
leverage popped;       # Prints 25
leverage numbers;      # Prints [5, 10, 15]
```

## 💬 Comments

SaaScript supports single-line comments using the `#` character. Everything after `#` on a line is treated as a comment and ignored by the compiler:

```saas
# This is a comment
bootstrap x = 10;  # This is also a comment
leverage x;  # Comments can appear after code
```

## 💡 Philosophy

**No Classes, No OOP** - This language intentionally omits object-oriented programming features. Why? Because when you're shipping fast, there's no time for design patterns, inheritance hierarchies, or abstract interfaces. Keep it simple, keep it functional, keep it shipping.
