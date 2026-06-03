#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#define BLOCK_ROWS 32
#define BLOCK_COLS 32

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <errno.h>

typedef struct {
    const Matrix *left;
    const Matrix *right;
    Matrix *out;
    size_t block_begin;
    size_t block_end;
    int thread_id;
    int core_id;
} WorkerArgs;

static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

static size_t min_size(size_t a, size_t b) {
    return a < b ? a : b;
}

static int pin_current_thread_to_core(int core_id) {
    if (core_id < 0) {
        return EINVAL;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET((size_t)core_id, &cpuset);

    return pthread_setaffinity_np(
        pthread_self(),
        sizeof(cpu_set_t),
        &cpuset
    );
}

static size_t element_size(DataType dtype) {
    return (dtype == TYPE_FLOAT) ? sizeof(float) : sizeof(double);
}

static int matrix_is_valid(const Matrix *m) {
    return (m != NULL && m->data != NULL && m->rows > 0 && m->cols > 0);
}

static int dims_match_for_mul(const Matrix *left, const Matrix *right, const Matrix *out) {
    if (!matrix_is_valid(left) || !matrix_is_valid(right) || !matrix_is_valid(out)) {
        return 0;
    }
    if (left->dtype != right->dtype || left->dtype != out->dtype) {
        return 0;
    }
    if (left->cols != right->rows) {
        return 0;
    }
    if (out->rows != left->rows || out->cols != right->cols) {
        return 0;
    }
    return 1;
}

int parse_dtype(const char *s, DataType *dtype) {
    if (s == NULL || dtype == NULL) {
        return -1;
    }

    if (strcmp(s, "float") == 0) {
        *dtype = TYPE_FLOAT;
        return 0;
    }
    if (strcmp(s, "double") == 0) {
        *dtype = TYPE_DOUBLE;
        return 0;
    }

    return -1;
}

const char *dtype_name(DataType dtype) {
    return (dtype == TYPE_FLOAT) ? "float" : "double";
}

int matrix_alloc(Matrix *m, size_t rows, size_t cols, DataType dtype) {
    if (m == NULL || rows == 0 || cols == 0) {
        return -1;
    }

    if (rows > SIZE_MAX / cols) {
        return -1;
    }

    size_t count = rows * cols;
    size_t elem = element_size(dtype);

    if (count > SIZE_MAX / elem) {
        return -1;
    }

    size_t bytes = count * elem;
    void *ptr = malloc(bytes);
    if (ptr == NULL) {
        return -1;
    }

    m->rows = rows;
    m->cols = cols;
    m->dtype = dtype;
    m->data = ptr;
    return 0;
}

void matrix_free(Matrix *m) {
    if (m != NULL) {
        free(m->data);
        m->data = NULL;
        m->rows = 0;
        m->cols = 0;
    }
}

void matrix_zero(Matrix *m) {
    if (!matrix_is_valid(m)) {
        return;
    }

    size_t bytes = m->rows * m->cols * element_size(m->dtype);
    memset(m->data, 0, bytes);
}

static double rand_unit(void) {
    return (double) arc4random() / (double) UINT32_MAX;
}

void matrix_fill_random(Matrix *m) {
    if (!matrix_is_valid(m)) {
        return;
    }

    size_t count = m->rows * m->cols;

    if (m->dtype == TYPE_FLOAT) {
        float *a = (float *) m->data;
        for (size_t i = 0; i < count; i++) {
            a[i] = (float) rand_unit();
        }
    } else {
        double *a = (double *) m->data;
        for (size_t i = 0; i < count; i++) {
            a[i] = rand_unit();
        }
    }
}

bool matrix_compare(const Matrix *a, const Matrix *b, double *max_diff) {
    if (max_diff != NULL) {
        *max_diff = 0.0;
    }

    if (!matrix_is_valid(a) || !matrix_is_valid(b)) {
        return false;
    }
    if (a->rows != b->rows || a->cols != b->cols || a->dtype != b->dtype) {
        return false;
    }

    bool ok = true;
    size_t count = a->rows * a->cols;

    if (a->dtype == TYPE_FLOAT) {
        const float *x = (const float *) a->data;
        const float *y = (const float *) b->data;
        const double tol = 1e-4;

        for (size_t i = 0; i < count; i++) {
            double diff = fabs((double)x[i] - (double)y[i]);
            double bound = tol * (1.0 + fabs((double)x[i]) + fabs((double)y[i]));
            if (max_diff != NULL && diff > *max_diff) {
                *max_diff = diff;
            }
            if (diff > bound) {
                ok = false;
            }
        }
    } else {
        const double *x = (const double *) a->data;
        const double *y = (const double *) b->data;
        const double tol = 1e-9;

        for (size_t i = 0; i < count; i++) {
            double diff = fabs(x[i] - y[i]);
            double bound = tol * (1.0 + fabs(x[i]) + fabs(y[i]));
            if (max_diff != NULL && diff > *max_diff) {
                *max_diff = diff;
            }
            if (diff > bound) {
                ok = false;
            }
        }
    }

    return ok;
}

double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}

MulOrder choose_best_order(size_t m, size_t k, size_t l, size_t n) {
    long double cost_ab_then_c =
        (long double)m * (long double)k * (long double)l +
        (long double)m * (long double)l * (long double)n;

    long double cost_a_then_bc =
        (long double)k * (long double)l * (long double)n +
        (long double)m * (long double)k * (long double)n;

    if (cost_ab_then_c <= cost_a_then_bc) {
        return ORDER_AB_THEN_C;
    }
    return ORDER_A_THEN_BC;
}

const char *order_name(MulOrder order) {
    return (order == ORDER_AB_THEN_C) ? "((A x B) x C)" : "(A x (B x C))";
}

static int matmul_sequential_once(const Matrix *left, const Matrix *right, Matrix *out) {
    if (!dims_match_for_mul(left, right, out)) {
        return -1;
    }

    size_t rows = left->rows;
    size_t shared = left->cols;
    size_t cols = right->cols;

    if (left->dtype == TYPE_FLOAT) {
        const float *A = (const float *) left->data;
        const float *B = (const float *) right->data;
        float *C = (float *) out->data;

        for (size_t bi = 0; bi < rows; bi += BLOCK_ROWS) {
            for (size_t bj = 0; bj < cols; bj += BLOCK_COLS) {
                size_t i_end = min_size(bi + BLOCK_ROWS, rows);
                size_t j_end = min_size(bj + BLOCK_COLS, cols);

                for (size_t i = bi; i < i_end; i++) {
                    for (size_t j = bj; j < j_end; j++) {
                        float sum = 0.0f;
                        for (size_t p = 0; p < shared; p++) {
                            sum += A[i * shared + p] * B[p * cols + j];
                        }
                        C[i * cols + j] = sum;
                    }
                }
            }
        }
    } else {
        const double *A = (const double *) left->data;
        const double *B = (const double *) right->data;
        double *C = (double *) out->data;

        for (size_t bi = 0; bi < rows; bi += BLOCK_ROWS) {
            for (size_t bj = 0; bj < cols; bj += BLOCK_COLS) {
                size_t i_end = min_size(bi + BLOCK_ROWS, rows);
                size_t j_end = min_size(bj + BLOCK_COLS, cols);

                for (size_t i = bi; i < i_end; i++) {
                    for (size_t j = bj; j < j_end; j++) {
                        double sum = 0.0;
                        for (size_t p = 0; p < shared; p++) {
                            sum += A[i * shared + p] * B[p * cols + j];
                        }
                        C[i * cols + j] = sum;
                    }
                }
            }
        }
    }

    return 0;
}

static void *worker_matmul(void *arg) {
    WorkerArgs *w = (WorkerArgs *) arg;

    int pin_result = pin_current_thread_to_core(w->core_id);
    int running_core = sched_getcpu();

    pthread_mutex_lock(&print_mutex);

    if (pin_result == 0) {
        printf(
            "[Pinning] Thread %d SUCCESS: requested logical CPU %d, running on logical CPU %d, blocks [%zu, %zu)\n",
            w->thread_id,
            w->core_id,
            running_core,
            w->block_begin,
            w->block_end
        );
    } else {
        printf(
            "[Pinning] Thread %d FAILED: requested logical CPU %d, error = %s, blocks [%zu, %zu)\n",
            w->thread_id,
            w->core_id,
            strerror(pin_result),
            w->block_begin,
            w->block_end
        );
    }

    pthread_mutex_unlock(&print_mutex);

    const Matrix *left = w->left;
    const Matrix *right = w->right;
    Matrix *out = w->out;

    size_t rows = left->rows;
    size_t shared = left->cols;
    size_t cols = right->cols;

    size_t num_block_cols = (cols + BLOCK_COLS - 1) / BLOCK_COLS;

    if (left->dtype == TYPE_FLOAT) {
        const float *A = (const float *) left->data;
        const float *B = (const float *) right->data;
        float *C = (float *) out->data;

        for (size_t block_id = w->block_begin; block_id < w->block_end; block_id++) {
            size_t block_row = block_id / num_block_cols;
            size_t block_col = block_id % num_block_cols;

            size_t bi = block_row * BLOCK_ROWS;
            size_t bj = block_col * BLOCK_COLS;

            size_t i_end = min_size(bi + BLOCK_ROWS, rows);
            size_t j_end = min_size(bj + BLOCK_COLS, cols);

            for (size_t i = bi; i < i_end; i++) {
                for (size_t j = bj; j < j_end; j++) {
                    float sum = 0.0f;
                    for (size_t p = 0; p < shared; p++) {
                        sum += A[i * shared + p] * B[p * cols + j];
                    }
                    C[i * cols + j] = sum;
                }
            }
        }
    } else {
        const double *A = (const double *) left->data;
        const double *B = (const double *) right->data;
        double *C = (double *) out->data;

        for (size_t block_id = w->block_begin; block_id < w->block_end; block_id++) {
            size_t block_row = block_id / num_block_cols;
            size_t block_col = block_id % num_block_cols;

            size_t bi = block_row * BLOCK_ROWS;
            size_t bj = block_col * BLOCK_COLS;

            size_t i_end = min_size(bi + BLOCK_ROWS, rows);
            size_t j_end = min_size(bj + BLOCK_COLS, cols);

            for (size_t i = bi; i < i_end; i++) {
                for (size_t j = bj; j < j_end; j++) {
                    double sum = 0.0;
                    for (size_t p = 0; p < shared; p++) {
                        sum += A[i * shared + p] * B[p * cols + j];
                    }
                    C[i * cols + j] = sum;
                }
            }
        }
    }

    return NULL;
}

static int matmul_parallel_once(const Matrix *left, const Matrix *right, Matrix *out, int threads) {
    if (!dims_match_for_mul(left, right, out)) {
        return -1;
    }
    if (threads < 1) {
        return -1;
    }

    size_t rows = out->rows;
    size_t cols = out->cols;

    size_t num_block_rows = (rows + BLOCK_ROWS - 1) / BLOCK_ROWS;
    size_t num_block_cols = (cols + BLOCK_COLS - 1) / BLOCK_COLS;
    size_t total_blocks = num_block_rows * num_block_cols;

    long available_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (available_cpus < 1) {
        available_cpus = 1;
    }

    pthread_t *tids = (pthread_t *) malloc((size_t)threads * sizeof(pthread_t));
    WorkerArgs *args = (WorkerArgs *) malloc((size_t)threads * sizeof(WorkerArgs));

    if (tids == NULL || args == NULL) {
        free(tids);
        free(args);
        return -1;
    }

    for (int t = 0; t < threads; t++) {
        size_t block_begin = (size_t)t * total_blocks / (size_t)threads;
        size_t block_end = (size_t)(t + 1) * total_blocks / (size_t)threads;

        args[t].left = left;
        args[t].right = right;
        args[t].out = out;
        args[t].block_begin = block_begin;
        args[t].block_end = block_end;
        args[t].thread_id = t;
        args[t].core_id = t % (int)available_cpus;

        if (pthread_create(&tids[t], NULL, worker_matmul, &args[t]) != 0) {
            for (int j = 0; j < t; j++) {
                pthread_join(tids[j], NULL);
            }
            free(tids);
            free(args);
            return -1;
        }
    }

    for (int t = 0; t < threads; t++) {
        pthread_join(tids[t], NULL);
    }

    free(tids);
    free(args);
    return 0;
}

int run_pipeline_sequential(const Matrix *A, const Matrix *B, const Matrix *C,
                            Matrix *T, Matrix *D, MulOrder order) {
    if (!matrix_is_valid(A) || !matrix_is_valid(B) || !matrix_is_valid(C) ||
        !matrix_is_valid(T) || !matrix_is_valid(D)) {
        return -1;
    }

    matrix_zero(T);
    matrix_zero(D);

    if (order == ORDER_AB_THEN_C) {
        if (matmul_sequential_once(A, B, T) != 0) {
            return -1;
        }
        if (matmul_sequential_once(T, C, D) != 0) {
            return -1;
        }
    } else {
        if (matmul_sequential_once(B, C, T) != 0) {
            return -1;
        }
        if (matmul_sequential_once(A, T, D) != 0) {
            return -1;
        }
    }

    return 0;
}

int run_pipeline_parallel(const Matrix *A, const Matrix *B, const Matrix *C,
                          Matrix *T, Matrix *D, MulOrder order, int threads) {
    if (!matrix_is_valid(A) || !matrix_is_valid(B) || !matrix_is_valid(C) ||
        !matrix_is_valid(T) || !matrix_is_valid(D)) {
        return -1;
    }

    matrix_zero(T);
    matrix_zero(D);

    if (order == ORDER_AB_THEN_C) {
        if (matmul_parallel_once(A, B, T, threads) != 0) {
            return -1;
        }
        if (matmul_parallel_once(T, C, D, threads) != 0) {
            return -1;
        }
    } else {
        if (matmul_parallel_once(B, C, T, threads) != 0) {
            return -1;
        }
        if (matmul_parallel_once(A, T, D, threads) != 0) {
            return -1;
        }
    }

    return 0;
}