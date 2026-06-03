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

## 5. Core data structures

### `DataType`

```c
typedef enum {
    TYPE_FLOAT = 0,
    TYPE_DOUBLE = 1
} DataType;
```

This enum tells the program whether matrix elements are stored as `float` or `double`.

### `MulOrder`

```c
typedef enum {
    ORDER_AB_THEN_C = 0,
    ORDER_A_THEN_BC = 1
} MulOrder;
```

This enum stores the chosen multiplication order:

- `((A × B) × C)`
- `(A × (B × C))`

### `Matrix`

```c
typedef struct {
    size_t rows;
    size_t cols;
    DataType dtype;
    void *data;
} Matrix;
```

This struct represents a matrix object.

It stores:

- number of rows
- number of columns
- data type
- pointer to the raw contiguous memory block

---

## 6. Function overview

### Functions in `main.c`

#### `parse_positive_size`
Safely converts a string into a positive `size_t` value.

Used for:

- `m`
- `k`
- `l`
- `n`
- thread count

It rejects invalid strings, zero, and negative values.

#### `print_usage`
Prints the correct command-line format when the user provides the wrong number of arguments.

#### `main`
The entry point of the program.

It controls the whole program flow:

- parse arguments
- check validity
- choose multiplication order
- allocate matrices
- initialize data
- run sequential computation
- run parallel computation
- compare outputs
- print results
- free memory

---

### Functions in `utils.c`

#### `parse_dtype`
Converts the input string:

- `"float"`
- `"double"`

into the internal enum values:

- `TYPE_FLOAT`
- `TYPE_DOUBLE`

#### `dtype_name`
Converts the internal `DataType` enum back into a printable string.

Useful for final output.

#### `matrix_alloc`
Allocates memory for a matrix using `malloc`.

It:

- checks that dimensions are valid
- computes the total number of elements
- chooses element size based on `float` or `double`
- allocates a contiguous memory block
- stores the information in a `Matrix` struct

#### `matrix_free`
Frees the matrix memory and resets the struct fields.

#### `matrix_zero`
Sets the whole matrix memory block to zero.

Used before computation for temporary and output matrices.

#### `rand_unit`
Generates a random floating-point number between 0 and 1.

Used internally to initialize matrix values.

#### `matrix_fill_random`
Fills every element of a matrix with a random value between 0 and 1.

It handles both `float` and `double` matrices.

#### `matrix_compare`
Compares two matrices element by element.

It checks whether the sequential and parallel results are close enough.

It also records the maximum difference found.

#### `now_seconds`
Returns the current monotonic time in seconds.

Used to measure execution time.

#### `choose_best_order`
Compares the theoretical cost of the two multiplication orders:

- `((A × B) × C)`
- `(A × (B × C))`

and returns the cheaper one.

#### `order_name`
Returns a printable string describing the chosen multiplication order.

#### `matrix_is_valid`
Checks whether a matrix is usable.

This is an internal helper function.

#### `dims_match_for_mul`
Checks whether two matrices can be multiplied and whether the output matrix has the correct size and data type.

This is an internal helper function.

#### `matmul_sequential_once`
Performs one normal single-thread matrix multiplication.

Examples:

- `A × B → T`
- `T × C → D`
- `B × C → T`
- `A × T → D`

This is the core sequential computation function.

#### `WorkerArgs`
A helper struct used to pass information into a pthread worker.

It stores:

- left matrix
- right matrix
- output matrix
- starting row index
- ending row index

#### `worker_matmul`
This is the worker function executed by each thread.

Each thread computes only a subset of rows in the output matrix.

This is how 1D row partitioning is implemented.

#### `matmul_parallel_once`
Performs one parallel matrix multiplication using pthreads.

It:

- creates the threads
- divides output rows among them
- starts the worker function on each thread
- waits for all threads to finish

#### `run_pipeline_sequential`
Runs the full sequential pipeline for computing `D = A × B × C`.

Depending on the chosen order, it does either:

- `T = A × B`, then `D = T × C`
- or `T = B × C`, then `D = A × T`

#### `run_pipeline_parallel`
Runs the full parallel pipeline.

It performs the same two-step matrix multiplication as the sequential version, but each matrix multiplication is computed with pthreads.

---

## 7. Data flow

### Case 1: `((A × B) × C)`

```text
A, B, C
↓
T = A × B
↓
D = T × C
↓
Final result D
```

### Case 2: `(A × (B × C))`

```text
A, B, C
↓
T = B × C
↓
D = A × T
↓
Final result D
```

The temporary matrix `T` stores the intermediate result between the two multiplication steps.

---

## 8. Parallel design

The parallel implementation uses pthreads.

The output matrix rows are divided among threads using 1D row partitioning.

That means:

- each thread is assigned a consecutive block of rows
- each thread writes only to its own rows
- threads do not write to the same output elements

This avoids race conditions during output computation.

---

## 9. Why parallel can be slower for small inputs

For very small matrices, the parallel version may be slower than the sequential version.

This is normal because parallel execution also introduces overhead:

- thread creation
- thread scheduling
- thread joining
- work distribution

If the matrix is too small, the overhead can be larger than the actual computation time.

For larger matrices, the parallel version is more likely to show speedup.

---

## 10. Example output

```text
Dimensions: A(64 x 64), B(64 x 64), C(64 x 64)
Datatype: float
Threads: 4
Chosen order: ((A x B) x C)
Sequential time: 0.001342 s
Parallel time: 0.001378 s
Speedup: 0.973414
Correctness: PASS
Max difference: 0
```

---

## 11. Notes

- The current version focuses on correctness and clarity.
- It is a baseline Task 1 implementation.
- It does not apply loop unrolling.
- Large matrix inputs can take a long time, especially with `-O0`.
- For early testing, start with small sizes such as `64`, `128`, or `256`.

---

## 12. Test commands

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

## 13. Memory measurement

```bash
/usr/bin/time -v ./task2 64 64 64 64 float 4
/usr/bin/time -v ./task2 67 74 62 83 float 4
/usr/bin/time -v ./task2 256 256 256 256 double 4
```