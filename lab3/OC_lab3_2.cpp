#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <iomanip>
#include <omp.h>        // Подключение OpenMP
#include <windows.h>    // Только для QueryPerformanceCounter (высокая точность таймера)

using namespace std;

// Константы
const long long N = 100000000LL;
const long long BLOCK_SIZE = 10 * 431521LL; // 4 315 210

// Функция вычисления pi с использованием OpenMP
double CalculatePi(int numThreads, double& outTimeMs) {
    double globalSum = 0.0;
    
    // Установка количества потоков OpenMP
    omp_set_num_threads(numThreads);
    
    // Запуск высокоточного таймера
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    
    // Параллельное вычисление с редукцией
    // schedule(dynamic, BLOCK_SIZE) — динамическое планирование блоками
    // reduction(+:globalSum) — автоматическая безопасная сумма по всем потокам
    #pragma omp parallel for schedule(dynamic, BLOCK_SIZE) reduction(+:globalSum)
    for (long long k = 0; k < N; ++k) {
        double x = (k + 0.5) / (double)N;
        globalSum += 4.0 / (1.0 + x * x);
    }
    
    // Применяем множитель 1/N один раз после суммирования
    globalSum /= (double)N;
    
    // Остановка таймера
    QueryPerformanceCounter(&end);
    outTimeMs = (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart;
    
    return globalSum;
}

int main(int argc, char* argv[]) {
    // Проверка аргументов командной строки
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <output.csv> <thread_count>" << endl;
        cerr << "Example: " << argv[0] << " results_omp.csv 8" << endl;
        system("pause");
        return 1;
    }

    string csvFilename = argv[1];
    int numThreads = 0;
    try {
        numThreads = stoi(argv[2]);
        if (numThreads < 1 || numThreads > 64) throw invalid_argument("Thread count must be 1-64");
    } catch (const exception& e) {
        cerr << "Error: Invalid thread count. " << e.what() << endl;
        system("pause");
        return 1;
    }

    const int RUNS = 5; // Количество повторений для усреднения
    double totalTime = 0.0;
    double lastPi = 0.0;

    cout << "=== OpenMP Benchmark: " << numThreads << " thread(s) ===" << endl;
    cout << "N = " << N << ", Block size = " << BLOCK_SIZE << endl;
    cout << "Executing " << RUNS << " runs to calculate average time...\n" << endl;

    // Цикл многократного выполнения для усреднения
    for (int run = 1; run <= RUNS; ++run) {
        double runTime = 0.0;
        lastPi = CalculatePi(numThreads, runTime);
        totalTime += runTime;
        cout << "Run " << run << "/" << RUNS << " completed. Time: " 
             << fixed << setprecision(2) << runTime << " ms" << endl;
    }

    double avgTime = totalTime / RUNS;
    const double REFERENCE_PI = 3.14159265358979323846;
    double error = abs(lastPi - REFERENCE_PI);

    // Запись результатов в CSV
    ofstream csv(csvFilename, ios::app);
    if (!csv.is_open()) {
        cerr << "Error: Cannot open CSV file for writing: " << csvFilename << endl;
        system("pause");
        return 1;
    }

    // Если файл пустой — записываем заголовок
    csv.seekp(0, ios::end);
    if (csv.tellp() == 0) {
        csv << "threads,avg_time_ms,computed_pi,error_vs_reference,api_type" << endl;
    }
    
    // Запись строки результатов (добавляем метку "OpenMP" для сравнения с прошлой работой)
    csv << numThreads << "," 
        << fixed << setprecision(3) << avgTime << "," 
        << setprecision(10) << lastPi << "," 
        << setprecision(12) << error << ","
        << "OpenMP" << endl;
    csv.close();

    // Итоговый вывод
    cout << "\n=== Results ===" << endl;
    cout << "Average time: " << fixed << setprecision(2) << avgTime << " ms" << endl;
    cout << "Computed Pi:  " << setprecision(10) << lastPi << endl;
    cout << "Error:        " << setprecision(12) << error << endl;
    cout << "\nData appended to: " << csvFilename << endl;

    return 0;
}