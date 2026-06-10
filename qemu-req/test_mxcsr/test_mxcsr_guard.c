/*
 * 验证 FMAccel MXCSR 守卫：在向零舍入模式下，向量化 helper 必须
 * 回退至软浮点，给出与 C 参考实现相同的结果。
 *
 * 编译（x86）：gcc -O2 -mavx2 -mfma -o test_mxcsr_guard test_mxcsr_guard.c -lm
 * 在 qemu-opt 下运行：./test_mxcsr_guard
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <immintrin.h>
#include <fenv.h>

/* 通过 AVX2 FMA 指令在向零舍入模式下计算 a*b+c */
static double fma_rtz_avx(__m256d va, __m256d vb, __m256d vc)
{
    uint32_t orig = _mm_getcsr();
    /* RC = 11b（向零舍入）= 位 [14:13] = 0x6000 */
    _mm_setcsr((orig & ~0x6000U) | 0x6000U);
    __m256d vres = _mm256_fmadd_pd(va, vb, vc);
    _mm_setcsr(orig);
    return ((double *)&vres)[0];
}

/* 参考值：使用 fesetround(FE_TOWARDZERO) 的 C fma() */
static double fma_rtz_ref(double a, double b, double c)
{
    int orig = fegetround();
    fesetround(FE_TOWARDZERO);
    double r = fma(a, b, c);
    fesetround(orig);
    return r;
}

int main(void)
{
    /* 使用 RN 和 RZ 结果不同的数值 */
    double a = 1.0 + 1e-15;
    double b = 1.0 + 2e-15;
    double c = 1e-30;

    __m256d va = _mm256_set1_pd(a);
    __m256d vb = _mm256_set1_pd(b);
    __m256d vc = _mm256_set1_pd(c);

    double got = fma_rtz_avx(va, vb, vc);
    double ref = fma_rtz_ref(a, b, c);

    if (got == ref) {
        printf("PASS: MXCSR 守卫在向零舍入下正确 (got=%.20e)\n", got);
        return 0;
    } else {
        printf("FAIL: got=%.20e  ref=%.20e\n", got, ref);
        return 1;
    }
}
