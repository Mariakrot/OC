#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <limits>
#include <vector>
#include <algorithm>

using namespace std;

struct TrackedRegion {
    void* address;
    SIZE_T requestedSize;
};

std::vector<TrackedRegion> g_Allocations;

void PrintAddress(void* ptr) {
    cout << "0x" << hex << uppercase << reinterpret_cast<uintptr_t>(ptr) << nouppercase << dec;
}

string StateToStr(DWORD state) {
    switch (state) {
        case MEM_COMMIT:  return "MEM_COMMIT (Physical memory allocated)";
        case MEM_RESERVE: return "MEM_RESERVE (Address space reserved)";
        case MEM_FREE:    return "MEM_FREE (Free page)";
        default:          return "UNKNOWN";
    }
}

string TypeToStr(DWORD type) {
    switch (type) {
        case MEM_IMAGE:    return "MEM_IMAGE (Mapped to executable image)";
        case MEM_MAPPED:   return "MEM_MAPPED (Mapped to file)";
        case MEM_PRIVATE:  return "MEM_PRIVATE (Private to process)";
        default:           return "UNKNOWN";
    }
}

string ProtectToStr(DWORD protect) {
    if (protect == PAGE_NOACCESS) return "PAGE_NOACCESS";
    if (protect == PAGE_READONLY) return "PAGE_READONLY";
    if (protect == PAGE_READWRITE) return "PAGE_READWRITE";
    if (protect == PAGE_WRITECOPY) return "PAGE_WRITECOPY";
    if (protect == PAGE_EXECUTE) return "PAGE_EXECUTE";
    if (protect == PAGE_EXECUTE_READ) return "PAGE_EXECUTE_READ";
    if (protect == PAGE_EXECUTE_READWRITE) return "PAGE_EXECUTE_READWRITE";
    if (protect & PAGE_GUARD) return ProtectToStr(protect & ~PAGE_GUARD) + " | PAGE_GUARD";
    return "OTHER (0x" + to_string(protect) + ")";
}

void Wait() {
    cout << "\nPress Enter to continue...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cin.get();
}

void PrintAddressProperties(void* addr) {
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T res = VirtualQuery(addr, &mbi, sizeof(mbi));
    if (res == 0) {
        cout << "VirtualQuery failed. Error: " << GetLastError() << "\n";
        return;
    }

    cout << "\n--- Properties for "; PrintAddress(addr); cout << " ---\n";
    cout << "Base Address:        "; PrintAddress(mbi.BaseAddress); cout << "\n";
    cout << "Allocation Base:     "; PrintAddress(mbi.AllocationBase); cout << "\n";
    cout << "Region Size:         " << mbi.RegionSize << " bytes\n";
    cout << "State:               " << StateToStr(mbi.State) << "\n";
    cout << "Protection:          " << ProtectToStr(mbi.Protect) << "\n";
    cout << "Type:                " << TypeToStr(mbi.Type) << "\n";
}

void GetSystemInfoTask() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    cout << "\n=== System Information (GetSystemInfo) ===\n";
    cout << "Page Size:                          " << si.dwPageSize << " bytes\n";
    cout << "Min Application Address:            "; PrintAddress(si.lpMinimumApplicationAddress); cout << "\n";
    cout << "Max Application Address:            "; PrintAddress(si.lpMaximumApplicationAddress); cout << "\n";
    cout << "Allocation Granularity:             " << si.dwAllocationGranularity << " bytes\n";
    cout << "Number of Processors:               " << si.dwNumberOfProcessors << "\n";
    cout << "Processor Type:                     " << si.dwProcessorType << "\n";
    Wait();
}

void GlobalMemoryStatusTask() {
    MEMORYSTATUS ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);

    cout << "\n=== Memory Status (GlobalMemoryStatus) ===\n";
    cout << "Memory Load:                        " << ms.dwMemoryLoad << "%\n";
    cout << "Total Physical Memory:              " << ms.dwTotalPhys / (1024 * 1024) << " MB\n";
    cout << "Available Physical Memory:          " << ms.dwAvailPhys / (1024 * 1024) << " MB\n";
    cout << "Total Virtual Memory:               " << ms.dwTotalVirtual / (1024 * 1024) << " MB\n";
    cout << "Available Virtual Memory:           " << ms.dwAvailVirtual / (1024 * 1024) << " MB\n";
    cout << "(Note: GlobalMemoryStatus is limited to 4GB. Use GlobalMemoryStatusEx for larger systems.)\n";
    Wait();
}

void VirtualQueryTask() {
    void* addr;
    cout << "\n=== Memory Region State (VirtualQuery) ===\n";
    cout << "Enter address to query (hex, e.g. 0x10000): ";
    cin >> hex >> addr;
    PrintAddressProperties(addr);
    Wait();
}

void ReserveCommitSeparateTask() {
    cout << "\n=== Separate Reserve & Commit (VirtualAlloc/Free) ===\n";
    
    int mode;
    cout << "Mode: 1 - Automatic address, 2 - Specify start address: ";
    cin >> mode;

    void* baseAddr = nullptr;
    if (mode == 2) {
        SYSTEM_INFO si; GetSystemInfo(&si);
        cout << "Note: Address must be a multiple of " << si.dwAllocationGranularity << " bytes.\n";
        cout << "Enter start address (hex): ";
        cin >> hex >> baseAddr;
    }

    SIZE_T size;
    cout << "Region size (bytes): "; cin >> dec >> size;

    cout << "\n[1] Reserving address space (MEM_RESERVE)...\n";
    void* reserved = VirtualAlloc(baseAddr, size, MEM_RESERVE, PAGE_NOACCESS);
    if (!reserved) { cout << "Reservation failed. Error: " << GetLastError() << "\n"; Wait(); return; }
    cout << "Reserved at: "; PrintAddress(reserved); cout << "\n";

    cout << "[2] Committing physical memory (MEM_COMMIT)...\n";
    void* committed = VirtualAlloc(reserved, size, MEM_COMMIT, PAGE_READWRITE);
    if (!committed) { 
        cout << "Commit failed. Error: " << GetLastError() << "\n"; 
        VirtualFree(reserved, 0, MEM_RELEASE); 
        Wait(); return; 
    }
    cout << "Committed at: "; PrintAddress(committed); cout << "\n";

    g_Allocations.push_back({committed, size});
    cout << "Memory allocated successfully. Track this address in the list.\n";
    Wait();
}

void ReserveCommitTogetherTask() {
    cout << "\n=== Combined Reserve & Commit (VirtualAlloc/Free) ===\n";
    
    int mode;
    cout << "Mode: 1 - Automatic address, 2 - Specify start address: ";
    cin >> mode;

    void* baseAddr = nullptr;
    if (mode == 2) {
        SYSTEM_INFO si; GetSystemInfo(&si);
        cout << "Note: Address must be a multiple of " << si.dwAllocationGranularity << " bytes.\n";
        cout << "Enter start address (hex): ";
        cin >> hex >> baseAddr;
    }

    SIZE_T size;
    cout << "Region size (bytes): "; cin >> dec >> size;

    cout << "\nReserving & committing (MEM_RESERVE | MEM_COMMIT)...\n";
    void* allocated = VirtualAlloc(baseAddr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!allocated) { cout << "Allocation failed. Error: " << GetLastError() << "\n"; Wait(); return; }
    cout << "Allocated at: "; PrintAddress(allocated); cout << "\n";

    g_Allocations.push_back({allocated, size});
    cout << "Memory allocated successfully. Track this address in the list.\n";
    Wait();
}

void ReadMemoryTask() {
    cout << "\n=== Read Data from Memory ===\n";
    void* addr;
    cout << "Address (hex): "; cin >> hex >> addr;

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) {
        cout << "VirtualQuery failed. Error: " << GetLastError() << "\n";
        Wait(); return;
    }

    if (mbi.State != MEM_COMMIT) {
        cout << "Error: Memory region is not committed.\n";
    } else if (mbi.Protect == PAGE_NOACCESS) {
        cout << "Error: Memory access is denied (PAGE_NOACCESS).\n";
    } else {
        int* p = reinterpret_cast<int*>(addr);
        int val = *p;
        cout << "Read successful. Value: " << val << " (0x" << hex << val << dec << ")\n";
    }
    Wait();
}

void WriteMemoryTask() {
    cout << "\n=== Write Data to Memory ===\n";
    cout << "Note: Address must point to a committed, writable memory region.\n";
    
    void* addr;
    int value;
    cout << "Address (hex): "; cin >> hex >> addr;
    cout << "Integer value to write (dec): "; cin >> dec >> value;

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) {
        cout << "VirtualQuery failed. Error: " << GetLastError() << "\n";
        Wait(); return;
    }

    if (mbi.State != MEM_COMMIT) {
        cout << "Error: Memory region is not committed.\n";
    } else if (mbi.Protect == PAGE_NOACCESS) {
        cout << "Error: Memory access is denied (PAGE_NOACCESS).\n";
    } else if (mbi.Protect == PAGE_READONLY) {
        cout << "Error: Memory is read-only (PAGE_READONLY).\n";
        cout << "Change protection to PAGE_READWRITE using option 8 first.\n";
    } else {
        int* p = reinterpret_cast<int*>(addr);
        *p = value;
        cout << "Write successful.\n";
    }
    Wait();
}

void SetProtectionTask() {
    cout << "\n=== Set Memory Protection (VirtualProtect) ===\n";
    
    void* addr;
    SIZE_T size;
    DWORD newProtect, oldProtect;
    int choice;

    cout << "Region address (hex): "; cin >> hex >> addr;
    cout << "Region size (bytes): "; cin >> dec >> size;
    
    cout << "Select new protection:\n";
    cout << " 1 - PAGE_READWRITE\n 2 - PAGE_READONLY\n 3 - PAGE_NOACCESS\n 4 - PAGE_EXECUTE_READWRITE\n";
    cout << "Your choice: "; cin >> choice;

    switch (choice) {
        case 1: newProtect = PAGE_READWRITE; break;
        case 2: newProtect = PAGE_READONLY; break;
        case 3: newProtect = PAGE_NOACCESS; break;
        case 4: newProtect = PAGE_EXECUTE_READWRITE; break;
        default: cout << "Invalid choice.\n"; Wait(); return;
    }

    cout << "\nApplying protection...\n";
    if (VirtualProtect(addr, size, newProtect, &oldProtect)) {
        cout << "Protection changed successfully.\n";
        cout << "Previous protection: " << ProtectToStr(oldProtect) << "\n";
        cout << "New protection:      " << ProtectToStr(newProtect) << "\n";

        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(addr, &mbi, sizeof(mbi))) {
            cout << "Verified via VirtualQuery: " << ProtectToStr(mbi.Protect) << "\n";
        }
    } else {
        cout << "VirtualProtect failed. Error: " << GetLastError() << "\n";
    }
    Wait();
}

void FreeMemoryTask() {
    cout << "\n=== Free Allocated Memory (VirtualFree) ===\n";
    int mode;
    cout << "1 - Free single address\n2 - Free all tracked allocations\nYour choice: ";
    cin >> mode;

    if (mode == 2) {
        if (g_Allocations.empty()) {
            cout << "No tracked allocations to free.\n";
        } else {
            cout << "Freeing " << g_Allocations.size() << " region(s)...\n";
            for (const auto& region : g_Allocations) {
                VirtualFree(region.address, 0, MEM_RELEASE);
            }
            g_Allocations.clear();
            cout << "All tracked memory freed successfully.\n";
        }
    } else {
        void* addr;
        cout << "Enter address to free (hex): "; cin >> hex >> addr;

        auto it = std::find_if(g_Allocations.begin(), g_Allocations.end(),
            [addr](const TrackedRegion& r) { return r.address == addr; });
        if (it != g_Allocations.end()) {
            g_Allocations.erase(it);
        }

        if (VirtualFree(addr, 0, MEM_RELEASE)) {
            cout << "Memory released successfully.\n";
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_INVALID_ADDRESS)
                cout << "Error: Invalid address or region is already free (ERROR_INVALID_ADDRESS).\n";
            else
                cout << "VirtualFree failed. Error: " << err << "\n";
        }
    }
    Wait();
}

void ListAllocationsTask() {
    cout << "\n=== Tracked Allocated Regions ===\n";
    if (g_Allocations.empty()) {
        cout << "No tracked allocations.\n";
    } else {
        cout << "Index | Address          | Requested Size | Actual State\n";
        cout << "------|------------------|----------------|---------------------------\n";
        for (size_t i = 0; i < g_Allocations.size(); ++i) {
            cout << "  " << i << "   | ";
            PrintAddress(g_Allocations[i].address);
            cout << " | " << setw(14) << g_Allocations[i].requestedSize << " bytes | ";
            
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(g_Allocations[i].address, &mbi, sizeof(mbi))) {
                cout << StateToStr(mbi.State);
            } else {
                cout << "UNKNOWN/INVALID";
            }
            cout << "\n";
        }
    }
    Wait();
}

void ShowMenu() {
    cout << "========================================\n";
    cout << "  LAB 2: Virtual Memory Management\n";
    cout << "========================================\n";
    cout << "1. Get System Information (GetSystemInfo)\n";
    cout << "2. Memory Status (GlobalMemoryStatus)\n";
    cout << "3. Address propeties(VirtualQuery)\n";
    cout << "4. Separate Reserve & Commit\n";
    cout << "5. Combined Reserve & Commit\n";
    cout << "6. Read Data from Memory\n";
    cout << "7. Write Data to Memory\n";
    cout << "8. Set & Verify Protection (VirtualProtect)\n";
    cout << "9. Free Allocated Memory\n";
    cout << "10. List Allocated Regions\n";
    cout << "0. Exit\n";
    cout << "========================================\n";
    cout << "Select option: ";
}

int main() {
    int choice;
    do {
        ShowMenu();
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        switch (choice) {
            case 1: GetSystemInfoTask(); break;
            case 2: GlobalMemoryStatusTask(); break;
            case 3: VirtualQueryTask(); break;
            case 4: ReserveCommitSeparateTask(); break;
            case 5: ReserveCommitTogetherTask(); break;
            case 6: ReadMemoryTask(); break;
            case 7: WriteMemoryTask(); break;
            case 8: SetProtectionTask(); break;
            case 9: FreeMemoryTask(); break;
            case 10: ListAllocationsTask(); break;
            case 0: cout << "Exiting.\n"; break;
            default: cout << "Invalid option.\n"; Wait(); break;
        }
    } while (choice != 0);

    // Cleanup: free all tracked allocations before exit
    for (const auto& region : g_Allocations) {
        VirtualFree(region.address, 0, MEM_RELEASE);
    }
    g_Allocations.clear();

    return 0;
}