# 🚀 VClang pre-alpha 

VClang is a lightweight, C-like interpreted programming language designed from the ground up for simplicity, speed, and educational purposes. If you've ever wanted to explore how a programming language evaluates logic, handles variables, and manages control flow under the hood, VClang is the perfect playground.

## 🤔 Why Use VClang?

*   **Educational Foundation**: Built in standard C, VClang is an excellent reference for anyone looking to learn about abstract syntax trees (ASTs), parsing, and custom interpreters.
*   **Zero Dependencies**: VClang is highly portable and builds rapidly with standard `gcc`. 
*   **Familiar C-Family Syntax**: If you know C, Java, or JavaScript, you will instantly feel at home writing VClang.
*   **Fast and Lightweight**: Scripts run in sub-millisecond times with incredibly low memory footprints.

## 🛠️ Installation

Building VClang from source is incredibly easy. All you need is a C compiler (`gcc`) and `make`.

```bash
# 1. Clone the repository (if applicable)
# git clone https://github.com/yourusername/vclang.git
# cd vclang

# 2. Build the interpreter
make
```

## 💻 How to Use

VClang provides three primary ways to interact with your code:

### 1. Fast Shortcut (Running Scripts)
You can run any `.vclang` script directly using the provided `run` helper script.
```bash
./run examples/06_dsa_gcd.vclang
```

### 2. Interactive REPL
Want to test out logic quickly? Run the interpreter without arguments to enter the Read-Eval-Print Loop.
```bash
./vclang
```
```javascript
vclang> int x = 10;
vclang> println(x * 2);
```

### 3. Compiling to a Binary
VClang also includes a compiler driver (`vcc`) that allows you to compile your script into a standalone executable.
```bash
./vcc my_script.vclang -o my_app
./my_app
```

## 📚 Documentation & Examples

VClang has a surprisingly robust feature set, including strong typing, string concatenation, explicit casting, control flow (`if/else`, `while`, `for`), and custom `println` semantics.

*   **📖 Full Language Manual**: For a comprehensive breakdown of the syntax, built-in functions, and language quirks, please read the [docs/manual.md](docs/manual.md).
*   **💡 Code Examples & DSA**: Check out the [examples/](examples/) directory! It is packed with sample scripts demonstrating all the language features, as well as a few Data Structures & Algorithms (DSA) challenges (like calculating Fibonacci sequences or checking for Prime numbers).

## 🧪 Testing

VClang comes with an automated test suite to ensure the interpreter remains stable when you make changes. To run the test suite:
```bash
make test
```
# VClang
