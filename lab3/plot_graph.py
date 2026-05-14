#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Визуализация результатов бенчмарка.
Зависит только от стандартной библиотеки и matplotlib.
"""
import sys
import os
import csv

try:
    import matplotlib.pyplot as plt
except ImportError:
    print("❌ Ошибка: отсутствует библиотека matplotlib.")
    print("Установите её командой: python -m pip install matplotlib")
    sys.exit(1)

def main():
    # 1. Получение имени файла
    filename = sys.argv[1] if len(sys.argv) > 1 else input("Введите имя CSV-файла (по умолчанию 'results.csv'): ").strip() or "results.csv"
    
    if not os.path.exists(filename):
        print(f"❌ Ошибка: Файл '{filename}' не найден.")
        sys.exit(1)

    print(f"📂 Чтение данных из '{filename}'...")
    threads, times = [], []

    # 2. Чтение CSV стандартным модулем csv
    try:
        with open(filename, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            for row in reader:
                threads.append(int(row['threads']))
                times.append(float(row['avg_time_ms']))
    except Exception as e:
        print(f"❌ Ошибка чтения файла: {e}")
        sys.exit(1)

    if not threads:
        print("❌ Файл пуст или не содержит данных.")
        sys.exit(1)

    # Сортировка по количеству потоков
    sorted_data = sorted(zip(threads, times))
    threads, times = zip(*sorted_data)

    # 3. Поиск оптимума
    min_idx = times.index(min(times))
    opt_t, opt_time = threads[min_idx], times[min_idx]

    print(f"\n📋 Результаты:")
    for t, time_ms in zip(threads, times):
        marker = " 👈 ОПТИМУМ" if t == opt_t else ""
        print(f"   Потоков: {t:>2} | Время: {time_ms:>8.3f} мс{marker}")

    print(f"\n🏆 Лучший результат: {opt_t} потоков ({opt_time:.3f} мс)")

    # 4. Построение графика
    plt.figure(figsize=(10, 6))
    
    plt.plot(threads, times, marker='o', linewidth=2, markersize=8, color='#2E86AB', label='Время выполнения')
    plt.scatter(opt_t, opt_time, color='#E94F37', s=100, zorder=5, label=f'Оптимум ({opt_t} потоков)')
    plt.annotate(f'{opt_time:.1f} ms', (opt_t, opt_time), xytext=(0, 10), textcoords="offset points", ha='center', color='#E94F37', fontweight='bold')

    plt.title('Зависимость времени вычисления π от количества потоков', fontsize=14, fontweight='bold')
    plt.xlabel('Количество потоков', fontsize=12)
    plt.ylabel('Среднее время выполнения (мс)', fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.xticks(threads)
    plt.gca().invert_yaxis()  # Чем ниже точка, тем быстрее
    plt.legend()
    plt.tight_layout()

    plt.savefig('benchmark_plot.png', dpi=300)
    print("\n💾 График сохранён в 'benchmark_plot.png'")
    plt.show()

if __name__ == "__main__":
    main()