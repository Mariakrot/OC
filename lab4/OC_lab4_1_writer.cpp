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
const char* MUTEX_META_NAME   = "RW_MetaMutex"; // Тот же мьютекс для метаданных
const int ITERATIONS = 10;

void LogState(std::ofstream& log, const std::string& state, int page = -1) {
    DWORD t = timeGetTime();
    std::string logMsg = std::to_string(t) + " | " + state;
    if (page >= 0) logMsg += " | PAGE " + std::to_string(page);
    log << logMsg << "\n"; log.flush();
    std::cout << "[W] " << logMsg << "\n";
}

int main() {
    std::cout << "=== Writer PID: " << GetCurrentProcessId() << " ===\n";
    srand(GetCurrentProcessId() + GetTickCount());

    std::string logFile = "writer_" + std::to_string(GetCurrentProcessId()) + ".log";
    std::ofstream ofs(logFile);

    HANDLE hWriterSem = CreateSemaphore(nullptr, 1, 1, "RW_WriterLock");
    HANDLE hMetaMutex = CreateMutex(nullptr, FALSE, MUTEX_META_NAME);

    SYSTEM_INFO si; GetSystemInfo(&si);
    DWORD pageSize = si.dwPageSize;
    SIZE_T totalSize = (SIZE_T)TOTAL_PAGES * pageSize;

    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD)totalSize, MAPPING_NAME);
    LPVOID pView = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, totalSize);

    volatile char* pPageState = (volatile char*)((char*)pView + 4);

    VirtualLock(pView, totalSize);

    for (int i = 0; i < ITERATIONS; ++i) {
        LogState(ofs, "BEGINNING OF WAITING");
        
        // Ждем разрешения на запись 
        WaitForSingleObject(hWriterSem, INFINITE);
        
        // поиск свободной страницы
        int pageIdx = -1;
        
        WaitForSingleObject(hMetaMutex, INFINITE);
        for (int k = 0; k < DATA_PAGES; ++k) {
            if (pPageState[k] == EMPTY) {
                pageIdx = k + 1; // Страницы 1..12
                pPageState[k] = HAS_DATA; // Резервируем: теперь здесь будут данные
                break;
            }
        }
        ReleaseMutex(hMetaMutex);

        if (pageIdx != -1) {
            // запись
            LogState(ofs, "RECORD", pageIdx);
            char* data = (char*)pView + (pageIdx * pageSize);
            data[0] = 'W'; // Записываем данные
            std::cout << "   [W] Wrote to page " << pageIdx << "\n";
            
            Sleep(500 + rand() % 1001); // Имитация работы
        } else {
            LogState(ofs, "NO EMPTY PAGES, SKIP");
            Sleep(200);
        }

        LogState(ofs, "TRANSITION TO LIBERATION");
        // Освобождаем семафор писателей
        ReleaseSemaphore(hWriterSem, 1, nullptr);

        Sleep(500 + rand() % 1001); 
    }

    VirtualUnlock(pView, totalSize);
    UnmapViewOfFile(pView);
    CloseHandle(hMap);
    CloseHandle(hWriterSem);
    CloseHandle(hMetaMutex);
    return 0;
}