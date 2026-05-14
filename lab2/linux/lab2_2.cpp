// Компиляция: g++ -o client client.cpp -std=c++11 -Wall

#include <iostream>
#include <string>
#include <cstring>
#include <fcntl.h>
#include <cstdint> 
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <cmath>
#include <iomanip>

#define DEFAULT_FILENAME "/tmp/shared_mmap.dat"
#define DEFAULT_SIZE 4096
#define MAX_FILENAME 256

// Структура данных в общей памяти (совпадает с сервером)
struct SharedData {
    volatile int status;        // 0 = пусто, 1 = записано, 2 = прочитано
    char buffer[1];             // гибкий массив
};

// Выравнивание размера до ближайшей степени двойки
size_t align_to_power_of_2(size_t size) {
    if (size <= 4096) return 4096;
    size_t aligned = 1;
    while (aligned < size + sizeof(int)) {
        aligned <<= 1;
    }
    return aligned;
}

// Получение размера данных
size_t get_data_size(size_t total_size) {
    return total_size - sizeof(int);
}

int main() {
    int fd = -1;
    SharedData* shared = nullptr;
    bool is_mapped = false;
    std::string current_filename = DEFAULT_FILENAME;
    size_t current_size = DEFAULT_SIZE;
    void* mapped_ptr = nullptr;
    
    std::cout << "=== Приложение клиента ===" << std::endl;
    
    while (true) {
        std::cout << "\n--- Меню клиента ---" << std::endl;
        std::cout << "1. Выполнить проецирование" << std::endl;
        std::cout << "2. Прочитать данные" << std::endl;
        std::cout << "3. Завершить работу" << std::endl;
        std::cout << "Выбор: ";
        
        int choice;
        std::cin >> choice;
        std::cin.ignore();
        
        switch (choice) {
            case 1: {
                if (is_mapped) {
                    std::cout << "Файл уже спроецирован в память!" << std::endl;
                    break;
                }
                
                // 1. Запрос имени файла
                std::cout << "Введите имя файла [" << DEFAULT_FILENAME << "]: ";
                std::string filename_input;
                std::getline(std::cin, filename_input);
                if (!filename_input.empty()) {
                    current_filename = filename_input;
                }
                std::cout << "Используемый файл: " << current_filename << std::endl;
                
                // 2. Открытие файла
                fd = open(current_filename.c_str(), O_RDWR);
                if (fd == -1) {
                    perror("open");
                    std::cout << "Файл не найден. Запустите сервер первым!" << std::endl;
                    break;
                }
                
                struct stat st;
                if (fstat(fd, &st) == -1) {
                    perror("fstat");
                    close(fd);
                    fd = -1;
                    break;
                }
                current_size = st.st_size;  // Точный размер, установленный сервером
                std::cout << "Размер файла: " << current_size << " байт" << std::endl;
                
                // 4. Отображение в память
                mapped_ptr = mmap(nullptr, current_size, 
                                PROT_READ | PROT_WRITE, 
                                MAP_SHARED, fd, 0);
                if (mapped_ptr == MAP_FAILED) {
                    perror("mmap");
                    close(fd);
                    fd = -1;
                    break;
                }
                
                shared = reinterpret_cast<SharedData*>(mapped_ptr);
                is_mapped = true;
                
                std::cout << "Файл спроецирован в память!" << std::endl;
                std::cout << "    Адрес отображения: 0x" 
                        << std::hex << std::uintptr_t(mapped_ptr) << std::dec << std::endl;
                std::cout << "    Размер области: " << current_size << " байт" << std::endl;
                break;
            }
            
            case 2: {
                if (!is_mapped || !shared) {
                    std::cout << "Сначала выполните проецирование!" << std::endl;
                    break;
                }
                
                std::cout << "Ожидание данных..." << std::endl;
                
                // Ожидание появления данных (опрос с таймаутом)
                const int MAX_ATTEMPTS = 100;
                int attempts = 0;
                
                while (shared->status != 1 && attempts < MAX_ATTEMPTS) {
                    usleep(100000);  // 100 мс
                    attempts++;
                }
                
                if (shared->status == 1) {
                    // Чтение данных
                    std::cout << "\n>>> Прочитано: " << shared->buffer << std::endl;
                    
                    // файл прочитан
                    shared->status = 2;
                    std::cout << "Файл прочитан" << std::endl;
                } else if (shared->status == 2) {
                    std::cout << "Данные уже были прочитаны ранее" << std::endl;
                } else {
                    std::cout << "Таймаут: данные не поступили" << std::endl;
                }
                break;
            }
            
            case 3: {
                // Отмена проецирования
                if (is_mapped && mapped_ptr) {
                    if (munmap(mapped_ptr, current_size) == -1) {
                        perror("munmap");
                    }
                    is_mapped = false;
                    mapped_ptr = nullptr;
                }
                
                // Закрытие дескриптора файла
                if (fd != -1) {
                    close(fd);
                    fd = -1;
                }
                
                std::cout << "Клиент завершил работу." << std::endl;
                return 0;
            }
            
            default:
                std::cout << "Неверный выбор. Попробуйте снова." << std::endl;
        }
    }
    
    return 0;
}