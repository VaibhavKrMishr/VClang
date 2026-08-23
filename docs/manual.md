# 📖 VClang Complete Language Reference & Specification

---

## 📑 Table of Contents
1. [Overview & Design Philosophy](#1-overview--design-philosophy)
2. [Quick Start & Toolchain](#2-quick-start--toolchain)
3. [Lexical Conventions & Grammar](#3-lexical-conventions--grammar)
4. [Data Types & Type System](#4-data-types--type-system)
5. [Universal Memory Model & Allocators](#5-universal-memory-model--allocators)
6. [Expressions & Operators](#6-expressions--operators)
7. [Statements & Control Flow](#7-statements--control-flow)
8. [Functions & Lexical Scoping](#8-functions--lexical-scoping)
9. [Built-in Functions Reference](#9-built-in-functions-reference)
10. [Algorithm & Systems Cookbook](#10-algorithm--systems-cookbook)
11. [Runtime Architecture & Performance](#11-runtime-architecture--performance)

---

## 1. Overview & Design Philosophy

**VClang** is a high-performance, lightweight systems language designed with three pillars:

* **Simplicity:** Familiar C-family syntax without legacy baggage (no header files, no pointer decay bugs, clean arrays).
* **Modern Safety (RAII):** Automatic scoped resource cleanup. Dynamically allocated memory (arrays, strings, environments) is freed the instant a variable leaves its `{ ... }` scope without requiring a background Garbage Collector (GC).
* **Low-Level Control:** First-class Arena allocators allowing users to group memory allocations into custom memory pools with $O(1)$ batch resets.

---

## 2. Quick Start & Toolchain

### Building the Runtime
```bash
make clean && make
```

### Running Programs
* **Via Runner:** `./run script.vclang`
* **Direct Execution:** `./vclang script.vclang`
* **Interactive REPL:** `./vclang`
* **Test Suite:** `bash tests/run_tests.sh`

---

## 3. Lexical Conventions & Grammar

### Identifiers
Identifiers must start with an ASCII letter or underscore (`_`), followed by any number of alphanumeric characters or underscores.
```c
int valid_name_123 = 10;
```

### Comments
* **Single-Line Comments:**
  ```c
  // This is a single-line comment
  int x = 5; // Inline comment
  ```
* **Multi-Line Block Comments:**
  ```c
  """
  Multi-line docstring or block comment.
  Ignored completely by the lexer.
  """
  ```

### Semicolons & Blocks
Statements terminate with a semicolon (`;`). Code blocks are enclosed within curly braces `{ ... }` and establish a new lexical scope.

---

## 4. Data Types & Type System

VClang supports strong typing with explicit type specifiers or `var`/`let` type inference.

### Primitive Types
| Type | Description | Example |
| :--- | :--- | :--- |
| **`int`** | 64-bit signed integer | `int count = 42;` |
| **`float`** | Double-precision 64-bit IEEE floating-point | `float pi = 3.14159;` |
| **`string`** | UTF-8 heap-allocated text with escape sequences | `string msg = "Hello\nWorld";` |
| **`bool`** | Logical boolean represented as integer (1 or 0) | `bool active = true;` |
| **`byte`** | 8-bit unsigned scalar for raw buffers | `byte b = 255;` |

### Compound & Reference Types
| Type | Description | Example |
| :--- | :--- | :--- |
| **`T[]` / `array`** | Reference-counted dynamic array of type `T` | `int[] nums = [1, 2, 3];` |
| **`Arena`** | Reference to a custom Arena memory allocator | `Arena pool = new_arena(4096);` |

---

## 5. Universal Memory Model & Allocators

VClang provides a 3-tier memory allocation system:

```
┌────────────────────────────────────────────────────────────────────────┐
│                              VCLANG MEMORY                             │
├─────────────────────────┬────────────────────────┬─────────────────────┤
│ 1. Stack Allocation     │ 2. Scoped Heap (Default)│ 3. Custom Arenas    │
│ • Fixed sizes           │ • Dynamic arrays       │ • Bulk memory       │
│ • Blazing fast          │ • Strings & Structs    │ • Game / batch loops│
│ • Auto-freed by CPU     │ • Auto-freed via RAII  │ • Free all at once  │
└─────────────────────────┴────────────────────────┴─────────────────────┘
```

### 1. Stack Allocation (Fixed-Size)
Declared with bracketed size. Lives on the CPU call stack and requires zero heap allocation:
```c
int stack_buf[16];
stack_buf[0] = 100;
```

### 2. Scoped Heap Allocation (Default RAII)
Dynamic arrays allocate an `ArrayBuffer` on the heap with reference-counting. When all references leave scope, memory is freed immediately:
```c
{
    int[] nums = [10, 20, 30];
    push(nums, 40);
} // <── SCOPE ENDS: 'nums' memory is freed automatically!
```

### 3. Explicit `new` & Custom Arenas
Use `new` to explicitly allocate sized buffers or allocate inside a specific `Arena`:
```c
// Sized heap allocation (zero-initialized)
int[] buffer = new int[1024];

// Arena-backed allocation
Arena arena = new_arena(16384);
int[] fast_data = new(arena) [1, 2, 3, 4, 5];

// Instant O(1) recycling
reset_arena(arena);
```

---

## 6. Expressions & Operators

### Arithmetic Operators
* `+` : Addition / String Concatenation
* `-` : Subtraction
* `*` : Multiplication
* `/` : Division (integer or floating-point)
* `%` : Modulo (integers)

### Comparison & Relational Operators
* `==` : Equality
* `!=` : Inequality
* `<`  : Less than
* `>`  : Greater than
* `<=` : Less than or equal
* `>=` : Greater than or equal

### Logical Operators
* `&&` : Logical AND (Short-circuiting)
* `||` : Logical OR (Short-circuiting)
* `!`  : Logical NOT (Unary prefix)

---

## 7. Statements & Control Flow

> [!IMPORTANT]
> In VClang, **assignment is strictly a statement, not an expression**. Constructs like `if (x = 5)` are syntax errors, preventing accidental assignment bugs.

### Conditional Branching (`if` / `else`)
```c
if (score >= 90) {
    println("Grade: A");
} else if (score >= 75) {
    println("Grade: B");
} else {
    println("Grade: C");
}
```

### While Loops
```c
int counter = 5;
while (counter > 0) {
    println("Countdown:", counter);
    counter = counter - 1;
}
```

### For Loops (`for`, `break`, `continue`)
```c
for (int i = 0; i < 10; i = i + 1) {
    if (i == 3) { continue; } // Skip 3
    if (i == 8) { break; }    // Stop at 8
    println("Iter:", i);
}
```

---

## 8. Functions & Lexical Scoping

Functions are declared using `func`, `def`, or `fn`. They support parameter type annotations, recursion, and return values.

### Basic & Recursive Functions
```c
func add(int a, int b) {
    return a + b;
}

func fib(int n) {
    if (n <= 0) { return 0; }
    if (n == 1) { return 1; }
    return fib(n - 1) + fib(n - 2);
}
```

### Arrays as First-Class Parameters
Unlike C, arrays in VClang **do not decay to raw pointers**. They retain their bounds and are passed via reference-counted handles:
```c
func array_sum(int[] arr) {
    int total = 0;
    for (int i = 0; i < len(arr); i = i + 1) {
        total = total + arr[i];
    }
    return total;
}
```

---

## 9. Built-in Functions Reference

### Console I/O
* **`print(arg1, arg2, ...)`**: Prints arguments separated by spaces without a trailing newline.
* **`println(arg1, arg2, ...)`**: Prints arguments separated by spaces followed by a newline.
* **`input(prompt)`**: Prints prompt to stdout and reads a line from stdin as a `string`.

### Array & Collection Operations
* **`len(collection)`**: Returns the integer length of an array or string.
* **`push(array, element)`**: Appends an element to a dynamic array in-place.

### Memory & Arena Management
* **`new_arena(capacity)` / `Arena(capacity)`**: Creates a memory arena with the given initial byte capacity (default: 4096).
* **`reset_arena(arena)`**: Wipes all allocations in the arena for instant reuse ($O(1)$).
* **`arena_used(arena)`**: Returns the current byte usage of an arena.

### Type Conversions & Casting
* **`int(val)`**: Converts a float, numeric string, or single ASCII char to `int`.
* **`float(val)`**: Converts an integer or string to `float`.
* **`string(val)`**: Converts an integer or float to its formatted `string` representation.
* **`char(ascii_code)`**: Converts an integer ASCII value to a 1-character `string`.

---

## 10. Algorithm & Systems Cookbook

### Recursive In-Place QuickSort
```c
func partition(int[] arr, int low, int high) {
    int pivot = arr[high];
    int i = low - 1;
    for (int j = low; j < high; j = j + 1) {
        if (arr[j] <= pivot) {
            i = i + 1;
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }
    int swap_temp = arr[i + 1];
    arr[i + 1] = arr[high];
    arr[high] = swap_temp;
    return i + 1;
}

func quicksort(int[] arr, int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);
        quicksort(arr, low, pi - 1);
        quicksort(arr, pi + 1, high);
    }
    return 0;
}
```

### Sieve of Eratosthenes
```c
func find_primes(int limit) {
    int[] is_prime = new int[limit + 1];
    for (int i = 2; i <= limit; i = i + 1) {
        is_prime[i] = 1;
    }
    for (int p = 2; p * p <= limit; p = p + 1) {
        if (is_prime[p] == 1) {
            for (int m = p * p; m <= limit; m = m + p) {
                is_prime[m] = 0;
            }
        }
    }
    int[] primes = new int[0];
    for (int i = 2; i <= limit; i = i + 1) {
        if (is_prime[i] == 1) {
            push(primes, i);
        }
    }
    return primes;
}
```

---

## 11. Runtime Architecture & Performance

### Compiler & AST Design
1. **Tokenizer (`src/lexer.c`):** Single-pass zero-copy lexer with lookahead for two-char operators and string escape resolution.
2. **Parser (`src/parser.c`):** Recursive descent precedence-climbing parser constructing AST nodes inside a fast internal Arena allocator.
3. **Evaluator (`src/interpreter.c`):** Tree-walking interpreter using environment pooling (`Env` freelist) and reference-counted `ArrayBuffer` handles to guarantee zero runtime allocation bottlenecks.

### Benchmark Statistics
On standard x86_64 Linux systems:
* **Interpreter Cold Startup:** `< 0.05 ms`
* **Test Suite (10 suites, 100+ assertions):** `< 3.0 ms total`
* **Memory Footprint:** `< 2.0 MB RSS`
