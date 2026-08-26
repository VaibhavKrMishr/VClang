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
9. [Structs & Object Model](#9-structs--object-model)
10. [Built-in Functions Reference](#10-built-in-functions-reference)
11. [Algorithm & Systems Cookbook](#11-algorithm--systems-cookbook)
12. [Runtime Architecture & Performance](#12-runtime-architecture--performance)

---

## 1. Overview & Design Philosophy

**VClang** is a high-performance, lightweight systems language designed with three pillars:

* **Simplicity & Predictability:** "No headache" design philosophy — every construct is explicit, with no hidden control flow, no hidden dynamic dispatch, and no hidden heap allocations.
* **Modern Safety (RAII):** Automatic scoped resource cleanup. Dynamically allocated memory (arrays, strings, environments, structs) is freed the instant a variable leaves its `{ ... }` scope without requiring a background Garbage Collector (GC).
* **Low-Level Control:** First-class Arena allocators allowing users to group memory allocations into custom memory pools with $O(1)$ batch resets.
* **Flat Object Model:** Struct-based only with true pass-by-value semantics, Zig-style inline methods, composition-only code reuse, and zero `is-a` substitutability.

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
| **`struct Name`** | User-defined flat data record with inline methods | `struct Vector2 { float x; float y; }` |
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
Dynamic arrays and heap instances allocate on the heap with reference-counting. When all references leave scope, memory is freed immediately:
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

### Arithmetic & Assignment Operators
* `+`, `-`, `*`, `/`, `%` : Standard binary arithmetic
* `+=`, `-=`, `*=`, `/=` : Compound in-place assignment operators
* `++`, `--` : Unary increment / decrement
* `-` (unary) : Unary numeric negation
* `!` (unary) : Logical NOT

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

## 9. Structs & Object Model

VClang implements a modern, explicit, and minimal object model designed specifically for predictable performance and zero mental overhead.

### Design Invariants
1. **Struct-Based Only:** No classes, no inheritance, no virtual methods, no `static` keywords.
2. **Zig-style Inline Methods:** Methods are declared directly inside the struct body and desugar to static calls with an explicit `self` receiver. Zero vtable indirection.
3. **Static / Factory Associated Functions:** Functions inside a struct without a `self` parameter act as static constructors (e.g. `Vector2.zero()`).
4. **Composition Only:** Code reuse is achieved strictly by embedding named struct fields. There is **no implicit field promotion** (unlike Go) — all accesses must use explicit paths (`car.engine.hp`, never `car.hp`).
5. **Zero `is-a` Substitutability:** Parameter type checking is strict. A struct holding an `Engine` cannot be passed where an `Engine` is expected; only the inner field `car.engine` can be passed.
6. **True Pass-by-Value Semantics:** Struct assignment (`b = a;`) and argument passing create completely independent copies. Mutating `b` never mutates `a`.
7. **In-Place L-Value Mutations:** Chained field assignments (`car.engine.hp = 620;`, `car.pos.x += 10.0;`, `car.engine.cylinders++;`) mutate memory directly in-place without copy-on-read temporaries.

### 1. Struct Declaration & Inline Methods
```c
struct Vector2 {
    float x;
    float y;

    // Static Factory Constructor
    func zero() {
        return Vector2 { x: 0.0, y: 0.0 };
    }

    func new_vec(float x, float y) {
        return Vector2 { x: x, y: y };
    }

    // Instance Method (Explicit 'self' receiver)
    func length_sq(Vector2 self) {
        return self.x * self.x + self.y * self.y;
    }

    func add(Vector2 self, Vector2 other) {
        return Vector2 { x: self.x + other.x, y: self.y + other.y };
    }

    func scale(Vector2 self, float factor) {
        return Vector2 { x: self.x * factor, y: self.y * factor };
    }
}
```

### 2. Instantiation Models
```c
// A. Typed Zero-Initialization
Vector2 v_zero; // Initialized to { x: 0, y: 0 }

// B. Explicit Literal Initializer
Vector2 v1 = Vector2 { x: 3.0, y: 4.0 };

// C. Static Factory Function
Vector2 v2 = Vector2.zero();

// D. Custom Arena Allocation
Arena game_arena = new_arena(4096);
Vector2 v_arena = new(game_arena) Vector2 { x: 10.0, y: 20.0 };
```

### 3. Calling Methods
```c
Vector2 v = Vector2 { x: 3.0, y: 4.0 };

// Method call syntax desugars to Vector2.length_sq(v)
float len_sq = v.length_sq(); // 25.0

Vector2 scaled = v.scale(2.0); // { x: 6, y: 8 }
```

### 4. Composition & Explicit Field Paths
```c
struct Engine {
    int hp;
    int cylinders;
}

struct Car {
    string model;
    Engine engine;
    Vector2 pos;
}

Car my_car = Car {
    model: "V-GT Turbo",
    engine: Engine { hp: 550, cylinders: 8 },
    pos: Vector2 { x: 10.0, y: 20.0 }
};

// Access must use explicit path:
println("HP:", my_car.engine.hp); // 550
println("X:",  my_car.pos.x);     // 10.0

// In-place deep mutation:
my_car.engine.hp = 620;
my_car.pos.x += 50.0;
my_car.engine.cylinders++;
```

### 5. Arrays of Structs & Structs Containing Arrays
```c
// Array of structs
array fleet = [
    Car { model: "Alpha", engine: Engine { hp: 300, cylinders: 4 }, pos: Vector2 { x: 0, y: 0 } },
    Car { model: "Beta",  engine: Engine { hp: 450, cylinders: 6 }, pos: Vector2 { x: 10, y: 20 } }
];

fleet[0].engine.hp += 50;
println("Fleet 0 HP:", fleet[0].engine.hp); // 350

// Struct holding an array
struct Polygon {
    string name;
    array vertices;

    func vertex_count(Polygon self) {
        return len(self.vertices);
    }
}
```

---

## 10. Built-in Functions Reference

### Console I/O
* **`print(arg1, arg2, ...)`**: Prints arguments separated by spaces without a trailing newline.
* **`println(arg1, arg2, ...)`**: Prints arguments separated by spaces followed by a newline.
* **`input(prompt)`**: Prints prompt to stdout and reads a line from stdin as a `string`.

### Array & Collection Operations
* **`len(collection)`**: Returns the integer length of an array, string, or collection.
* **`push(array, element)`**: Appends an element to a dynamic array in-place.

### Memory & Arena Management
* **`new_arena(capacity)` / `Arena(capacity)`**: Creates a memory arena with the given initial byte capacity (default: 4096).
* **`reset_arena(arena)`**: Wipes all allocations in the arena for instant reuse ($O(1)$).
* **`arena_used(arena)`**: Returns the current byte usage of an arena in bytes.

### Type Reflection & Conversions
* **`typeof(val)`**: Returns the type name as a string (e.g., `"int"`, `"float"`, `"string"`, `"array"`, `"Arena"`, or the user struct name `"Vector2"`, `"type"`).
* **`int(val)`**: Converts a float, numeric string, or single ASCII char to `int`.
* **`float(val)`**: Converts an integer or string to `float`.
* **`string(val)`**: Converts an integer, float, or value to its formatted `string` representation.
* **`char(ascii_code)`**: Converts an integer ASCII value to a 1-character `string`.

---

## 11. Algorithm & Systems Cookbook

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

## 12. Runtime Architecture & Performance

### Compiler & AST Design
1. **Tokenizer (`src/lexer.c`):** Single-pass zero-copy lexer with lookahead for two-char operators (`==`, `!=`, `<=`, `>=`, `&&`, `||`, `+=`, `-=`, `++`, `--`, `*=`, `/=`) and string escape resolution.
2. **Parser (`src/parser.c`):** Recursive descent precedence-climbing parser constructing AST nodes inside a fast internal Arena allocator.
3. **Evaluator (`src/interpreter.c`):** Tree-walking interpreter using environment pooling (`Env` freelist), flat 16-byte `Value` representation respecting System V ABI register returns, and reference-counted handles.

### Benchmark Statistics
On standard x86_64 Linux systems:
* **Interpreter Cold Startup:** `< 0.05 ms`
* **Test Suite (14 suites, 100+ assertions):** `< 3.0 ms total`
* **Memory Footprint:** `< 2.0 MB RSS`
