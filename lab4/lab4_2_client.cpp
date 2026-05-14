#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include <limits>

using namespace std;

HANDLE hPipe = INVALID_HANDLE_VALUE;
bool isConnected = false;
char readBuffer[4096] = {0};
string receivedData;
HANDLE hReadDoneEvent = NULL;

VOID CALLBACK ReadCompletionRoutine(
    DWORD dwErrorCode,
    DWORD dwNumberOfBytesTransfered,
    LPOVERLAPPED lpOverlapped
) {
    if (dwErrorCode == ERROR_SUCCESS) {
        receivedData.assign(readBuffer, dwNumberOfBytesTransfered);
    } else {
        receivedData = "[READ ERROR: " + to_string(dwErrorCode) + "]";
    }
    SetEvent(hReadDoneEvent);
}

void PrintMenu() {
    cout << "\n=== CLIENT MENU ===\n"
         << "1. Connect (CreateFile)\n"
         << "2. Read Data (Async ReadFileEx)\n"
         << "3. Disconnect\n"
         << "4. Exit\n"
         << "Choice: ";
}

int main() {
    hReadDoneEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    int choice;
    while (true) {
        PrintMenu();
        if (!(cin >> choice)) { cin.clear(); cin.ignore(10000, '\n'); continue; }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        switch (choice) {
            case 1: {
                if (isConnected) { cout << "Already connected.\n"; break; }
                hPipe = CreateFile(
                    L"\\\\.\\pipe\\AsyncLabPipe",
                    GENERIC_READ,
                    0, NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED,
                    NULL
                );
                if (hPipe == INVALID_HANDLE_VALUE) {
                    cout << "CreateFile failed: " << GetLastError() << "\n"; break;
                }
                
                // Явно устанавливаем режим чтения в байтовый (важно для ReadFileEx)
                DWORD mode = PIPE_READMODE_BYTE;
                SetNamedPipeHandleState(hPipe, &mode, NULL, NULL);
                
                isConnected = true;
                cout << "Connected.\n"; break;
            }
            case 2: {
                if (!isConnected) { cout << "Not connected.\n"; break; }
                ResetEvent(hReadDoneEvent); 
                receivedData.clear();
                memset(readBuffer, 0, sizeof(readBuffer));

                OVERLAPPED ov = {0}; // hEvent обязан быть 0 для ReadFileEx
                BOOL res = ReadFileEx(hPipe, readBuffer, sizeof(readBuffer) - 1, &ov, ReadCompletionRoutine);
                DWORD err = GetLastError();

                if (!res && err != ERROR_IO_PENDING) {
                    cout << "ReadFileEx failed immediately. Code: " << err << "\n";
                    break;
                }

                cout << "Waiting for async completion...\n";
                
                // Ждём сигнал события, корректно обрабатывая возврат 192 (WAIT_IO_COMPLETION)
                DWORD waitRes = WaitForSingleObjectEx(hReadDoneEvent, 5000, TRUE);
                while (waitRes == WAIT_IO_COMPLETION) {
                    waitRes = WaitForSingleObjectEx(hReadDoneEvent, 5000, TRUE);
                }

                if (waitRes == WAIT_OBJECT_0) {
                    cout << "[RECEIVED]: " << receivedData << "\n";
                } else if (waitRes == WAIT_TIMEOUT) {
                    cout << "[TIMEOUT] 5s passed, no data.\n";
                } else {
                    cout << "[WAIT FAILED] Code: " << waitRes << "\n";
                }
                break;
            }
            case 3: {
                if (!isConnected) { cout << "Not connected.\n"; break; }
                CloseHandle(hPipe); hPipe = INVALID_HANDLE_VALUE;
                isConnected = false; cout << "Disconnected.\n"; break;
            }
            case 4: {
                if (hPipe != INVALID_HANDLE_VALUE) CloseHandle(hPipe);
                CloseHandle(hReadDoneEvent); return 0;
            }
            default: cout << "Invalid.\n";
        }
    }
}