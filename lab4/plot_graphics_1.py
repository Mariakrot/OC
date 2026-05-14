import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

def generate_plot(file_path):
    # 1. Загрузка и подготовка данных
    df = pd.read_csv(file_path)
    
    # Очистка имен столбцов (на случай лишних пробелов)
    df.columns = df.columns.str.strip()
    
    # Нормализуем время: вычитаем минимальный timestamp, чтобы график начинался с 0
    df['time_ms'] = df['timestamp'] - df['timestamp'].min()
    df['time_sec'] = df['time_ms'] / 1000.0
    
    # Создаем уникальный идентификатор для легенды (Тип + PID)
    df['process'] = df['type'] + " " + df['pid'].astype(str)

    # Устанавливаем стиль
    sns.set_theme(style="whitegrid")
    plt.rcParams.update({'font.size': 10})

    plt.figure(figsize=(14, 8))
    
    # Маппинг цветов для состояний
    color_map = {
        'BEGINNING OF WAITING': 'orange',
        'BEGINING OF WAITING': 'orange', # обработка опечатки из лога
        'READING': 'green',
        'RECORD': 'red',
        'TRANSITION TO LIBERATION': 'gray'
    }

    # Для построения "линий" состояний нам нужно знать время окончания
    # Мы аппроксимируем его как время начала следующего события для того же PID
    df_sorted = df.sort_values(['pid', 'time_sec'])
    
    for pid in df['pid'].unique():
        proc_data = df_sorted[df_sorted['pid'] == pid].copy()
        proc_label = proc_data['process'].iloc[0]
        
        for i in range(len(proc_data) - 1):
            start = proc_data.iloc[i]['time_sec']
            end = proc_data.iloc[i+1]['time_sec']
            state = proc_data.iloc[i]['state'].strip()
            
            plt.hlines(y=proc_label, xmin=start, xmax=end, 
                       colors=color_map.get(state, 'blue'), 
                       linewidth=8, alpha=0.8)

    plt.title('Смена состояний процессов (Reader-Writer Timeline)', fontsize=15)
    plt.xlabel('Время (секунды)')
    plt.ylabel('Процесс (Тип + PID)')
    
    # Создание кастомной легенды
    from matplotlib.lines import Line2D
    legend_elements = [Line2D([0], [0], color='orange', lw=4, label='Ожидание'),
                       Line2D([0], [0], color='green', lw=4, label='Чтение'),
                       Line2D([0], [0], color='red', lw=4, label='Запись'),
                       Line2D([0], [0], color='gray', lw=4, label='Освобождение')]
    plt.legend(handles=legend_elements, loc='upper right')
    
    plt.tight_layout()
    plt.savefig('state_transitions.png')
    plt.show()

def generate_occupancy(file_path, bin_size_ms=2500):
    # 1. Загрузка и очистка
    df = pd.read_csv(file_path)
    df.columns = df.columns.str.strip()
    df['time_ms'] = df['timestamp'] - df['timestamp'].min()
    
    # 2. Формируем интервалы активности процессов
    intervals = []
    df_sorted = df.sort_values(['pid', 'time_ms'])
    
    for pid in df['pid'].unique():
        pdf = df_sorted[df_sorted['pid'] == pid]
        start_time = None
        page = None
        
        for _, row in pdf.iterrows():
            state = row['state'].strip()
            if state in ['READING', 'RECORD']:
                start_time = row['time_ms']
                page = row['page']
            elif state == 'TRANSITION TO LIBERATION' and start_time is not None:
                intervals.append({
                    'pid': pid,
                    'page': int(page),
                    'start': start_time,
                    'end': row['time_ms']
                })
                start_time = None

    inv_df = pd.DataFrame(intervals)
    
    # 3. Создаем сетку времени
    max_time = df['time_ms'].max()
    bins = np.arange(0, max_time + bin_size_ms, bin_size_ms)
    pages = sorted(inv_df['page'].unique())
    
    # Матрица для подсчета уникальных процессов
    # rows = страницы, cols = временные бины
    heatmap_data = np.zeros((len(pages), len(bins)-1))

    for i, p in enumerate(pages):
        page_intervals = inv_df[inv_df['page'] == p]
        for j in range(len(bins)-1):
            bin_start = bins[j]
            bin_end = bins[j+1]
            
            # Считаем количество уникальных PID, чьи интервалы пересекаются с текущим бином
            active_pids = page_intervals[
                (page_intervals['start'] < bin_end) & (page_intervals['end'] > bin_start)
            ]['pid'].nunique()
            
            heatmap_data[i, j] = active_pids

    # 4. Визуализация
    plt.figure(figsize=(16, 8))
    
    # Форматируем метки времени в секунды
    x_labels = [f"{b/1000:.1f}" for b in bins[:-1]]
    
    # Используем целочисленный формат для аннотаций (annot=True выведет цифры прямо в клетках)
    sns.heatmap(heatmap_data, 
                xticklabels=x_labels, 
                yticklabels=pages, 
                annot=False, 
                fmt=".0f", 
                cmap="YlGnBu", 
                cbar_kws={'label': 'Кол-во процессов на странице'})
    
    plt.title(f'Конкуренция процессов за страницы памяти (шаг {bin_size_ms}мс)', fontsize=15)
    plt.xlabel('Время выполнения (сек)')
    plt.ylabel('Номер страницы')
    
    plt.tight_layout()
    plt.savefig('concurrency_heatmap.png')
    plt.show()

generate_plot('combined_results.csv')
generate_occupancy('combined_results.csv')