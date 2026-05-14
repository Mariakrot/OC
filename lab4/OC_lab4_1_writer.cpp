#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <mmsystem.h>

const int DATA_PAGES = 12; 
const int TOTAL_PAGES = DATA_PAGES + 1; // +1 для метаданных (счетчика)

const char* MAPPING_NAME      = "RW_SharedMem";
const char* MUTEX_WRITER_NAME = "RW_WriterMutex";
const int ITERATIONS = 10;

void LogState(std::ofstream& log, const std::string& state, int page = -1) {
    DWORD t = timeGetTime();
    std::string logMsg = std::to_string(t) + " | " + state;
    if (page >= 0) logMsg += " | PAGE " + std::to_string(page);
    
    log << logMsg << "\n";
    log.flush();
    std::cout << "[W] " << logMsg << "\n";
}

int main() {
    std::cout << "=== Writer PID: " << GetCurrentProcessId() << " ===\n";
    srand(GetCurrentProcessId() + GetTickCount());

    std::string logFile = "writer_" + std::to_string(GetCurrentProcessId()) + ".log";
    std::ofstream ofs(logFile);

    HANDLE hWriterSem = CreateSemaphore(nullptr, 1, 1, "RW_WriterLock");

    SYSTEM_INFO si; GetSystemInfo(&si);
    DWORD pageSize = si.dwPageSize;
    SIZE_T totalSize = (SIZE_T)TOTAL_PAGES * pageSize;

    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD)totalSize, MAPPING_NAME);
    LPVOID pView = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, totalSize);

    // Блокируем в ОЗУ
    VirtualLock(pView, totalSize);

    for (int i = 0; i < ITERATIONS; ++i) {
        int pageIdx = (rand() % DATA_PAGES) + 1; // Страницы с 1 по 12

        LogState(ofs, "BEGINING OF WAITING");
        WaitForSingleObject(hWriterSem, INFINITE);
        LogState(ofs, "RECORD", pageIdx);
        char* data = (char*)pView + (pageIdx * pageSize);
        data[0] = 'W'; 

        Sleep(500 + rand() % 1001);

        LogState(ofs, "TRANSITION TO LIBERATION");
        ReleaseSemaphore(hWriterSem, 1, nullptr);

        Sleep(500 + rand() % 1001); // Пауза между итерациями
    }

    VirtualUnlock(pView, totalSize);
    UnmapViewOfFile(pView);
    CloseHandle(hMap);
    CloseHandle(hWriterSem);
    return 0;
}