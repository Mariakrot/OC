#!/bin/bash
# save as: benchmark_concurrent.sh

SRC="test_data.bin"
DST="test_out.bin"
BLOCK_SIZE=262144 # Фиксированный размер блока (можно изменить под ваши нужды)
FILE_SIZE_MB=500

# Создаём тестовый файл
echo "Creating ${FILE_SIZE_MB}MB test file..."
dd if=/dev/zero of="$SRC" bs=1M count=$FILE_SIZE_MB status=none

echo "Concurrency,Time_ms,Throughput_MB_s" > results_concurrent.csv

for concurrent in 1 2 4 8 12 16; do
    echo "Testing concurrent operations: $concurrent..."
    
    times=()
    throughputs=()
    for run in {1..3}; do
        output=$(./lab1 "$SRC" "$DST" $BLOCK_SIZE $concurrent 2>/dev/null)
        time_ms=$(echo "$output" | grep "Time:" | awk '{print $2}')
        throughput=$(echo "$output" | grep "Throughput:" | awk '{print $2}')
        times+=("$time_ms")
        throughputs+=("$throughput")
    done
    
    # Сортируем: время по возрастанию, throughput по убыванию
    # -g корректно работает с дробными числами
    best_time=$(printf '%s\n' "${times[@]}" | sort -g | head -1)
    best_throughput=$(printf '%s\n' "${throughputs[@]}" | sort -rg | head -1)
    
    echo "$concurrent,$best_time,$best_throughput" >> results_concurrent.csv
    rm -f "$DST"
done

rm -f "$SRC"
echo "✅ Результаты сохранены в results_concurrent.csv"