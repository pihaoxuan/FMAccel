#!/bin/bash

# 检查是否传入了标签参数
if [ -z "$1" ]; then
    echo "Usage: $0 <Engine_Label>"
    echo "Example: $0 Official"
    echo "Example: $0 Optimized"
    exit 1
fi

ENGINE_LABEL=$1
OUTPUT_FILE="eval_results.csv"

# 测试的矩阵维度列表和对应的迭代次数
# SIZES=(128 256 512 1024 2048 4096)
# ITERS=(1000 400 200 100 50 20)

SIZES=(128 256 512 1024 2048 4096 8192)
ITERS=(2000 1000 500 200 100 50 20)
# 如果输出文件不存在，则初始化 CSV 表头
if [ ! -f "$OUTPUT_FILE" ]; then
    echo "Engine,N,Time_s,GFLOPS" > "$OUTPUT_FILE"
    echo "Created new output file: $OUTPUT_FILE"
fi

echo "=================================================="
echo "Starting Evaluation for engine: $ENGINE_LABEL"
echo "=================================================="

for i in "${!SIZES[@]}"; do
    N=${SIZES[$i]}
    ITER=${ITERS[$i]}
    
    echo -n "Testing N=$N with $ITER iterations... "

    # 由于配置了 binfmt_misc，直接执行可执行文件即可
    # 底层会自动调用当前注册的 QEMU 版本
    RES=$(./benchmark $N $ITER | grep "GFLOPS")

    # 简单的错误处理：如果执行失败或没有拿到预期输出
    if [ -z "$RES" ]; then
        echo "Failed! Please check your binfmt_misc configuration or binary."
        continue
    fi

    # 提取时间与 GFLOPS
    TIME_VAL=$(echo "$RES" | awk -F'Avg_Time=' '{print $2}' | awk '{print $1}')
    GFLOPS_VAL=$(echo "$RES" | awk -F'GFLOPS=' '{print $2}')

    # 追加到统一的 CSV 文件
    echo "${ENGINE_LABEL},${N},${TIME_VAL},${GFLOPS_VAL}" >> "$OUTPUT_FILE"
    echo "Done. (Time: ${TIME_VAL}s, GFLOPS: ${GFLOPS_VAL})"
done

echo "=================================================="
echo "Evaluation finished for $ENGINE_LABEL."
echo "Results appended to $OUTPUT_FILE"
echo "=================================================="
