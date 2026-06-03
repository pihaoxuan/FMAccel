#!/bin/bash
# batch-time-advanced.sh
# 用法: ./batch-time-advanced.sh [重复次数]
# 默认重复次数 10

DIR="$(cd "$(dirname "$0")" && pwd)"            # 脚本所在目录（即项目根目录）
DETECTOR_DIR="$(cd "$DIR/../detectorAndQEMU" && pwd)"  # detector 目录
REPEAT="${1:-10}"

# -------------------
# 获取所有子目录
PROJECTS=()
for d in "$DIR"/*/; do
    [ -d "$d" ] && PROJECTS+=("$(basename "$d")")
done

if [ ${#PROJECTS[@]} -eq 0 ]; then
    echo "没有子目录可测试！"
    exit 1
fi

# 检查 detector.py 是否存在
if [ ! -f "$DETECTOR_DIR/detector.py" ]; then
    echo "错误：未找到 detector.py（期望路径: $DETECTOR_DIR/detector.py）"
    exit 1
fi

# 跨平台计时（秒，带小数）
get_time() {
    if date +%s.%N 2>/dev/null | grep -q '\.'; then
        date +%s.%N
    else
        python3 -c "import time; print(time.time())"
    fi
}

# 保存结果的数组
declare -A RESULTS

# -------------------
# 批量执行
for proj in "${PROJECTS[@]}"; do
    echo "==============================="
    echo "开始测试项目: $proj"
    CMD_FILE="$DIR/$proj/cmd.sh"

    if [ ! -f "$CMD_FILE" ]; then
        echo "  未找到 cmd.sh，跳过"
        continue
    fi

    CMD=$(cat "$CMD_FILE")
    # 从 cmd.sh 里解析出第一个参数作为二进制文件路径
    BINARY=$(echo "$CMD" | awk '{print $1}')
    # 如果是相对路径，转成基于项目目录的绝对路径
    if [[ "$BINARY" != /* ]]; then
        BINARY="$DIR/$proj/$BINARY"
    fi
    echo "  命令:   $CMD"
    echo "  二进制: $BINARY"
    TIMES=()

    for ((i=1; i<=REPEAT; i++)); do
        # Step 1: 运行 detector，生成检测信息供 qemu 读取
        echo "  [第 $i 次] 运行 detector..."
        (cd "$DETECTOR_DIR" && python3 detector.py "$BINARY")

        # Step 2: 计时执行真正的命令
        start=$(get_time)
        (cd "$DIR/$proj" && eval "$CMD")
	echo "$(pwd)"
        end=$(get_time)
        elapsed=$(echo "$end - $start" | bc)
        TIMES+=("$elapsed")
        printf "  第 %2d 次: %ss\n" "$i" "$elapsed"
    done

    RESULTS[$proj]="${TIMES[*]}"
done

# -------------------
# 计算统计值并输出表格
echo
echo "===== 测试结果汇总 ====="
echo

# 列宽（项目名列动态）
MAX_PROJ_LEN=7  # 最小宽度，"项目名"三字
for proj in "${PROJECTS[@]}"; do
    len=${#proj}
    [ $len -gt $MAX_PROJ_LEN ] && MAX_PROJ_LEN=$len
done

# 打印分隔线
print_line() {
    printf "+-%-${MAX_PROJ_LEN}s-+------------+------------+------------+------------+\n" \
        "$(printf '%*s' $MAX_PROJ_LEN '' | tr ' ' '-')"
}

# 表头
print_line
printf "| %-${MAX_PROJ_LEN}s | %10s | %10s | %10s | %10s |\n" \
    "项目名" "最小(s)" "平均(s)" "最大(s)" "总计(s)"
print_line

for proj in "${PROJECTS[@]}"; do
    TIMES=(${RESULTS[$proj]})
    if [ ${#TIMES[@]} -eq 0 ]; then
        printf "| %-${MAX_PROJ_LEN}s | %10s | %10s | %10s | %10s |\n" \
            "$proj" "N/A" "N/A" "N/A" "N/A"
        continue
    fi

    min=${TIMES[0]}
    max=${TIMES[0]}
    sum=0

    for t in "${TIMES[@]}"; do
        sum=$(echo "$sum + $t" | bc)
        [ "$(echo "$t < $min" | bc)" -eq 1 ] && min=$t
        [ "$(echo "$t > $max" | bc)" -eq 1 ] && max=$t
    done

    avg=$(echo "scale=6; $sum / ${#TIMES[@]}" | bc)
    # 保证格式统一（保留6位小数）
    min=$(printf "%.6f" "$min")
    max=$(printf "%.6f" "$max")
    avg=$(printf "%.6f" "$avg")
    sum=$(printf "%.6f" "$sum")

    printf "| %-${MAX_PROJ_LEN}s | %10s | %10s | %10s | %10s |\n" \
        "$proj" "$min" "$avg" "$max" "$sum"
done

print_line
echo
echo "（共测试 ${#PROJECTS[@]} 个项目，每项重复 $REPEAT 次）"
