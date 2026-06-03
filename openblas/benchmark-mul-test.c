#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cblas.h>

double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void init_d(double *m, int n)  { for (int i=0;i<n;i++) m[i] = (double)rand()/RAND_MAX; }
void init_s(float  *m, int n)  { for (int i=0;i<n;i++) m[i] = (float)rand()/RAND_MAX;  }

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <op> <N> <iter>\n  op: dgemm | sgemm | dsyrk | dgemv\n", argv[0]);
        return 1;
    }
    const char *op = argv[1];
    int N = atoi(argv[2]);
    int iter = atoi(argv[3]);
    double flops = 0.0;       // 一次操作的浮点数
    double avg_time = 0.0;

    if (strcmp(op, "dgemm") == 0) {
        double *A = malloc(N*N*sizeof(double));
        double *B = malloc(N*N*sizeof(double));
        double *C = malloc(N*N*sizeof(double));
        init_d(A, N*N); init_d(B, N*N);
        cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, N,N,N, 1.0, A,N, B,N, 0.0, C,N); // warmup
        double t0 = get_time();
        for (int i = 0; i < iter; i++)
            cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, N,N,N, 1.0, A,N, B,N, 0.0, C,N);
        avg_time = (get_time() - t0) / iter;
        flops = 2.0 * N * N * N;
        free(A); free(B); free(C);

    } else if (strcmp(op, "sgemm") == 0) {
        float *A = malloc(N*N*sizeof(float));
        float *B = malloc(N*N*sizeof(float));
        float *C = malloc(N*N*sizeof(float));
        init_s(A, N*N); init_s(B, N*N);
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, N,N,N, 1.0f, A,N, B,N, 0.0f, C,N); // warmup
        double t0 = get_time();
        for (int i = 0; i < iter; i++)
            cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, N,N,N, 1.0f, A,N, B,N, 0.0f, C,N);
        avg_time = (get_time() - t0) / iter;
        flops = 2.0 * N * N * N;
        free(A); free(B); free(C);

    } else if (strcmp(op, "dsyrk") == 0) {
        // C := alpha * A * A^T + beta * C, A is N x N
        double *A = malloc(N*N*sizeof(double));
        double *C = malloc(N*N*sizeof(double));
        init_d(A, N*N);
        cblas_dsyrk(CblasRowMajor, CblasUpper, CblasNoTrans, N, N, 1.0, A,N, 0.0, C,N); // warmup
        double t0 = get_time();
        for (int i = 0; i < iter; i++)
            cblas_dsyrk(CblasRowMajor, CblasUpper, CblasNoTrans, N, N, 1.0, A,N, 0.0, C,N);
        avg_time = (get_time() - t0) / iter;
        flops = 1.0 * N * N * N;   // dsyrk 浮点数约为 dgemm 一半
        free(A); free(C);

    } else if (strcmp(op, "dgemv") == 0) {
        // y := alpha * A * x + beta * y
        double *A = malloc(N*N*sizeof(double));
        double *x = malloc(N*sizeof(double));
        double *y = malloc(N*sizeof(double));
        init_d(A, N*N); init_d(x, N);
        cblas_dgemv(CblasRowMajor, CblasNoTrans, N, N, 1.0, A,N, x,1, 0.0, y,1); // warmup
        double t0 = get_time();
        for (int i = 0; i < iter; i++)
            cblas_dgemv(CblasRowMajor, CblasNoTrans, N, N, 1.0, A,N, x,1, 0.0, y,1);
        avg_time = (get_time() - t0) / iter;
        flops = 2.0 * N * N;       // dgemv: 2*N^2 浮点数
        free(A); free(x); free(y);

    } else {
        printf("Unknown op: %s\n", op);
        return 2;
    }

    double gflops = flops / avg_time / 1e9;
    printf("Op=%s, N=%d, Iters=%d, Avg_Time=%.5f s, GFLOPS=%.2f\n", op, N, iter, avg_time, gflops);
    return 0;
}
