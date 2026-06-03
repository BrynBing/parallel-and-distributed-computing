#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TYPE_FLOAT = 0,
    TYPE_DOUBLE = 1
} DataType;

typedef enum {
    ORDER_AB_THEN_C = 0,   // (A x B) x C
    ORDER_A_THEN_BC = 1    // A x (B x C)
} MulOrder;

typedef struct {
    size_t rows;
    size_t cols;
    DataType dtype;
    void *data;
} Matrix;

int parse_dtype(const char *s, DataType *dtype);
const char *dtype_name(DataType dtype);

int matrix_alloc(Matrix *m, size_t rows, size_t cols, DataType dtype);
void matrix_free(Matrix *m);
void matrix_zero(Matrix *m);
void matrix_fill_random(Matrix *m);

bool matrix_compare(const Matrix *a, const Matrix *b, double *max_diff);

double now_seconds(void);

MulOrder choose_best_order(size_t m, size_t k, size_t l, size_t n);
const char *order_name(MulOrder order);

int run_pipeline_sequential(const Matrix *A, const Matrix *B, const Matrix *C,
                            Matrix *T, Matrix *D, MulOrder order);

int run_pipeline_parallel(const Matrix *A, const Matrix *B, const Matrix *C,
                          Matrix *T, Matrix *D, MulOrder order, int threads);

#endif