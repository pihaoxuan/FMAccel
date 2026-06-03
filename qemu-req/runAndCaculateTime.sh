#!/bin/bash
# time-command.sh
# 用法: ./time-command.sh <tag> <命令> [参数...]

if [ $# -lt 2 ]; then
    echo "用法: $0 <tag> <命令> [参数...]"
    echo "示例: $0 mytest sleep 1"
    exit 1
fi

# 第一个参数为用户自定义 tag
USER_TAG="$1"
shift

# 脚本所在目录（仅用于存放 CSV）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 执行目录名称作为第二个 tag
DIR_TAG="$(basename "$PWD")"

# CSV 文件路径（保存在脚本所在目录）
CSV_FILE="$SCRIPT_DIR/time-command-results.csv"

# 若 CSV 不存在则写入表头
if [ ! -f "$CSV_FILE" ]; then
    echo "timestamp,user_tag,dir_tag,command,run1_sec,run2_sec,run3_sec,run4_sec,run5_sec,avg_sec,exit_status" > "$CSV_FILE"
fi

RUNS=5
COMMAND="$*"
total=0
times=()
last_status=0

echo "开始对命令 '$COMMAND' 进行 $RUNS 次计时..."

for i in $(seq 1 $RUNS); do
    start=$(date +%s.%N)
    "$@"
    last_status=$?
    end=$(date +%s.%N)

    elapsed=$(echo "$end - $start" | bc)
    times+=("$elapsed")
    total=$(echo "$total + $elapsed" | bc)

    printf "  第 %d 次: %.6f 秒\n" "$i" "$elapsed"
done

# 计算平均值
avg=$(echo "scale=6; $total / $RUNS" | bc)

# 当前时间戳
timestamp=$(date "+%Y-%m-%d %H:%M:%S")

# 拼接 CSV 行
csv_line="$timestamp,\"$USER_TAG\",\"$DIR_TAG\",\"$COMMAND\",${times[0]},${times[1]},${times[2]},${times[3]},${times[4]},$avg,$last_status"
echo "$csv_line" >> "$CSV_FILE"

echo "----------------------------------------"
printf "命令 '%s' 执行完毕\n" "$COMMAND"
printf "各次耗时: %s / %s / %s / %s / %s 秒\n" "${times[@]}"
printf "平均耗时: %.6f 秒\n" "$avg"
echo "结果已追加至: $CSV_FILE"

exit $last_status
