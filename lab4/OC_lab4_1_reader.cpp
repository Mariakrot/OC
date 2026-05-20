#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

const int DATA_PAGES = 12;
const int TOTAL_PAGES = DATA_PAGES + 1;
const char EMPTY = 0;
const char HAS_DATA = 1;

const char* MAPPING_NAME      = "RW_SharedMem";
const char* MUTEX_WRITER_NAME = "RW_WriterMutex";
const char* MUTEX_READER_NAME = "RW_ReaderCountMutex";
const char* MUTEX_META_NAME   = "RW_MetaMutex"; // Новый мьютекс для метаданных
const int ITERATIONS = 10;

void LogState(std::ofstream& log, const std::string& state, int page = -1) {
    DWORD t = timeGetTime();
    std::string logMsg = std::to_string(t) + " | " + state;
    if (page >= 0) logMsg += " | PAGE " + std::to_string(page);
    log << logMsg << "\n"; log.flush();
    std::cout << "[R] " << logMsg << "\n";
}

int main() {
    std::cout << "=== Reader PID: " << GetCurrentProcessId() << " ===\n";
    srand(GetCurrentProcessId() + GetTickCount());

    std::string logFile = "reader_" + std::to_string(GetCurrentProcessId()) + ".log";
    std::ofstream ofs(logFile);

    HANDLE hWriterSem = CreateSemaphore(nullptr, 1, 1, "RW_WriterLock");
    HANDLE hReaderMutex = CreateMutex(nullptr, FALSE, MUTEX_READER_NAME);
    // Мьютекс для защиты массива состояний страниц
    HANDLE hMetaMutex = CreateMutex(nullptr, FALSE, MUTEX_META_NAME); 

    SYSTEM_INFO si; GetSystemInfo(&si);
    DWORD pageSize = si.dwPageSize;
    SIZE_T totalSize = (SIZE_T)TOTAL_PAGES * pageSize;

    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD)totalSize, MAPPING_NAME);
    LPVOID pView = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, totalSize);
    
    volatile LONG* pReaderCount = (volatile LONG*)pView;
    // Указатель на массив состояний (смещение 4 байта от начала)
    volatile char* pPageState = (volatile char*)((char*)pView + 4); 

    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        *pReaderCount = 0;
        // Инициализируем все страницы как пустые
        for(int i=0; i<DATA_PAGES; ++i) pPageState[i] = EMPTY; 
    }

    VirtualLock(pView, totalSize);

    for (int i = 0; i < ITERATIONS; ++i) {
        LogState(ofs, "BEGINNING OF WAITING");

        // --- ВХОД ЧИТАТЕЛЯ (Алгоритм Читатели-Писатели) ---
        WaitForSingleObject(hReaderMutex, INFINITE);
        if (InterlockedIncrement(pReaderCount) == 1) {
            WaitForSingleObject(hWriterSem, INFINITE);
        }
        ReleaseMutex(hReaderMutex);

        // --- ПОИСК СТРАНИЦЫ С ДАННЫМИ ---
        int pageIdx = -1;
        
        // Захватываем мьютекс метаданных для атомарного поиска и захвата страницы
        WaitForSingleObject(hMetaMutex, INFINITE);
        
        for (int k = 0; k < DATA_PAGES; ++k) {
            if (pPageState[k] == HAS_DATA) {
                pageIdx = k + 1; // Страницы 1..12
                pPageState[k] = EMPTY; // Сразу помечаем как "прочитано/захвачено"
                break;
            }
        }
        // Освобождаем мьютекс метаданных, но страницу мы уже "забронировали"
        ReleaseMutex(hMetaMutex);

        if (pageIdx != -1) {
            // --- ЧТЕНИЕ ---
            LogState(ofs, "READING", pageIdx);
            // Читаем данные. Мы в безопасности, т.к. пометили страницу как EMPTY, 
            // и писатель теперь не может в неё писать, пока мы читаем.
            char val = ((char*)pView + (pageIdx * pageSize))[0]; 
            std::cout << "   [R] Read val: " << val << "\n";
            Sleep(500 + rand() % 1001); 
        } else {
            LogState(ofs, "NO DATA AVAILABLE, SKIP");
            Sleep(200); // Короткая пауза, если данных нет
        }

        // --- ВЫХОД ЧИТАТЕЛЯ ---
        LogState(ofs, "TRANSITION TO LIBERATION");
        WaitForSingleObject(hReaderMutex, INFINITE);
        if (InterlockedDecrement(pReaderCount) == 0) {
            ReleaseSemaphore(hWriterSem, 1, nullptr);
        }
        ReleaseMutex(hReaderMutex);

        Sleep(500 + rand() % 1001); 
    }

    VirtualUnlock(pView, totalSize);
    UnmapViewOfFile(pView);
    CloseHandle(hMap);
    CloseHandle(hWriterSem);
    CloseHandle(hReaderMutex);
    CloseHandle(hMetaMutex);
    return 0;
}