#!/bin/bash
# save as: benchmark_block.sh

SRC="test_data.bin"
DST="test_out.bin"
CONCURRENT=1
FILE_SIZE_MB=500

# Создаём тестовый файл
echo "Creating ${FILE_SIZE_MB}MB test file..."
dd if=/dev/zero of="$SRC" bs=1M count=$FILE_SIZE_MB status=none

echo "BlockSize,Time_ms,Throughput_MB_s" > results_block.csv

for bs in 4096 8192 16384 32768 65536 131072 262144 524288 1084576; do
    echo "Testing block size: $bs bytes..."
    
    # Запускаем 3 раза
    times=()
    throughputs=()
    for run in {1..3}; do
        output=$(./lab1 "$SRC" "$DST" $bs $CONCURRENT 2>/dev/null)
        time_ms=$(echo "$output" | grep "Time:" | awk '{print $2}')
        throughput=$(echo "$output" | grep "Throughput:" | awk '{print $2}')
        times+=("$time_ms")
        throughputs+=("$(echo "$output" | grep "Throughput:" | awk '{print $2}')")
    done
    
    # Берём лучший результат (минимальное время)
    best_time=$(printf '%s\n' "${times[@]}" | sort -n | head -1)
    best_throughput=$(printf '%s\n' "${throughputs[@]}" | sort -rg | head -1)
    
    echo "$bs,$best_time,$best_throughput" >> results_block.csv
    rm -f "$DST"
done

rm -f "$SRC"
echo "✅ Результаты сохранены в results_block.csv"