# VClang: Language Reference and Documentation

VClang is a lightweight, dynamically typed (with explicit declarations), C-like interpreted language designed for simplicity and learning. It features a straightforward syntax, built-in casting, control structures, and an easy-to-use REPL.

## 1. Quick Start

### Installation & Compilation
To build the VClang interpreter and compiler from source:
```bash
make
```

### Running VClang
**Interactive REPL**:
```bash
./vclang
```
**Run a script (Fast Shortcut)**:
```bash
./run script.vclang
```
*(This is a convenience script that runs `./vclang script.vclang` under the hood)*

**Run a script manually**:
```bash
./vclang script.vclang
```
**Compile to a standalone binary**:
```bash
./vcc script.vclang -o my_app
./my_app
```

## 2. Variables and Data Types

VClang supports strong typing with three fundamental primitive types. Variables are declared using the syntax `type name = value;`.

### Primitive Types
*   **`int`**: 64-bit integers. (e.g., `10`, `-5`, `0`)
*   **`float`**: Double-precision floating-point numbers. (e.g., `3.14`, `0.001`, `-2.5`)
*   **`string`**: Text delimited by double `"` or single `'` quotes. (e.g., `"Hello"`, `'World'`)

> [!NOTE]
> Strings support standard escape sequences including `\n` (newline), `\t` (tab), `\r`, `\\`, `\"`, and `\'`.

### Examples
```c
int age = 21;
float pi = 3.14159;
string greeting = "Hello, VClang!";
```

## 3. Operations and Arithmetic

### Arithmetic Operators
Standard mathematical operators (`+`, `-`, `*`, `/`) are supported.

### Comparison Operators
VClang supports a full suite of comparison operators which evaluate to boolean-like integers (1 for true, 0 for false):
*   **`==`**: Equal to
*   **`!=`**: Not equal to
*   **`<`**: Less than
*   **`>`**: Greater than
*   **`<=`**: Less than or equal to
*   **`>=`**: Greater than or equal to

> [!TIP]
> Mixing `int` and `float` in an arithmetic operation will automatically promote the result to a `float`.

You can also use the `+` operator for **string concatenation**:
```c
string first = "Hello ";
string last = "World";
string full = first + last; // "Hello World"

// Automatic string conversion for numbers!
string message = "Your age is: " + 21; 
```

## 4. Input and Output

VClang has a unique but predictable approach to printing to the console.

*   **`print(val1, val2, ...)`**: Prints all arguments separated by a space, **without** adding any newlines.
*   **`println(val1, val2, ...)`**: Prints a newline character (`\n`) **BEFORE** it prints its arguments. It does *not* append a newline at the end.
*   **`input(prompt)`**: Displays the prompt and reads a line of text from the user, returning a `string`.

### Output Example
```c
string name = "Alice";
int age = 30;

print("Name:");
print(name);
println("Age:"); // Prints \n before "Age:"
print(age);

// Output:
// Name: Alice
// Age: 30
```

## 5. Control Flow

VClang supports standard C-style control structures for branching and looping.

### If / Else
```c
int score = 85;
if (score > 90) {
    println("Grade: A");
} else {
    println("Grade: B or lower");
}
```

### While Loop
```c
int count = 3;
while (count > 0) {
    println(count);
    count = count - 1;
}
```

### For Loop
```c
for (int i = 0; i < 5; i = i + 1) {
    if (i == 2) { 
        continue; // Skip iteration 2
    }
    if (i == 4) {
        break; // Exit loop early
    }
    println("Iter: " + i);
}
```

## 6. Type Conversion (Casting)

VClang provides built-in functions to explicitly cast between data types:

*   **`int(val)`**: Converts a float or string to an integer. If given a single character string, it returns its ASCII integer value.
*   **`float(val)`**: Converts an integer or string to a float.
*   **`string(val)`**: Converts an integer or float to its string representation.
*   **`char(val)`**: Converts an integer (ASCII value) into a single-character string.
*   **`len(string)`**: Returns the length (number of characters) of a string.

### Casting Examples
```c
int a = int(10.9);         // Result: 10
int b = int("123");        // Result: 123
int ascii = int("A");      // Result: 65

float f = float(10);       // Result: 10.0
float f2 = float("3.14");  // Result: 3.14

string s = string(100);    // Result: "100"
string letter = char(66);  // Result: "B"

int length = len("Hello"); // Result: 5
```

## 7. Comments

> [!IMPORTANT]
> VClang uses a unique syntax for comments. They are delimited by `//` at **both ends**. This allows them to act as both single-line and multi-line comments.

```c
// This is a single-line comment //
int x = 10;

//
  This is a 
  multi-line comment
  spanning several lines!
//
```
