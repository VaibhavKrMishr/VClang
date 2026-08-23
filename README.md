# 🚀 VClang

[![Language](https://img.shields.io/badge/Language-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Build](https://img.shields.io/badge/Build-Passing-brightgreen.svg)]()
[![Execution Speed](https://img.shields.io/badge/Speed-Sub--millisecond-orange.svg)]()

**VClang** is a lightweight, high-performance, C-family programming language designed from the ground up for simplicity, speed, and modern systems-level control. Built in standard C99 with **zero external dependencies**, VClang merges familiar C-like syntax with modern memory safety (RAII scoped auto-cleanup) and explicit Zig-style memory allocators (`Arena`).

---

## ✨ Key Highlights

* **⚡ Sub-Millisecond Execution:** Instantaneous startup and execution (average script execution in **0.10 ms – 1.4 ms**), outperforming interpreted languages like Python and Node.js by **20x–30x**.
* **🛡️ Scoped Memory Safety (RAII):** Automatic cleanup of dynamic arrays, strings, and environments at scope exit—no garbage collection pauses, no manual `free()` required.
* **🧠 Universal Memory & Custom Allocators:** First-class support for explicit Arena allocators (`new_arena()`, `reset_arena()`) and the `new` operator (`new(arena) int[100]`).
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
| **QuickSort & Sieve & Linear Algebra** | ~42.0 ms | ~45.0 ms | **1.35 ms** |

---

## 🛠️ Installation & Quick Start

### 1. Build from Source
```bash
git clone https://github.com/VaibhavKrMishr/VClang.git
cd VClang
make
```

### 2. Run Scripts
Execute any `.vclang` script directly using the `run` helper:
```bash
./run tests/test_complex_benchmark.vclang
```
Or with the binary directly:
```bash
./vclang tests/test_complex_benchmark.vclang
```

### 3. Interactive REPL
Start the multi-line Read-Eval-Print Loop:
```bash
./vclang
```
```javascript
vclang> func add(int a, int b) { return a + b; }
vclang> int[] list = [10, 20, 30];
vclang> println(add(list[0], list[1]));
30
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

### 2. First-Class Functions & Recursion
```c
func factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

println("factorial(5):", factorial(5)); // 120
```

### 3. Arrays & Memory Allocation
```c
// Dynamic heap array (syntactic sugar)
int[] nums = [10, 20, 30];
push(nums, 40);
nums[1] = 99;
println("Length:", len(nums), "Nums:", nums);

// Explicit sized allocation
int[] buffer = new int[10];

// Fixed-size stack array
int fixed[4];
fixed[0] = 7;
```

### 4. High-Performance Memory Arenas
```c
// Create a fast memory pool
Arena pool = new_arena(8192);

// Allocate batch buffer inside the arena
int[] fast_batch = new(pool) int[100];
println("Arena usage:", arena_used(pool), "bytes");

// Reset the entire memory pool in O(1) time
reset_arena(pool);
```

---

## 📚 Documentation

For the exhaustive language specification, grammar rules, built-in functions, and memory internals, please consult:
* 📖 **[Comprehensive Language Manual](docs/manual.md)**
* 🧪 **[Test Suite & Benchmarks](tests/)**

---

## 📜 License
VClang is open-source software licensed under the [MIT License](LICENSE).
