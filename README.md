# Task 1 README

## 1. What this program does

This program computes:

```text
D = A × B × C
```

The matrix dimensions are:

- `A`: `m × k`
- `B`: `k × l`
- `C`: `l × n`
- `D`: `m × n`

The program accepts six command-line arguments:

```bash
./task1 m k l n float|double threads
```

Example:

```bash
./task1 64 64 64 64 float 4
```

The program will:

1. Read and validate input arguments
2. Allocate memory for all matrices
3. Fill `A`, `B`, and `C` with random values between 0 and 1
4. Choose the cheaper multiplication order automatically
5. Run a sequential version
6. Run a parallel pthread version
7. Compare the results for correctness
8. Print execution times and speedup

---

## 2. Project structure

```text
task1/
├── main.c
├── utils.c
├── utils.h
└── Makefile
```

### `main.c`
This is the main control file. It is responsible for:

- parsing command-line arguments
- validating input
- allocating matrices
- calling sequential and parallel computation functions
- timing execution
- printing results
- freeing memory

### `utils.h`
This is the header file. It contains:

- type definitions
- enum definitions
- struct definitions
- function declarations

It acts like the interface of the project.

### `utils.c`
This file contains the actual implementation of helper functions, including:

- matrix allocation
- random initialization
- matrix zeroing
- matrix comparison
- timing
- multiplication order selection
- sequential matrix multiplication
- parallel matrix multiplication
- pthread worker logic

### `Makefile`
This file defines how to build the program.

It tells `make` how to:

- compile `main.c`
- compile `utils.c`
- link them into the final executable `task1`
- remove generated files

---

## 3. Build and run

To compile the program:

```bash
make clean
make
```

To run the program:

```bash
./task1 64 64 64 64 float 4
```

Another example:

```bash
./task1 256 256 256 256 double 8
```

---

## 4. High-level program workflow

The program follows this workflow:

1. Check the number of command-line arguments
2. Parse `m`, `k`, `l`, `n`, data type, and thread count
3. Choose the cheaper multiplication order
4. Decide the size of the temporary matrix `T`
5. Allocate matrices `A`, `B`, `C`, `T`, `D_seq`, and `D_par`
6. Fill `A`, `B`, and `C` with random numbers
7. Run the sequential pipeline
8. Run the parallel pipeline
9. Compare results
10. Print timing, speedup, and correctness
11. Free all allocated memory

---

## 5. Test commands

### Example Correct Usage

```bash
./task1 64 64 64 64 float 4
./task1 128 128 128 128 float 4
./task1 256 256 256 256 float 4
./task1 256 256 256 256 double 1
./task1 1024 2048 4096 1024 float 64
./task1 4096 8192 1024 2048 double 16
```

### Example Error Cases

```bash
./task1 100 100 100 100 complex 64
./task1 100 abcd 100 100 float 64
./task1 100 -200 100 100 float 64
./task1 100 200 100 100 float -64
```

---

## 6. Memory measurement

```bash
/usr/bin/time -v ./task2 64 64 64 64 float 4
/usr/bin/time -v ./task2 67 74 62 83 float 4
/usr/bin/time -v ./task2 256 256 256 256 double 4
```