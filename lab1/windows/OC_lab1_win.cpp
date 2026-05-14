#define _WIN32_WINNT 0x0600
#include <iostream>
#include <string>
#include <windows.h>
#include <iomanip>
#include <cstdio>

// Вспомогательная функция для вывода ошибки
void printLastError(const std::string& context) {
    DWORD err = GetLastError();
    std::cout << "[Error] " << context << ". Error code: " << err << std::endl;
    // Сброс флага ошибки, чтобы не влиял на следующие операции
    SetLastError(0);
}

// Очистка буфера ввода
void clearInput() {
    std::cin.clear();
    std::cin.ignore(32600, '\n');
}

// Ввод строки с пробелами
std::string getInputString(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

// 1. Вывод списка дисков
void listDrives() {
    std::cout << "\n--- List of Logical Drives ---" << std::endl;
    
    // GetLogicalDriveStrings возвращает строки, разделенные нулями, в конце два нуля
    char buffer[1024] = { 0 };
    DWORD result = GetLogicalDriveStringsA(sizeof(buffer), buffer);

    if (result == 0) {
        printLastError("GetLogicalDriveStrings");
        return;
    }

    // Проверка битовой маски через GetLogicalDrives
    DWORD mask = GetLogicalDrives();
    std::cout << "Drive bit mask (GetLogicalDrives): " << std::hex << mask << std::dec << std::endl;

    char* drive = buffer;
    int count = 0;
    while (*drive) {
        std::cout << "Drive " << (count + 1) << ": " << drive << std::endl;
        drive += strlen(drive) + 1; // Переход к следующей строке через нулевой байт
        count++;
    }
    if (count == 0) std::cout << "No drives found." << std::endl;
}

// 2. Информация о диске и свободное место
void getDriveInfo() {
    std::cout << "\n--- Drive Information ---" << std::endl;
    std::string rootPath = getInputString("Enter drive path (e.g., C:\\): ");
    
    if (rootPath.empty()) return;

    // ========================================================================
    // 1. Тип диска
    // ========================================================================
    UINT type = GetDriveTypeA(rootPath.c_str());
    std::string typeStr;
    switch (type) {
        case DRIVE_REMOVABLE: typeStr = "Removable"; break;
        case DRIVE_FIXED:     typeStr = "Fixed"; break;
        case DRIVE_REMOTE:    typeStr = "Remote"; break;
        case DRIVE_CDROM:     typeStr = "CD/DVD"; break;
        case DRIVE_RAMDISK:   typeStr = "RAM Disk"; break;
        default:              typeStr = "Unknown"; break;
    }
    std::cout << "Drive Type: " << typeStr << std::endl;

    // ========================================================================
    // 2. Информация о томе (метка, ФС, серийный номер, флаги возможностей)
    // ========================================================================
    char volName[MAX_PATH] = {0};
    char fsName[MAX_PATH] = {0};
    DWORD serial = 0;
    DWORD maxCompLen = 0;
    DWORD fsFlags = 0;  // Сюда запишутся флаги атрибутов тома

    if (GetVolumeInformationA(rootPath.c_str(), volName, MAX_PATH, &serial, &maxCompLen, &fsFlags, fsName, MAX_PATH)) {
        std::cout << "Volume Label: " << volName << std::endl;
        std::cout << "File System: " << fsName << std::endl;
        std::cout << "Serial Number: 0x" << std::hex << serial << std::dec << std::endl;
        
        // === Вывод атрибутов/возможностей тома ===
        std::cout << "\nVolume Capabilities:" << std::endl;
        
        struct FlagInfo {
            DWORD flag;
            const char* name;
        };
        
        // Используем #ifdef для совместимости с разными версиями Windows SDK
        FlagInfo flags[] = {
#ifdef FILE_CASE_SENSITIVE_SEARCH
            {FILE_CASE_SENSITIVE_SEARCH, "[Case Sensitive]"},
#endif
#ifdef FILE_CASE_PRESERVED_NAMES
            {FILE_CASE_PRESERVED_NAMES, "[Case Preserved]"},
#endif
#ifdef FILE_UNICODE_ON_DISK
            {FILE_UNICODE_ON_DISK, "[Unicode on Disk]"},
#endif
#ifdef FILE_PERSISTENT_ACLS
            {FILE_PERSISTENT_ACLS, "[Persistent ACLs]"},
#endif
#ifdef FILE_FILE_COMPRESSION  // ← Правильное имя константы!
            {FILE_FILE_COMPRESSION, "[File Compression]"},
#endif
#ifdef FILE_VOLUME_QUOTAS
            {FILE_VOLUME_QUOTAS, "[Volume Quotas]"},
#endif
#ifdef FILE_SUPPORTS_SPARSE_FILES
            {FILE_SUPPORTS_SPARSE_FILES, "[Sparse Files]"},
#endif
#ifdef FILE_SUPPORTS_REPARSE_POINTS
            {FILE_SUPPORTS_REPARSE_POINTS, "[Reparse Points]"},
#endif
#ifdef FILE_SUPPORTS_REMOTE_STORAGE
            {FILE_SUPPORTS_REMOTE_STORAGE, "[Remote Storage]"},
#endif
#ifdef FILE_SUPPORTS_ENCRYPTION
            {FILE_SUPPORTS_ENCRYPTION, "[Encryption (EFS)]"},
#endif
#ifdef FILE_NAMED_STREAMS
            {FILE_NAMED_STREAMS, "[Named Streams (ADS)]"},
#endif
#ifdef FILE_READ_ONLY_VOLUME
            {FILE_READ_ONLY_VOLUME, "[Read-Only Volume]"},
#endif
#ifdef FILE_VOLUME_IS_COMPRESSED
            {FILE_VOLUME_IS_COMPRESSED, "[Volume Compressed]"},
#endif
#ifdef FILE_SUPPORTS_OBJECT_IDS
            {FILE_SUPPORTS_OBJECT_IDS, "[Object IDs]"},
#endif
#ifdef FILE_SUPPORTS_TRANSACTIONS
            {FILE_SUPPORTS_TRANSACTIONS, "[Transactions (TxF)]"},
#endif
#ifdef FILE_SUPPORTS_HARD_LINKS
            {FILE_SUPPORTS_HARD_LINKS, "[Hard Links]"},
#endif
#ifdef FILE_SUPPORTS_EXTENDED_ATTRIBUTES
            {FILE_SUPPORTS_EXTENDED_ATTRIBUTES, "[Extended Attributes]"},
#endif
#ifdef FILE_SUPPORTS_OPEN_BY_FILE_ID
            {FILE_SUPPORTS_OPEN_BY_FILE_ID, "[Open by File ID]"},
#endif
#ifdef FILE_SUPPORTS_USN_JOURNAL
            {FILE_SUPPORTS_USN_JOURNAL, "[USN Journal]"},
#endif
#ifdef FILE_SUPPORTS_INTEGRITY_STREAMS
            {FILE_SUPPORTS_INTEGRITY_STREAMS, "[Integrity Streams]"},
#endif
#ifdef FILE_SUPPORTS_BLOCK_REFCOUNTING
            {FILE_SUPPORTS_BLOCK_REFCOUNTING, "[Block RefCounting]"},
#endif
#ifdef FILE_DAX_VOLUME
            {FILE_DAX_VOLUME, "[DAX Volume]"},
#endif
            {0, nullptr} // Маркер конца массива
        };
        
        bool anyFlag = false;
        for (int i = 0; flags[i].name != nullptr; ++i) {
            if (fsFlags & flags[i].flag) {
                std::cout << "  " << flags[i].name << std::endl;
                anyFlag = true;
            }
        }
        if (!anyFlag) {
            std::cout << "  [No special capabilities reported]" << std::endl;
        }
        
    } else {
        printLastError("GetVolumeInformation");
    }

    // ========================================================================
    // 3. Свободное место (GetDiskFreeSpace)
    // ========================================================================
    DWORD sectorsPerCluster = 0, bytesPerSector = 0, freeClusters = 0, totalClusters = 0;
    
    if (GetDiskFreeSpaceA(rootPath.c_str(), &sectorsPerCluster, &bytesPerSector, &freeClusters, &totalClusters)) {
        ULONGLONG clusterSize = (ULONGLONG)sectorsPerCluster * bytesPerSector;
        ULONGLONG totalBytes = clusterSize * totalClusters;
        ULONGLONG freeBytes = clusterSize * freeClusters;
        ULONGLONG usedBytes = totalBytes - freeBytes;
        
        std::cout << "\nSpace Information:" << std::endl;
        std::cout << "Cluster Size: " << clusterSize << " bytes" << std::endl;
        std::cout << "Total Space:  " << std::fixed << std::setprecision(2) 
                  << (double)totalBytes / (1024.0 * 1024.0 * 1024.0) << " GB" << std::endl;
        std::cout << "Free Space:   " << std::fixed << std::setprecision(2) 
                  << (double)freeBytes / (1024.0 * 1024.0 * 1024.0) << " GB" << std::endl;
        std::cout << "Used Space:   " << std::fixed << std::setprecision(2) 
                  << (double)usedBytes / (1024.0 * 1024.0 * 1024.0) << " GB" << std::endl;
        
        // Процент использования
        if (totalBytes > 0) {
            double percentUsed = (double)usedBytes / totalBytes * 100.0;
            std::cout << "Usage:        " << std::fixed << std::setprecision(1) 
                      << percentUsed << "%" << std::endl;
        }
    } else {
        printLastError("GetDiskFreeSpace");
    }
}

// 3. Создание и удаление каталогов
void createDirectoryFunc() {
    std::string path = getInputString("\nEnter path for new folder: ");
    if (path.empty()) return;

    if (CreateDirectoryA(path.c_str(), NULL)) {
        std::cout << "Directory created successfully." << std::endl;
    } else {
        printLastError("CreateDirectory");
    }
}

void removeDirectoryFunc() {
    std::string path = getInputString("\nEnter path of folder to delete (must be empty): ");
    if (path.empty()) return;

    if (RemoveDirectoryA(path.c_str())) {
        std::cout << "Directory deleted successfully." << std::endl;
    } else {
        printLastError("RemoveDirectory");
    }
}

// 4. Создание файлов
void createFileFunc() {
    std::string path = getInputString("\nEnter full path for new file: ");
    if (path.empty()) return;

    HANDLE hFile = CreateFileA(
        path.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_NEW, // Ошибка, если файл существует
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        std::cout << "File created successfully." << std::endl;
        CloseHandle(hFile);
    } else {
        printLastError("CreateFile");
    }
}

// 5. Копирование и перемещение файлов
void copyMoveFileFunc() {
    std::cout << "\n--- Copy / Move ---" << std::endl;
    std::string src = getInputString("Source file: ");
    std::string dst = getInputString("Destination file/path: ");
    
    if (src.empty() || dst.empty()) return;

    // Проверка на существование целевого файла (выявление совпадения имен)
    if (GetFileAttributesA(dst.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::cout << "WARNING: Destination file already exists!" << std::endl;
        std::cout << "1. Overwrite" << std::endl;
        std::cout << "2. Cancel" << std::endl;
        int choice;
        std::cout << "Choice: ";
        std::cin >> choice;
        clearInput();
        if (choice != 1) return;
    }

    std::cout << "1. Copy (CopyFile)" << std::endl;
    std::cout << "2. Move (MoveFile)" << std::endl;
    std::cout << "3. Move with extra flags (MoveFileEx)" << std::endl;
    int op;
    std::cout << "Operation: ";
    std::cin >> op;
    clearInput();

    BOOL res = FALSE;
    if (op == 1) {
        // CopyFile: failIfExists = TRUE (но мы уже проверили выше, тут можно FALSE если разрешили перезапись)
        res = CopyFileA(src.c_str(), dst.c_str(), FALSE); 
    } else if (op == 2) {
        res = MoveFileA(src.c_str(), dst.c_str());
    } else if (op == 3) {
        // MOVEFILE_REPLACE_EXISTING позволяет перезаписать без предварительной проверки
        res = MoveFileExA(src.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING);
    }

    if (res) {
        std::cout << "Operation completed successfully." << std::endl;
    } else {
        printLastError("File Operation");
    }
}

// 6. Атрибуты и время файлов
void printFileTime(const char* label, FILETIME* ft) {
    FILETIME localFt;
    SYSTEMTIME st;
    
    // Преобразуем UTC в локальное время
    FileTimeToLocalFileTime(ft, &localFt);
    FileTimeToSystemTime(&localFt, &st);
    
    std::cout << label << std::setw(4) << st.wYear << "-"
              << std::setw(2) << std::setfill('0') << st.wMonth << "-"
              << std::setw(2) << std::setfill('0') << st.wDay << " "
              << std::setw(2) << std::setfill('0') << st.wHour << ":"
              << std::setw(2) << std::setfill('0') << st.wMinute << std::endl;
}

void fileAttributesFunc() {
    std::string path = getInputString("\nEnter file path: ");
    if (path.empty()) return;

    // 1. GetFileAttributes
    DWORD attrs = GetFileAttributesA(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        printLastError("GetFileAttributes");
        return;
    }

    std::cout << "\nCurrent Attributes:" << std::endl;
    std::cout << ((attrs & FILE_ATTRIBUTE_READONLY) ? "[Read-Only] " : "");
    std::cout << ((attrs & FILE_ATTRIBUTE_HIDDEN)   ? "[Hidden] " : "");
    std::cout << ((attrs & FILE_ATTRIBUTE_SYSTEM)   ? "[System] " : "");
    std::cout << ((attrs & FILE_ATTRIBUTE_ARCHIVE)  ? "[Archive] " : "");
    std::cout << std::endl;

    // 2. GetFileInformationByHandle (требует открытия файла)
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION info;
        if (GetFileInformationByHandle(hFile, &info)) {
            std::cout << "File Size (bytes): " << (info.nFileSizeHigh * (MAXDWORD + 1) + info.nFileSizeLow) << std::endl;
            
            // 3. GetFileTime
            std::cout << "\nTime Stamps:" << std::endl;
            printFileTime("Created:  ", &info.ftCreationTime);
            printFileTime("Accessed: ", &info.ftLastAccessTime);
            printFileTime("Modified: ", &info.ftLastWriteTime);
        } else {
            printLastError("GetFileInformationByHandle");
        }
        CloseHandle(hFile);
    } else {
        printLastError("Open File for Info");
    }

    // 4. Изменение атрибутов (SetFileAttributes)
    std::cout << "\nChange attributes?" << std::endl;
    std::cout << "1. Set Read-Only" << std::endl;
    std::cout << "2. Clear Read-Only" << std::endl;
    std::cout << "3. Set Hidden" << std::endl;
    std::cout << "4. Clear Hidden" << std::endl;
    std::cout << "0. Do not change" << std::endl;
    int choice;
    std::cout << "Choice: ";
    std::cin >> choice;
    clearInput();

    DWORD newAttrs = attrs;
    if (choice == 1) newAttrs |= FILE_ATTRIBUTE_READONLY;
    else if (choice == 2) newAttrs &= ~FILE_ATTRIBUTE_READONLY;
    else if (choice == 3) newAttrs |= FILE_ATTRIBUTE_HIDDEN;
    else if (choice == 4) newAttrs &= ~FILE_ATTRIBUTE_HIDDEN;
    else if (choice != 0) return;

    if (choice != 0) {
        if (SetFileAttributesA(path.c_str(), newAttrs)) {
            std::cout << "Attributes updated." << std::endl;
        } else {
            printLastError("SetFileAttributes");
        }
    }

    // 5. Изменение времени (SetFileTime)
    std::cout << "\nChange last write time to current?" << std::endl;
    std::cout << "1. Yes" << std::endl;
    std::cout << "0. No" << std::endl;
    std::cout << "Choice: ";
    std::cin >> choice;
    clearInput();

    if (choice == 1) {
        HANDLE hMod = CreateFileA(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (hMod != INVALID_HANDLE_VALUE) {
            SYSTEMTIME st;
            GetSystemTime(&st);
            FILETIME ft;
            SystemTimeToFileTime(&st, &ft);
            
            // NULL для Creation и Access time означает "не менять", ставим только Write
            if (SetFileTime(hMod, NULL, NULL, &ft)) {
                std::cout << "Time changed." << std::endl;
            } else {
                printLastError("SetFileTime");
            }
            CloseHandle(hMod);
        } else {
            printLastError("Open File for Time Set");
        }
    }
}

// Главное меню
void showMenu() {
    std::cout << "========================================" << std::endl;
    std::cout << "1. List Drives" << std::endl;
    std::cout << "2. Drive Info and Free Space" << std::endl;
    std::cout << "3. Create Directory" << std::endl;
    std::cout << "4. Remove Directory" << std::endl;
    std::cout << "5. Create File" << std::endl;
    std::cout << "6. Copy / Move Files" << std::endl;
    std::cout << "7. File Attributes and Time" << std::endl;
    std::cout << "0. Exit" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
}

int main() {
    int choice;

    while (true) {
        showMenu();
        std::cout << "Enter menu item number: ";
        
        if (!(std::cin >> choice)) {
            clearInput();
            std::cout << "Invalid input." << std::endl;
            continue;
        }
        clearInput();

        switch (choice) {
            case 1: listDrives(); break;
            case 2: getDriveInfo(); break;
            case 3: createDirectoryFunc(); break;
            case 4: removeDirectoryFunc(); break;
            case 5: createFileFunc(); break;
            case 6: copyMoveFileFunc(); break;
            case 7: fileAttributesFunc(); break;
            case 0: 
                std::cout << "Exiting program." << std::endl;
                return 0;
            default:
                std::cout << "Invalid menu item number." << std::endl;
        }
    }
}