# Tisp - Tiny Lisp Compiler

Tisp is a lightweight compiler for a lisp-like programming language that compiles to LLVM IR, then generates native code through the LLVM toolchain. The shortcut for a tisp file is `.tsp`, like teaspoon (so pround of this name). The entire "codebase" is one file, and a little bit under 300 lines (excluding newlines and comments).

## Features

- **Type system**: Supports integers and floating-point numbers with automatic type promotion
- **Functions**: Define and call functions (with recursion support)
- **Control flow**: 
  - `if` expressions for conditional branching
  - `cond` for multi-way conditionals
  - `loop` for iteration
- **Arithmetic operations**: `+`, `-`, `*`, `/`
- **Comparisons**: `<`, `>`, `=`
- **Variables**: Local variable definitions with `define`
- **LLVM backend**: Compiles to LLVM IR, then to native code

## Requirements

- **C++ compiler** with filesystem support
- **LLVM toolchain** (specifically `llc`)
- **GCC or Clang** for linking

## Building

Compile the compiler itself:

```bash
g++ -std=c++17 -o tisp src/tisp.cpp
```

## Usage

### Basic Usage

Compile a `.tsp` file to an executable:

```bash
tisp program.tsp
```

This will generate an executable with the same name as the input file (without the `.tsp` extension).

### Command-Line Options

```
Usage: tisp <input.tsp> [options]

Options:
  -o <output>   Specify output executable name
  --emit-ir     Emit LLVM IR only (.ll)
  --emit-asm    Emit assembly only (.s)
  --emit-obj    Emit object file only (.o)
  --verbose     Preserve all intermediate files
  --help        Show this help message
  --version     Show version information
```

## How It Works

1. **Lexing & Parsing**: The compiler tokenizes and parses the source code into an abstract syntax tree
2. **IR Generation**: Generates LLVM Intermediate Representation (IR)
3. **Optimization**: Uses `llc -O2` to optimize and convert IR to assembly
4. **Linking**: Links the assembly code with GCC or Clang to produce a native executable

## License

This project is provided as-is for all purposes, and was really just for my own fun - it's pretty bad code, and really not practical, but if you want to use it, it's yours. Feel free to use, modify, and distribute as you wish.

## Acknowledgments

* [The LLVM Project](https://llvm.org) - The real backbone of all these little fun projects I do.
* [The Dragon Book](https://faculty.sist.shanghaitech.edu.cn/faculty/songfu/cav/Dragon-book.pdf) - For foundational compiler theory, and lots on lexing and parsing.
* [Wisp by Adam McDaniel](https://github.com/adam-mcdaniel/wisp) - A minimal Lisp-like language built with LLVM.
* [LLVM IR with the C++ API](https://mukulrathi.com/create-your-own-programming-language/llvm-ir-cpp-api-tutorial) - Tutorial by Mukul Rathi
* [“Hello, LLVM”](https://jameshamilton.eu/programming/llvm-hello-world) - Tutorial by James Hamilton
* [Awesome LLVM](https://github.com/learn-llvm/awesome-llvm) - A curated list of useful LLVM resources.
* [Jesse Squires' Blog](https://www.jessesquires.com/blog/2020/12/28/resources-for-learning-about-compilers-and-llvm) - A great roundup of compiler and LLVM learning materials.
* [Building an Optimizing Compiler](https://turbo51.com/download/Building-an-Optimizing-Compile-Book-Preview.pdf) - Old, but still very good, helpful with high level design.