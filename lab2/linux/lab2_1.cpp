// Компиляция: g++ -o server server.cpp -std=c++11 -Wall

#include <iostream>
#include <string>
#include <cstring>
#include <cstdint> 
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <cmath>
#include <iomanip>

#define DEFAULT_FILENAME "/tmp/shared_mmap.dat"
#define DEFAULT_SIZE 4096
#define MAX_FILENAME 256

// Структура данных в общей памяти
struct SharedData {
    volatile int status;        // 0 = пусто, 1 = записано, 2 = прочитано
    char buffer[1];             // гибкий массив (данные начинаются после status)
};

// Выравнивание размера до ближайшей степени двойки (минимум 4096)
size_t align_to_power_of_2(size_t size) {
    if (size <= 4096) return 4096;
    size_t aligned = 1;
    while (aligned < size + sizeof(int)) {  // + место под status
        aligned <<= 1;
    }
    return aligned;
}

// Получение размера данных (вычитаем размер заголовка)
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
    
    std::cout << "=== Приложение сервера ===" << std::endl;
    
    while (true) {
        std::cout << "\n--- Меню сервера ---" << std::endl;
        std::cout << "1. Выполнить проецирование" << std::endl;
        std::cout << "2. Записать данные" << std::endl;
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
                
                // Запрос имени файла
                std::cout << "Введите имя файла [" << DEFAULT_FILENAME << "]: ";
                std::string filename_input;
                std::getline(std::cin, filename_input);
                if (!filename_input.empty()) {
                    current_filename = filename_input;
                }
                std::cout << "Используемый файл: " << current_filename << std::endl;
                
                // Запрос размера файла
                std::cout << "Введите размер данных (байт): ";
                size_t requested_size;
                std::cin >> requested_size;
                std::cin.ignore();
                
                // Выравнивание до степени 2
                current_size = align_to_power_of_2(requested_size + sizeof(int));
                std::cout << "Запрошено: " << requested_size 
                          << " байт, выровнено до: " << current_size 
                          << " байт" << std::endl;
                
                // Создание/открытие файла
                fd = open(current_filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 
                         S_IRUSR | S_IWUSR);
                if (fd == -1) {
                    perror("open");
                    break;
                }
                
                // Установка размера файла
                if (ftruncate(fd, current_size) == -1) {
                    perror("ftruncate");
                    close(fd);
                    fd = -1;
                    break;
                }
                
                // Отображение файла в память
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
                shared->status = 0;  // нет данных
                std::memset(shared->buffer, 0, get_data_size(current_size));
                is_mapped = true;
                
                // вывод адреса отображенной памяти
                std::cout << "Файл спроецирован в память!" << std::endl;
                std::cout << "    Адрес отображения: " 
                          << "0x" << std::hex << std::uintptr_t(mapped_ptr) << std::dec << std::endl;
                std::cout << "    Размер области: " << current_size << " байт" << std::endl;
                break;
            }
            
            case 2: {
                if (!is_mapped || !shared) {
                    std::cout << "Сначала выполните проецирование!" << std::endl;
                    break;
                }

                std::cout << "Введите данные для записи: ";
                std::string input;
                std::getline(std::cin, input);

                // Запись данных в общую память
                size_t data_size = get_data_size(current_size);
                std::strncpy(shared->buffer, input.c_str(), data_size - 1);
                shared->buffer[data_size - 1] = '\0';
                shared->status = 1;  // данные готовы

                std::cout << "Данные записаны в общую память!" << std::endl;
                std::cout << "Ожидание прочтения..." << std::flush;

                // бесконечное ожидание (пока клиент не прочитает)
                while (shared->status != 2) {
                    usleep(10000);  // 10 мс — чтобы не грузить процессор на 100%
                }

                // Клиент прочитал!
                std::cout << "\nФайл прочитан клиентом!" << std::endl;
                shared->status = 0;  // сброс для следующей итерации
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
                
                // Удаление файла с диска
                if (unlink(current_filename.c_str()) == -1) {
                    perror("unlink");
                }
                
                std::cout << "Сервер завершил работу. Файл удалён." << std::endl;
                return 0;
            }
            
            default:
                std::cout << "Неверный выбор." << std::endl;
        }
    }
    
    return 0;
}