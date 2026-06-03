#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>

#include "utils.h"

static int parse_positive_size(const char *s, size_t *out) {
    if (s == NULL || out == NULL) {
        return -1;
    }

    errno = 0;
    char *end = NULL;
    unsigned long long value = strtoull(s, &end, 10);

    if (errno != 0 || end == s || *end != '\0' || value == 0ULL) {
        return -1;
    }

    if (value > (unsigned long long) SIZE_MAX) {
        return -1;
    }

    *out = (size_t) value;
    return 0;
}

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s m k l n float|double threads\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        print_usage(argv[0]);
        return 1;
    }

    size_t m, k, l, n, thread_count_sz;
    DataType dtype;

    if (parse_positive_size(argv[1], &m) != 0 ||
        parse_positive_size(argv[2], &k) != 0 ||
        parse_positive_size(argv[3], &l) != 0 ||
        parse_positive_size(argv[4], &n) != 0) {
        fprintf(stderr, "Error: m, k, l, n must be positive integers.\n");
        return 1;
    }

    if (parse_dtype(argv[5], &dtype) != 0) {
        fprintf(stderr, "Error: datatype must be exactly \"float\" or \"double\".\n");
        return 1;
    }

    if (parse_positive_size(argv[6], &thread_count_sz) != 0) {
        fprintf(stderr, "Error: thread count must be a positive integer.\n");
        return 1;
    }

    if (thread_count_sz < 1 || thread_count_sz > 64) {
        fprintf(stderr, "Error: thread count must be between 1 and 64.\n");
        return 1;
    }

    int threads = (int) thread_count_sz;

    MulOrder order = choose_best_order(m, k, l, n);

    size_t t_rows = 0;
    size_t t_cols = 0;
    if (order == ORDER_AB_THEN_C) {
        t_rows = m;
        t_cols = l;
    } else {
        t_rows = k;
        t_cols = n;
    }

    Matrix A = {0}, B = {0}, C = {0};
    Matrix T = {0}, D_seq = {0}, D_par = {0};

    if (matrix_alloc(&A, m, k, dtype) != 0 ||
        matrix_alloc(&B, k, l, dtype) != 0 ||
        matrix_alloc(&C, l, n, dtype) != 0 ||
        matrix_alloc(&T, t_rows, t_cols, dtype) != 0 ||
        matrix_alloc(&D_seq, m, n, dtype) != 0 ||
        matrix_alloc(&D_par, m, n, dtype) != 0) {
        fprintf(stderr, "Error: memory allocation failed.\n");
        matrix_free(&A);
        matrix_free(&B);
        matrix_free(&C);
        matrix_free(&T);
        matrix_free(&D_seq);
        matrix_free(&D_par);
        return 1;
    }

    matrix_fill_random(&A);
    matrix_fill_random(&B);
    matrix_fill_random(&C);

    double seq_start = now_seconds();
    if (run_pipeline_sequential(&A, &B, &C, &T, &D_seq, order) != 0) {
        fprintf(stderr, "Error: sequential computation failed.\n");
        matrix_free(&A);
        matrix_free(&B);
        matrix_free(&C);
        matrix_free(&T);
        matrix_free(&D_seq);
        matrix_free(&D_par);
        return 1;
    }
    double seq_time = now_seconds() - seq_start;

    double par_start = now_seconds();
    if (run_pipeline_parallel(&A, &B, &C, &T, &D_par, order, threads) != 0) {
        fprintf(stderr, "Error: parallel computation failed.\n");
        matrix_free(&A);
        matrix_free(&B);
        matrix_free(&C);
        matrix_free(&T);
        matrix_free(&D_seq);
        matrix_free(&D_par);
        return 1;
    }
    double par_time = now_seconds() - par_start;

    double max_diff = 0.0;
    bool ok = matrix_compare(&D_seq, &D_par, &max_diff);

    printf("Dimensions: A(%zu x %zu), B(%zu x %zu), C(%zu x %zu)\n",
           m, k, k, l, l, n);
    printf("Datatype: %s\n", dtype_name(dtype));
    printf("Threads: %d\n", threads);
    printf("Chosen order: %s\n", order_name(order));
    printf("Sequential time: %.6f s\n", seq_time);
    printf("Parallel time: %.6f s\n", par_time);

    if (par_time > 0.0) {
        printf("Speedup: %.6f\n", seq_time / par_time);
    } else {
        printf("Speedup: inf\n");
    }

    printf("Correctness: %s\n", ok ? "PASS" : "FAIL");
    printf("Max difference: %.12g\n", max_diff);

    matrix_free(&A);
    matrix_free(&B);
    matrix_free(&C);
    matrix_free(&T);
    matrix_free(&D_seq);
    matrix_free(&D_par);

    return ok ? 0 : 2;
}