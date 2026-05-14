#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <string>
#include <cmath>
#include <iomanip>

using namespace std;

// Константы
const long long N = 100000000LL;
const long long BLOCK_SIZE = 10 * 431521LL;

// Структура для синхронизации доступа к общим данным
struct SharedContext {
    CRITICAL_SECTION csSum;      // Защита глобальной суммы
    CRITICAL_SECTION csBlock;    // Защита индекса блока
    double globalSum;
    long long nextBlockIndex;
    long long totalBlocks;

};

// Структура параметров для каждого потока
struct ThreadInfo {
    HANDLE hThread;
    HANDLE hEvent;
    long long blockStart;
    long long blockSize;
    BOOL exitFlag;
    SharedContext* ctx;
    // Используем атомарную переменную для синхронизации состояния
    volatile LONG isWaiting;
};

// Процедура потока
DWORD WINAPI ThreadProc(LPVOID lpParam) {
    ThreadInfo* info = (ThreadInfo*)lpParam;
    SharedContext* ctx = info->ctx;

    while (true) {
        // Проверка на выход до расчетов
        if (info->exitFlag) break;

        double localSum = 0.0;
        long long end = info->blockStart + info->blockSize;
        if (end > N) end = N;

        for (long long k = info->blockStart; k < end; ++k) {
            double x = (k + 0.5) / (double)N;
            localSum += 4.0 / (1.0 + x * x);
        }
        localSum /= (double)N;

        EnterCriticalSection(&ctx->csSum);
        ctx->globalSum += localSum;
        LeaveCriticalSection(&ctx->csSum);

        // Указываем, что мы уснем
        InterlockedExchange(&info->isWaiting, 1);
        
        // Сигнализируем главному потоку
        SetEvent(info->hEvent);

        // Засыпаем
        SuspendThread(GetCurrentThread());
        
        // После просыпания сбрасываем флаг (это сделает главный поток, но тут на всякий случай)
        InterlockedExchange(&info->isWaiting, 0);
    }
    return 0;
}

double RunSingleCalculation(int numThreads, double& outTimeMs) {
    SharedContext ctx;
    InitializeCriticalSection(&ctx.csSum);
    InitializeCriticalSection(&ctx.csBlock);
    ctx.globalSum = 0.0;
    ctx.nextBlockIndex = 0;
    ctx.totalBlocks = (N + BLOCK_SIZE - 1) / BLOCK_SIZE;

    int actualThreads = max(1, min(numThreads, (int)ctx.totalBlocks));
    ThreadInfo* threads = new ThreadInfo[actualThreads];

    // 1. Создание и первичная настройка
    for (int i = 0; i < actualThreads; ++i) {
        threads[i].hThread = CreateThread(NULL, 0, ThreadProc, &threads[i], CREATE_SUSPENDED, NULL);
        threads[i].hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        threads[i].exitFlag = FALSE;
        threads[i].ctx = &ctx;
        threads[i].isWaiting = 0; // Инициализируем флаг
        threads[i].blockStart = i * BLOCK_SIZE;
        threads[i].blockSize = min<long long>(BLOCK_SIZE, N - threads[i].blockStart);
    }
    ctx.nextBlockIndex = actualThreads;

    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    // 2. Запуск
    for (int i = 0; i < actualThreads; ++i) {
        ResumeThread(threads[i].hThread);
    }

    HANDLE* hEvents = new HANDLE[actualThreads];
    for (int i = 0; i < actualThreads; ++i) hEvents[i] = threads[i].hEvent;

    int activeThreads = actualThreads;

    while (activeThreads > 0) {
        DWORD waitRes = WaitForMultipleObjects(actualThreads, hEvents, FALSE, 5000);
        
        if (waitRes == WAIT_TIMEOUT || waitRes == WAIT_FAILED) break;

        int idx = waitRes - WAIT_OBJECT_0;

        // Мы проверяем флаг isWaiting. Если он 1, значит поток готов уснуть.
        // Мы сбрасываем его в 0 и идем дальше.
        while (InterlockedCompareExchange(&threads[idx].isWaiting, 0, 1) == 0) {
            SwitchToThread(); 
        }

        // Назначаем новые данные
        if (ctx.nextBlockIndex < ctx.totalBlocks) {
            threads[idx].blockStart = ctx.nextBlockIndex * BLOCK_SIZE;
            threads[idx].blockSize = min<long long>(BLOCK_SIZE, N - threads[idx].blockStart);
            ctx.nextBlockIndex++;
        } else {
            threads[idx].exitFlag = TRUE;
            activeThreads--;
        }

        // ResumeThread возвращает 0, если поток еще не уснул. 
        // Поэтому крутим цикл, пока она не вернет 1 (успешное пробуждение).
        while (ResumeThread(threads[idx].hThread) == 0) {
            SwitchToThread();
        }
    }

    // 3. Завершение
    HANDLE* hThreads = new HANDLE[actualThreads];
    for (int i = 0; i < actualThreads; ++i) hThreads[i] = threads[i].hThread;
    WaitForMultipleObjects(actualThreads, hThreads, TRUE, INFINITE);

    QueryPerformanceCounter(&end);
    outTimeMs = (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart;

    // Очистка дескрипторов
    for (int i = 0; i < actualThreads; ++i) {
        CloseHandle(threads[i].hThread);
        CloseHandle(threads[i].hEvent);
    }
    DeleteCriticalSection(&ctx.csSum);
    DeleteCriticalSection(&ctx.csBlock);
    delete[] threads;
    delete[] hEvents;
    delete[] hThreads;

    return ctx.globalSum;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <output.csv> <thread_count>" << endl;
        cerr << "Example: " << argv[0] << " results.csv 8" << endl;
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

    const int RUNS = 5;
    double totalTime = 0.0;
    double lastPi = 0.0;

    cout << "=== Benchmark: " << numThreads << " thread(s) ===" << endl;
    cout << "N = " << N << ", Block size = " << BLOCK_SIZE << endl;
    cout << "Executing " << RUNS << " runs to calculate average time...\n" << endl;

    for (int run = 1; run <= RUNS; ++run) {
        double runTime = 0.0;
        lastPi = RunSingleCalculation(numThreads, runTime);
        totalTime += runTime;
        cout << "Run " << run << "/" << RUNS << " completed. Time: " 
             << fixed << setprecision(2) << runTime << " ms" << endl;
    }

    double avgTime = totalTime / RUNS;
    const double REFERENCE_PI = 3.14159265358979323846;
    double error = abs(lastPi - REFERENCE_PI);

    ofstream csv(csvFilename, ios::app);
    if (!csv.is_open()) {
        cerr << "Error: Cannot open CSV file for writing: " << csvFilename << endl;
        system("pause");
        return 1;
    }

    csv.seekp(0, ios::end);
    if (csv.tellp() == 0) {
        csv << "threads,avg_time_ms,computed_pi,error_vs_reference" << endl;
    }
    csv << numThreads << "," 
        << fixed << setprecision(3) << avgTime << "," 
        << setprecision(10) << lastPi << "," 
        << setprecision(12) << error << endl;
    csv.close();

    cout << "\n=== Results ===" << endl;
    cout << "Average time: " << fixed << setprecision(2) << avgTime << " ms" << endl;
    cout << "Computed Pi:  " << setprecision(10) << lastPi << endl;
    cout << "Error:        " << setprecision(12) << error << endl;
    cout << "\nData appended to: " << csvFilename << endl;

    return 0;
}