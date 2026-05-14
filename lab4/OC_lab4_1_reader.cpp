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

const char* MAPPING_NAME      = "RW_SharedMem";
const char* MUTEX_WRITER_NAME = "RW_WriterMutex";
const char* MUTEX_READER_NAME = "RW_ReaderCountMutex";
const int ITERATIONS = 20;

void LogState(std::ofstream& log, const std::string& state, int page = -1) {
    DWORD t = timeGetTime();
    std::string logMsg = std::to_string(t) + " | " + state;
    if (page >= 0) logMsg += " | PAGE " + std::to_string(page);
    
    log << logMsg << "\n";
    log.flush();
    std::cout << "[R] " << logMsg << "\n";
}

int main() {
    std::cout << "=== Reader PID: " << GetCurrentProcessId() << " ===\n";
    srand(GetCurrentProcessId() + GetTickCount());

    std::string logFile = "reader_" + std::to_string(GetCurrentProcessId()) + ".log";
    std::ofstream ofs(logFile);

    HANDLE hWriterSem = CreateSemaphore(nullptr, 1, 1, "RW_WriterLock");
    HANDLE hReaderMutex = CreateMutex(nullptr, FALSE, MUTEX_READER_NAME);

    SYSTEM_INFO si; GetSystemInfo(&si);
    DWORD pageSize = si.dwPageSize;
    SIZE_T totalSize = (SIZE_T)TOTAL_PAGES * pageSize;

    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD)totalSize, MAPPING_NAME);
    LPVOID pView = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, totalSize);
    
    volatile LONG* pReaderCount = (volatile LONG*)pView;

    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        *pReaderCount = 0;
    }

    VirtualLock(pView, totalSize);

    for (int i = 0; i < ITERATIONS; ++i) {
        int pageIdx = (rand() % DATA_PAGES) + 1;
        LogState(ofs, "BEGINNING OF WAITING");

        // вход читателя
        DWORD waitReader = WaitForSingleObject(hReaderMutex, INFINITE);
        
        if (InterlockedIncrement(pReaderCount) == 1) {
            WaitForSingleObject(hWriterSem, INFINITE);
        }
        ReleaseMutex(hReaderMutex);

        // чтение
        LogState(ofs, "READING", pageIdx);
        char val = ((char*)pView + (pageIdx * pageSize))[0]; 
        Sleep(500 + rand() % 1001); 

        // выход читателя
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
    return 0;
}