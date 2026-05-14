import csv
import os
import glob
import re

def parse_logs_and_cleanup():
    output_file = 'combined_results.csv'
    log_files = glob.glob("*.log")
    
    if not log_files:
        print("Лог-файлы не найдены!")
        return

    data_rows = []

    for filename in log_files:
        # Определяем тип и PID
        match = re.match(r"(reader|writer)_(\d+)\.log", filename)
        if match:
            proc_type = match.group(1)
            pid = match.group(2)
        else:
            proc_type = "unknown"
            pid = "unknown"

        # Читаем данные
        try:
            with open(filename, 'r', encoding='utf-8') as f:
                for line in f:
                    line = line.strip()
                    if not line: continue
                    
                    parts = [p.strip() for p in line.split('|')]
                    timestamp = parts[0]
                    state = parts[1]
                    page = parts[2].replace("PAGE", "").strip() if len(parts) > 2 else ""
                    
                    data_rows.append({
                        'timestamp': timestamp,
                        'type': proc_type,
                        'pid': pid,
                        'state': state,
                        'page': page
                    })
            
            # --- УДАЛЕНИЕ ФАЙЛА ПОСЛЕ ЧТЕНИЯ ---
            os.remove(filename)
            print(f"Файл {filename} обработан и удален.")
            
        except Exception as e:
            print(f"Ошибка при обработке {filename}: {e}")

    if not data_rows:
        print("Нет данных для записи.")
        return

    # Сортировка по времени
    data_rows.sort(key=lambda x: int(x['timestamp']))

    # Запись в CSV (utf-8-sig нужен для корректного открытия в Excel)
    fieldnames = ['timestamp', 'type', 'pid', 'state', 'page']
    with open(output_file, 'w', newline='', encoding='utf-8-sig') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        for row in data_rows:
            writer.writerow(row)

    print(f"\nГотово! Все данные собраны в {output_file}")

if __name__ == "__main__":
    parse_logs_and_cleanup()