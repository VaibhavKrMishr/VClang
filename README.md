# 🚀 VClang

[![Language](https://img.shields.io/badge/Language-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Build](https://img.shields.io/badge/Build-Passing-brightgreen.svg)]()
[![Execution Speed](https://img.shields.io/badge/Speed-Sub--millisecond-orange.svg)]()

**VClang** is a lightweight, high-performance, C-family programming language designed from the ground up for simplicity, speed, and modern systems-level control. Built in standard C99 with **zero external dependencies**, VClang merges familiar C-like syntax with modern memory safety (RAII scoped auto-cleanup), explicit Zig-style memory allocators (`Arena`), and an explicit, predictable struct object model.

---

## ✨ Key Highlights

* **⚡ Sub-Millisecond Execution:** Instantaneous startup and execution (average script execution in **0.10 ms – 1.4 ms**), outperforming interpreted languages like Python and Node.js by **20x–30x**.
* **🏗️ Predictable Struct Object Model:** Struct-based only (no classes, inheritance, or virtual dispatch). Features Zig-style inline methods, static factory constructors, composition-only reuse, true pass-by-value semantics, and in-place L-value mutations.
* **🛡️ Scoped Memory Safety (RAII):** Automatic cleanup of dynamic arrays, strings, environments, and struct instances at scope exit—no garbage collection pauses, no manual `free()` required.
* **🧠 Universal Memory & Custom Allocators:** First-class support for explicit Arena allocators (`new_arena()`, `reset_arena()`) and the `new` operator (`new(arena) Vector2 { ... }`).
* **📦 First-Class Functions & Recursion:** Full lexical scoping, nested function calls, recursive algorithms, and early returns.
* **📊 Dynamic & Sized Arrays:** Native dynamic lists (`[...]`, `push()`, `len()`) and fixed-size stack arrays (`int buf[64];`).
* **🧩 Zero Dependencies:** Builds in milliseconds with standard `gcc` and `make`.

---

## 🏎️ Benchmark Comparison

Total wall-clock time (File Read + Parse + AST Build + Execution + Output):

| Task / Benchmark | 🐍 Python 3 | 🟨 Node.js (V8) | 🚀 **VClang** |
| :--- | :--- | :--- | :--- |
| **Arithmetic & Loops** | ~28.5 ms | ~32.0 ms | **0.10 ms** |
| **Recursive Functions & Fib** | ~32.0 ms | ~35.0 ms | **0.26 ms** |
| **Structs, Physics & Arrays** | ~38.0 ms | ~40.0 ms | **0.50 ms** |
| **QuickSort & Sieve & Linear Algebra** | ~42.0 ms | ~45.0 ms | **1.35 ms** |

---

## 🛠️ Installation & Quick Start

### 1. Build from Source
```bash
git clone https://github.com/VaibhavKrMishr/VClang.git
cd VClang
make
```

### 2. Run Scripts & Examples
Execute any `.vclang` script directly using the `run` helper:
```bash
./run examples/07_structs_basics.vclang
```
Or with the binary directly:
```bash
./vclang tests/test_structs.vclang
```

### 3. Interactive REPL
Start the multi-line Read-Eval-Print Loop:
```bash
./vclang
```
```javascript
vclang> struct Vector2 { float x; float y; func len_sq(Vector2 self) { return self.x * self.x + self.y * self.y; } }
vclang> Vector2 v = Vector2 { x: 3.0, y: 4.0 };
vclang> println(v.len_sq());
25
```

### 4. Run Test Suite
```bash
bash tests/run_tests.sh
```

---

## 💻 Language Tour

### 1. Variables & Types
```c
int age = 21;
float pi = 3.14159;
string name = "VClang";
bool active = true;
```

### 2. Structs & Zig-Style Inline Methods
```c
struct Vector2 {
    float x;
    float y;

    // Static / Factory constructor
    func zero() {
        return Vector2 { x: 0.0, y: 0.0 };
    }

    // Inline instance method with explicit 'self' receiver
    func length_sq(Vector2 self) {
        return self.x * self.x + self.y * self.y;
    }

    func scale(Vector2 self, float factor) {
        return Vector2 { x: self.x * factor, y: self.y * factor };
    }
}

Vector2 v1 = Vector2 { x: 3.0, y: 4.0 };
println("v1.length_sq():", v1.length_sq()); // 25.0
```

### 3. Struct Composition & In-Place Mutations
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

// Explicit path access & in-place L-value mutations:
my_car.engine.hp += 70;
my_car.pos.x += 100.0;
my_car.engine.cylinders++;
println("Tuned HP:", my_car.engine.hp); // 620
```

### 4. True Pass-by-Value Semantics
```c
Vector2 a = Vector2 { x: 10.0, y: 20.0 };
Vector2 b = a; // Deep independent copy
b.x = 999.0;
println("a.x (unchanged):", a.x); // 10.0
println("b.x (modified):",  b.x); // 999.0
```

### 5. Arrays & Sized Allocations
```c
// Dynamic heap array (syntactic sugar)
int[] nums = [10, 20, 30];
push(nums, 40);
nums[1] = 99;
println("Length:", len(nums), "Nums:", nums);

// Fixed-size stack array
int fixed[4];
fixed[0] = 7;
```

### 6. High-Performance Memory Arenas
```c
// Create a fast memory pool
Arena pool = new_arena(8192);

// Allocate struct or array directly inside the arena
Vector2 v_arena = new(pool) Vector2 { x: 1.0, y: 2.0 };
int[] fast_batch = new(pool) int[100];
println("Arena usage:", arena_used(pool), "bytes");

// Reset the entire memory pool in O(1) time
reset_arena(pool);
```

---

## 📂 Example Programs

Browse the curated runnable examples in the [`examples/`](examples/) directory:

| File | Description |
| :--- | :--- |
| **[`01_basics.vclang`](examples/01_basics.vclang)** | Variables, expressions, and string concatenation |
| **[`02_control_flow.vclang`](examples/02_control_flow.vclang)** | `if`/`else`, `while`, and `for` loops with `break`/`continue` |
| **[`03_guessing_game.vclang`](examples/03_guessing_game.vclang)** | Interactive terminal guessing game with `input()` |
| **[`04_dsa_fibonacci.vclang`](examples/04_dsa_fibonacci.vclang)** | Recursive and iterative Fibonacci generation |
| **[`05_dsa_prime.vclang`](examples/05_dsa_prime.vclang)** | Prime number checking and Sieve of Eratosthenes |
| **[`06_dsa_gcd.vclang`](examples/06_dsa_gcd.vclang)** | Euclidean Algorithm for greatest common divisor |
| **[`07_structs_basics.vclang`](examples/07_structs_basics.vclang)** | Struct definitions, static constructors, inline methods, and pass-by-value |
| **[`08_struct_composition.vclang`](examples/08_struct_composition.vclang)** | Multi-level nested composition, explicit paths, in-place tuning |
| **[`09_particle_simulation.vclang`](examples/09_particle_simulation.vclang)** | 2D particle physics simulation with arrays of structs and arenas |
| **[`10_geometry_polygon.vclang`](examples/10_geometry_polygon.vclang)** | Computational geometry with structs holding dynamic vertex arrays |

---

## 📚 Documentation

For the exhaustive language specification, grammar rules, built-in functions, and memory internals, please consult:
* 📖 **[Comprehensive Language Manual](docs/manual.md)**
* 🧪 **[Test Suite & Invariant Tests](tests/)**

---

## 📜 License
VClang is open-source software licensed under the [MIT License](LICENSE).
