# FMAccel

**Profile-Guided FMA Acceleration for x86-to-LoongArch Dynamic Binary Translation**

Artifact repository for the paper *FMAccel: Profile-Guided FMA Acceleration for x86-to-LoongArch Dynamic Binary Translation*. This README documents the role of each file/directory and how to build and run the experiments.

> 论文 *FMAccel: Profile-Guided FMA Acceleration for x86-to-LoongArch Dynamic Binary Translation* 的实验工件仓库。本文档说明各文件/目录的作用，以及如何构建并复现实验。

---

## Repository layout / 仓库结构

```
FMAccel/
├── detector.py            # Offline FMA profiler  (x86 binary → fma_profile.txt)
├── fma_profile.txt        # Generated profile: META: <hex offset> <decimal length> per FMA site
├── setup.sh               # GLib PKG_CONFIG_PATH helper (source it, don't bash it)
├── glib-prefix/           # Prebuilt GLib install prefix (dependency for the QEMU forks)
├── qemu-count/            # QEMU fork #1 — instruction counter / measurement build
├── qemu-opt/              # QEMU fork #2 — FMA acceleration build
├── openblas/              # LoongArch-NATIVE BLAS benchmarks (hardware ceiling)
└── qemu-req/              # x86-under-QEMU evaluation harness (rc0 vs opt)
    └── openblas/          # x86-compiled BLAS executables, run through QEMU
```

### Root files / 根目录文件

| File | Role |
|------|------|
| `detector.py` | **Offline profiler.** Runs `llvm-objdump -d <binary>`, regex-matches `vfmadd(132\|213\|231)(ps\|pd)` whose first opcode byte is `c4` (VEX-encoded; EVEX/AVX-512 excluded), and writes one `META: <hex offset> <length>` line per match to `fma_profile.txt`. Works on stripped binaries (byte-pattern matching, not symbols). Usage: `python3 detector.py <x86-binary>`. |
| `fma_profile.txt` | Generated profile consumed by **both** QEMU forks at startup. Maps guest virtual addresses → FMA instruction metadata (offset + length). |
| `setup.sh` | Exports `PKG_CONFIG_PATH` pointing at `glib-prefix/lib/loongarch64-linux-gnu/pkgconfig`, then prints the resolved GLib version. **Run with `source setup.sh`** (or copy the commands manually) — running via `bash` scopes the export to a child process and has no effect. |
| `glib-prefix/` | Prebuilt GLib install prefix. `fma_detector.c` uses GLib's `GHashTable` for the O(1) PC lookup; the forks link against this prefix via `pkg-config`. |

### The two QEMU forks / 两个 QEMU 分支

Both are QEMU forks that share the same offline-profile loader and the same FMA interception point in `target/i386/tcg/translate.c`. **Both are built identically on the LoongArch host** (same `configure` flags). They differ only in what the custom FMA helper does:

| | `qemu-count/` | `qemu-opt/` |
|---|---|---|
| Purpose | **Measurement** — quantify FMA execution density | **Acceleration** — the performance build (= QEMU 11.0.0-opt) |
| FMA helper | Increments FMA + total instruction counters | Executes the FMA via native LSX/LASX intrinsics |
| Per-insn counter | `gen_helper_count_total_insn()` at every instruction | Not present (avoids per-instruction overhead) |
| Output | `print_fma_ratio()` prints FMA% of total on exit | None — used for the speedup numbers |

Custom / modified files in each fork:

| File | Change |
|------|--------|
| `target/i386/tcg/fma_detector.{c,h}` | Loads `fma_profile.txt` into a `GHashTable` at startup (`init_gemm_detector`, `getMeta`); declares the execution counters. |
| `target/i386/tcg/translate.c` | Interception inserted at the top of `i386_tr_translate_insn`: on a profile hit, manually decode the VEX prefix and dispatch to the custom FMA helper, then end the TB; on a miss, fall through to QEMU's normal decoder. |
| `target/i386/tcg/misc_helper.c` | Defines `helper_custom_fma_vector_reg` / `helper_custom_fma_vector_mem`. In **qemu-opt** these call `__lasx_xvfmadd_d/s` (256-bit / YMM) and `__lsx_vfmadd_d/s` (128-bit / XMM); memory operands use `cpu_ldq_data_ra(env, vaddr, GETPC())` for fault-safe loads. |
| `target/i386/helper.h` | Declares the custom FMA helpers (and `count_total_insn` in qemu-count). |
| `linux-user/syscall.c` | (qemu-count) calls `print_fma_ratio()` on `TARGET_NR_exit_group`. |
| `target/i386/tcg/meson.build` | Adds `fma_detector.c` to the TCG build. |

### The two `openblas/` directories / 两套 openblas 目录

This is the most important distinction in the repo — the two directories hold **different binaries** for **different roles**:

- **`openblas/` (root)** — BLAS benchmarks **compiled natively on the Loongson host and run as native LoongArch code**. They produce the *hardware ceiling* (`native-loongarch64`) numbers and do **not** go through QEMU.
- **`qemu-req/openblas/`** — the **x86-compiled** benchmark executables, copied here to be **run through QEMU** via `binfmt_misc`. They produce the baseline (QEMU `rc0`) vs. optimized (`qemu-opt`) comparison.

> 根目录的 `openblas/` 是在龙芯本地编译、以 **LoongArch 原生**方式运行的基准，代表硬件上限（`native-loongarch64`），不经过 QEMU；`qemu-req/openblas/` 是 **x86 编译**摘出来的可执行文件，通过 `binfmt_misc` **由 QEMU 翻译运行**，用于对比 baseline（rc0）与优化版（opt）。两者不是同一份二进制。

| File (in either openblas dir) | Role |
|---|---|
| `benchmark.c` | DGEMM microbenchmark: warm-up + timed `cblas_dgemm`, prints `N, Avg_Time, GFLOPS`. |
| `benchmark-mul-test.c` | Multi-op benchmark, op selected by CLI arg: `dgemm` / `sgemm` / `dsyrk` / `dgemv`. |
| `run_eval.sh` | Sweeps matrix sizes (128–4096/8192), takes an engine label, appends rows to `eval_results.csv`. |
| `run_eval-mul-test.sh` | Same for the multi-op benchmark → `eval_results_multi.csv`. |
| `eval_results.csv`, `eval_results_multi.csv` | Recorded GFLOPS results. |

### `qemu-req/` evaluation harness / 评测脚本

| File | Role |
|------|------|
| `use-opt.sh` | Re-registers `binfmt_misc` so x86-64 ELF binaries route through the **optimized** QEMU (`qemu-11.0.0-opt`). Needs root. |
| `use-rc0.sh` | Same, routing through stock QEMU 11.0.0-rc0 (baseline). Needs root. |
| `runAndCaculateTime.sh` | Times a single command (`date +%s.%N` before/after) and prints elapsed seconds. Used for SPEC benchmarks. |
| `batch-time.sh` | Iterates benchmark subdirectories, runs `detector.py` to pre-generate the profile, then times each via `runAndCaculateTime.sh` and prints a summary. |
| `time-command-results.csv` | SPEC CPU 2017 timing results (rc0 vs opt). |

---

## Build & run / 构建与运行

> **Host requirement:** the `qemu-opt` build and the native benchmarks require a **LoongArch64 host** (Loongson 3A6000 in the paper; see `machinee-info.md`). The LSX/LASX intrinsics in `misc_helper.c` (`lasxintrin.h`, `lsxintrin.h`) only compile on LoongArch.

### 1. Build OpenBLAS + benchmark (statically) / 静态编译 OpenBLAS 与 benchmark

On the **x86** machine — build OpenBLAS for HASWELL (no AVX-512), install, then link the benchmark statically:

```bash
# OpenBLAS
cd OpenBLAS
make clean
make TARGET=HASWELL NO_AVX512=1 COMMON_OPT="-fcf-protection=none"
make install PREFIX=/data2/pihaoxuan/OpenBLAS/install

# benchmark (x86, static)
gcc benchmark.c -march=haswell \
    -I../OpenBLAS/install/include \
    ../OpenBLAS/install/lib/libopenblas.a \
    -o benchmark -lpthread -lm -static
```

Copy the resulting x86 `benchmark` to the LoongArch machine (→ `qemu-req/openblas/`).

On the **LoongArch** machine — also build OpenBLAS and the benchmark natively (set `march`/`TARGET` to `loongarch64`) to obtain the `native-loongarch64` ceiling (→ `openblas/`).

### 2. Build the two QEMU forks (on LoongArch) / 在龙芯上编译两个 QEMU

Both forks use the **same** configuration; `-mlsx -mlasx` enable the LSX/LASX intrinsics:

```bash
cd qemu-opt    # (and repeat for qemu-count)
mkdir build && cd build
../configure --target-list=x86_64-linux-user \
    --disable-system --disable-rust --disable-tools --disable-docs \
    --enable-debug --extra-cflags="-mlsx -mlasx"
make -j8
```

Make GLib discoverable first if needed: `source setup.sh`.

### 3. Generate the FMA profile / 生成 FMA 剖析文件

```bash
python3 detector.py benchmark    # → fma_profile.txt
```

The QEMU forks read `fma_profile.txt` from the fixed path expected by `fma_detector.c` (`init_gemm_detector`); place/symlink it accordingly.

### 4. Select the active QEMU via binfmt_misc / 切换 QEMU 版本

As root (`sudo su`), register which QEMU handles x86 binaries — afterwards an x86 executable can be run directly with `./`:

```bash
bash qemu-req/use-rc0.sh    # baseline (QEMU 11.0.0-rc0)
bash qemu-req/use-opt.sh    # optimized (QEMU 11.0.0-opt)
```

### 5. Run the benchmarks / 运行基准测试

```bash
# x86 binaries through QEMU — pass the engine label matching the active QEMU
cd qemu-req/openblas
bash run_eval.sh rc0
bash run_eval.sh opt        # results → eval_results.csv

# native LoongArch ceiling (no QEMU)
cd ../../openblas
bash run_eval.sh native-loongarch64
```

For SPEC CPU 2017: build statically with the SPEC config, extract the per-benchmark executables to LoongArch, then time each with `qemu-req/runAndCaculateTime.sh` under the chosen QEMU.

---

## Requirements / 环境要求

- **Host:** LoongArch64 (Loongson 3A6000), Loongnix GNU/Linux 20
- **ISA support:** LSX (128-bit) + LASX (256-bit); build QEMU with `-mlsx -mlasx`
- **Toolchain:** `llvm-objdump` (for `detector.py`), Python 3, GCC / Meson (QEMU build), `bc` (timing script)
- **Libraries:** OpenBLAS (static, HASWELL on x86 / loongarch64 native), GLib (via `glib-prefix/`)
- **Scope:** the current prototype covers VEX-encoded (`0xC4`) packed `vfmadd132/213/231 ps/pd`. EVEX/AVX-512 and scalar `vfmadd*sd/ss` variants are not profiled and fall back to QEMU's original translation path.
