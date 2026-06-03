#!/bin/bash
# run_eval_multi.sh

if [ -z "$1" ]; then
    echo "Usage: $0 <Engine_Label>"
    exit 1
fi

ENGINE_LABEL=$1
OUTPUT_FILE="eval_results_multi.csv"

# 4 种 op
OPS=(dgemv)

# 矩阵尺寸 + 内层 iter 数（与现有 DGEMM run_eval.sh 完全一致的 K_N 表）
SIZES=(128 256 512 1024 2048 4096 8192)
ITERS=(1000 500 200 100 50 20 10)

# 每个数据点做 1 次独立 process launch 
NUM_LAUNCHES=1

if [ ! -f "$OUTPUT_FILE" ]; then
    echo "Engine,Op,N,LaunchID,Time_s,GFLOPS" > "$OUTPUT_FILE"
fi

for OP in "${OPS[@]}"; do
    for i in "${!SIZES[@]}"; do
        N=${SIZES[$i]}
        ITER=${ITERS[$i]}
        for L in $(seq 1 $NUM_LAUNCHES); do
            echo -n "[${ENGINE_LABEL}] op=$OP N=$N iter=$ITER launch=$L ... "
            RES=$(./benchmark-mul-test $OP $N $ITER | grep "GFLOPS")
            if [ -z "$RES" ]; then
                echo "FAILED"
                continue
            fi
            TIME_VAL=$(echo "$RES" | awk -F'Avg_Time=' '{print $2}' | awk '{print $1}')
            GFLOPS_VAL=$(echo "$RES" | awk -F'GFLOPS=' '{print $2}')
            echo "${ENGINE_LABEL},${OP},${N},${L},${TIME_VAL},${GFLOPS_VAL}" >> "$OUTPUT_FILE"
            echo "GFLOPS=${GFLOPS_VAL}"
        done
    done
done
