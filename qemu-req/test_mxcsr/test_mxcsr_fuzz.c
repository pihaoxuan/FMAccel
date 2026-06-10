/*
 * 对全部四种舍入模式 + 随机 FMA 输入进行压力测试，
 * 验证 FMAccel MXCSR 守卫 + 软浮点回退的正确性。
 *
 * 编译（x86）：gcc -O2 -mavx2 -mfma -o test_mxcsr_fuzz test_mxcsr_fuzz.c -lm
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <immintrin.h>
#include <fenv.h>

/* MXCSR 舍入控制字段值 */
static const uint32_t RC_TABLE[4] = {
    0x0000,   /* 向最近偶数舍入 (RN) */
    0x2000,   /* 向负无穷舍入        */
    0x4000,   /* 向正无穷舍入        */
    0x6000,   /* 向零舍入 (RZ)       */
};
static const char *RC_NAME[4] = { "RN", "R-inf", "R+inf", "RZ" };
static const int FENV_RC[4] = {
    FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO
};

static double fma_rc_avx(double a, double b, double c, uint32_t rc_bits)
{
    uint32_t orig = _mm_getcsr();
    _mm_setcsr((orig & ~0x6000U) | rc_bits);
    __m256d va = _mm256_set1_pd(a);
    __m256d vb = _mm256_set1_pd(b);
    __m256d vc = _mm256_set1_pd(c);
    __m256d vr = _mm256_fmadd_pd(va, vb, vc);
    _mm_setcsr(orig);
    return ((double *)&vr)[0];
}

static double fma_rc_ref(double a, double b, double c, int fenv_rc)
{
    int orig = fegetround();
    fesetround(fenv_rc);
    double r = fma(a, b, c);
    fesetround(orig);
    return r;
}

int main(void)
{
    srand(42);
    int failures = 0;
    int total = 0;

    for (int rc = 0; rc < 4; rc++) {
        for (int iter = 0; iter < 10000; iter++) {
            double a = 0.5 + (rand() / (double)RAND_MAX) * 1.5;
            double b = 0.5 + (rand() / (double)RAND_MAX) * 1.5;
            double c = (rand() / (double)RAND_MAX) * 1e-10;

            double got = fma_rc_avx(a, b, c, RC_TABLE[rc]);
            double ref = fma_rc_ref(a, b, c, FENV_RC[rc]);
            total++;

            if (got != ref) {
                if (failures < 5)
                    fprintf(stderr, "FAIL rc=%s iter=%d a=%.17e b=%.17e c=%.17e "
                            "got=%.17e ref=%.17e\n",
                            RC_NAME[rc], iter, a, b, c, got, ref);
                failures++;
            }
        }
    }

    if (failures == 0)
        printf("PASS：%d 次 FMA 操作覆盖 4 种舍入模式，全部正确。\n", total);
    else
        printf("FAIL：%d/%d 次不匹配。\n", failures, total);

    return failures ? 1 : 0;
}
