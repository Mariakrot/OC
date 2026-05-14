#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include <limits>

using namespace std;

HANDLE hPipe = INVALID_HANDLE_VALUE;
HANDLE hEvent = NULL;
bool isPipeCreated = false;
bool isConnected = false;

void PrintMenu() {
    cout << "\n=== SERVER MENU ===\n"
         << "1. Create Named Pipe & Event\n"
         << "2. Connect Client (ConnectNamedPipe)\n"
         << "3. Send Data (Async WriteFile)\n"
         << "4. Disconnect (DisconnectNamedPipe)\n"
         << "5. Exit\n"
         << "Choice: ";
}

int main() {
    int choice;
    while (true) {
        PrintMenu();
        if (!(cin >> choice)) { cin.clear(); cin.ignore(10000, '\n'); continue; }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        switch (choice) {
            case 1: {
                if (isPipeCreated) { cout << "Already created.\n"; break; }
                
                hPipe = CreateNamedPipe(
                    L"\\\\.\\pipe\\AsyncLabPipe",
                    PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
                    PIPE_TYPE_BYTE | PIPE_WAIT,
                    1, 4096, 4096, 0, NULL
                );
                if (hPipe == INVALID_HANDLE_VALUE) {
                    cout << "CreateNamedPipe failed: " << GetLastError() << "\n"; break;
                }

                hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
                if (hEvent == NULL) {
                    cout << "CreateEvent failed: " << GetLastError() << "\n";
                    CloseHandle(hPipe); hPipe = INVALID_HANDLE_VALUE; break;
                }
                isPipeCreated = true;
                cout << "Pipe & Event created.\n"; break;
            }
            case 2: {
                if (!isPipeCreated || isConnected) { cout << "Check state.\n"; break; }
                OVERLAPPED ov = {0}; ov.hEvent = hEvent; ResetEvent(hEvent);
                
                cout << "Waiting for client...\n";
                if (!ConnectNamedPipe(hPipe, &ov)) {
                    DWORD err = GetLastError();
                    if (err == ERROR_PIPE_CONNECTED) { isConnected = true; }
                    else if (err == ERROR_IO_PENDING) {
                        WaitForSingleObject(hEvent, INFINITE);
                        isConnected = true;
                    } else {
                        cout << "Connect failed: " << err << "\n"; break;
                    }
                } else isConnected = true;
                
                if (isConnected) cout << "Client connected.\n";
                break;
            }
            case 3: {
                if (!isConnected) { cout << "Not connected.\n"; break; }
                cout << "Enter message: "; string msg; getline(cin, msg);
                if (msg.empty()) break;

                OVERLAPPED ov = {0}; ov.hEvent = hEvent; ResetEvent(hEvent);
                if (!WriteFile(hPipe, msg.c_str(), (DWORD)msg.size(), NULL, &ov)) {
                    if (GetLastError() != ERROR_IO_PENDING) {
                        cout << "Write failed: " << GetLastError() << "\n"; break;
                    }
                }
                WaitForSingleObject(hEvent, INFINITE);
                DWORD bytes = 0;
                GetOverlappedResult(hPipe, &ov, &bytes, FALSE);
                cout << "Sent: " << bytes << " bytes\n";
                break;
            }
            case 4: {
                if (!isConnected) { cout << "Not connected.\n"; break; }
                DisconnectNamedPipe(hPipe); isConnected = false;
                cout << "Disconnected.\n"; break;
            }
            case 5: {
                if (isPipeCreated) { CloseHandle(hPipe); CloseHandle(hEvent); }
                return 0;
            }
            default: cout << "Invalid.\n";
        }
    }
}