import pandas as pd
import matplotlib.pyplot as plt

def create_performance_plot(file_name):
    try:
        # 1. Чтение данных из CSV
        # Предполагаем, что файл называется 'results_block.csv'
        df = pd.read_csv(file_name)
        
        # Выводим первые строки для проверки в консоли
        print("Данные успешно загружены:")
        print(df)

        # 2. Создание полотна для графика
        plt.figure(figsize=(10, 6))

        # 3. Построение графика
        # Используем BlockSize для оси X и Throughput_MB_s для оси Y
        plt.plot(df['Concurrency'], df['Throughput_MB_s'], marker='o', linestyle='-', color='b', label='Пропускная способность')

        # 4. Настройка оформления
        plt.title('Зависимость пропускной способности от количества перекрывающихся операций', fontsize=14)
        plt.xlabel('Количество операций', fontsize=12)
        plt.ylabel('Пропускная способность (MB/s)', fontsize=12)
        
        # Делаем ось X логарифмической, так как размеры блоков обычно растут в степени 2
        plt.xscale('log', base=2) 
        
        # Добавляем сетку
        plt.grid(True, which="both", ls="-", alpha=0.5)
        
        # Добавляем легенду
        plt.legend()

        # 5. Сохранение и показ
        plt.tight_layout()
        plt.savefig('performance_chart2.png') # Сохранит график в файл
        print("\nГрафик сохранен как performance_chart.png")
        plt.show()

    except FileNotFoundError:
        print(f"Ошибка: Файл {file_name} не найден. Проверьте путь.")
    except Exception as e:
        print(f"Произошла ошибка: {e}")

if __name__ == "__main__":
    # Укажи точное имя твоего файла здесь
    create_performance_plot('results_concurrent.csv')