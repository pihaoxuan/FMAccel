    #include <stdio.h>
    #include <stdlib.h>
    #include <time.h>
    #include <cblas.h>

    // 获取高精度时间（秒）
    double get_time() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec + ts.tv_nsec * 1e-9;
    }

    // 随机初始化矩阵
    void init_matrix(double *mat, int N) {
        for (int i = 0; i < N * N; i++) {
            mat[i] = (double)rand() / RAND_MAX;
        }
    }

    int main(int argc, char *argv[]) {
        if (argc != 3) {
            printf("Usage: %s <Matrix_Size_N> <Num_Iterations>\n", argv[0]);
            return 1;
        }

        int N = atoi(argv[1]);
        int iterations = atoi(argv[2]);

        // 动态分配内存
        double *A = (double *)malloc(N * N * sizeof(double));
        double *B = (double *)malloc(N * N * sizeof(double));
        double *C = (double *)malloc(N * N * sizeof(double));

        if (!A || !B || !C) {
            printf("Memory allocation failed!\n");
            return -1;
        }

        init_matrix(A, N);
        init_matrix(B, N);

        // 1. 预热阶段 (Warmup)
        // 目的：让 QEMU 触发并完成 translate.c 的翻译工作，包括你的拦截和 Hash 查找
        // 这样正式计时时，测量的才是纯粹的执行期 (Execution Phase) 性能
        cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                    N, N, N, 1.0, A, N, B, N, 0.0, C, N);

        // 2. 正式性能测量阶段
        double start_time = get_time();

        for (int i = 0; i < iterations; i++) {
            cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                        N, N, N, 1.0, A, N, B, N, 0.0, C, N);
        }

        double end_time = get_time();
        double total_time = end_time - start_time;
        double avg_time = total_time / iterations;

        // 3. 计算 GFLOPS
        // 矩阵乘法 C = A * B，浮点操作总数为 2 * N^3
        double gflops = (2.0 * (double)N * (double)N * (double)N) / avg_time / 1e9;

        printf("N=%d, Iters=%d, Avg_Time=%.5f s, GFLOPS=%.2f\n", N, iterations, avg_time, gflops);

        free(A);
        free(B);
        free(C);

        return 0;
    }
