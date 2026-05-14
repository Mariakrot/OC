
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <aio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <iostream>

// Структура асинхронной операции
struct aio_operation {
    struct aiocb aio;           // Блок управления AIO
    char *buffer;               // Буфер данных
    bool write_operation;       // false = чтение, true = запись
    off_t file_offset;          // Смещение в файле
    size_t requested_size;      // Запрошенный размер
    size_t actual_size;         // Фактически обработанный размер
    bool completed;             // Флаг завершения
    bool in_use;                // Флаг использования
};

// Глобальные переменные для синхронизации и конфигурации
static std::mutex g_mutex;
static std::condition_variable g_cv;
static std::atomic<int> g_pending_ops{0};
static std::atomic<int64_t> g_bytes_copied{0};
static std::atomic<bool> g_error_occurred{false};

static int g_read_fd = -1;
static int g_write_fd = -1;
static size_t g_block_size = 4096;
static int g_num_concurrent = 1;
static off_t g_file_size = 0;

// Обработчик завершения асинхронной операции
// Вызывается автоматически при завершении aio_read/aio_write
void aio_completion_handler(sigval_t sigval) {
    struct aio_operation *aio_op = (struct aio_operation *)sigval.sival_ptr;
    if (!aio_op) return;

    // Получаем результат завершённой операции
    ssize_t result = aio_return(&aio_op->aio);

    if (result < 0) {
        fprintf(stderr, "AIO error: %s\n", strerror(errno));
        g_error_occurred = true;
    } 
    else if (!aio_op->write_operation) {
        // === Завершено ЧТЕНИЕ ===
        if (result == 0) {
            // EOF - больше данных нет
            aio_op->completed = true;
            aio_op->in_use = false;
            g_pending_ops--;
            g_cv.notify_all();
            return;
        }

        // Подготовка к операции ЗАПИСИ тех же данных
        aio_op->actual_size = static_cast<size_t>(result);
        aio_op->write_operation = true;  // Переключаем флаг
        aio_op->aio.aio_fildes = g_write_fd;  // Меняем дескриптор на файл назначения
        aio_op->aio.aio_buf = aio_op->buffer;
        aio_op->aio.aio_nbytes = result;  // Записываем только прочитанное
        // Смещение и сигнатурные поля сохраняются

        // Отправка асинхронной записи
        if (aio_write(&aio_op->aio) < 0) {
            fprintf(stderr, "aio_write submit failed: %s\n", strerror(errno));
            g_error_occurred = true;
            aio_op->completed = true;
            aio_op->in_use = false;
            g_pending_ops--;
            g_cv.notify_all();
        }

        return;
    }
    
    // === Завершена ЗАПИСЬ ===
    if (result > 0) {
        g_bytes_copied += result;
    }

    // Освобождаем операцию для повторного использования
    aio_op->completed = true;
    aio_op->in_use = false;
    
    g_pending_ops--;
    g_cv.notify_all();  // Уведомляем главный поток
}

// Отправка асинхронной операции чтения
bool submit_read(aio_operation *op, off_t offset, size_t size) {
    op->write_operation = false;
    op->file_offset = offset;
    op->requested_size = size;
    op->actual_size = 0;
    op->completed = false;
    op->in_use = true;

    op->aio.aio_fildes = g_read_fd;
    op->aio.aio_offset = offset;
    op->aio.aio_buf = op->buffer;
    op->aio.aio_nbytes = size;
    
    // Настройка механизма завершения (APC-style callback)
    op->aio.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    op->aio.aio_sigevent.sigev_signo = SIGRTMIN;
    op->aio.aio_sigevent.sigev_value.sival_ptr = op;

    if (aio_read(&op->aio) < 0) {
        fprintf(stderr, "aio_read submit failed: %s\n", strerror(errno));
        op->in_use = false;
        return false;
    }

    g_pending_ops++;
    return true;
}


// Получение размера кластера файловой системы
size_t get_cluster_size(const char *path) {
    struct statfs fs_info;
    if (statfs(path, &fs_info) == 0) {
        return fs_info.f_bsize;
    }
    return 4096; // Значение по умолчанию
}

// Высокоточный таймер (миллисекунды)
double get_time_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(now.time_since_epoch()).count();
}

int main(int argc, char *argv[]) {
    // --- Парсинг аргументов ---
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_file> <dest_file> [block_size] [num_concurrent]\n", argv[0]);
        fprintf(stderr, "  block_size: I/O block size in bytes (default: 4096, must be multiple of cluster size)\n");
        fprintf(stderr, "  num_concurrent: number of overlapping I/O operations (1..64, default: 1)\n");
        fprintf(stderr, "\nExample: %s input.bin output.bin 65536 4\n", argv[0]);
        return 1;
    }

    const char *src_path = argv[1];
    const char *dst_path = argv[2];
    
    if (argc >= 4) {
        g_block_size = std::max(512, atoi(argv[3]));
    }
    if (argc >= 5) {
        g_num_concurrent = std::max(1, std::min(64, atoi(argv[4])));
    }

    // Выравнивание размера блока по кластеру ФС
    size_t cluster_size = get_cluster_size(dst_path);
    if (g_block_size % cluster_size != 0) {
        g_block_size = ((g_block_size + cluster_size - 1) / cluster_size) * cluster_size;
        fprintf(stderr, "[INFO] Block size aligned to %zu bytes (cluster: %zu)\n", 
                g_block_size, cluster_size);
    }

    // Открытие исходного файла
    g_read_fd = open(src_path, O_RDONLY | O_NONBLOCK, 0666);
    if (g_read_fd < 0) {
        perror("Failed to open source file");
        return 1;
    }

    // Получение размера файла
    struct stat st;
    if (fstat(g_read_fd, &st) < 0) {
        perror("Failed to get file size");
        close(g_read_fd);
        return 1;
    }
    g_file_size = st.st_size;
    fprintf(stderr, "[INFO] Source file size: %" PRId64 " bytes\n", static_cast<int64_t>(g_file_size));

    // Открытие/создание файла назначения
    g_write_fd = open(dst_path, O_CREAT | O_WRONLY | O_TRUNC | O_NONBLOCK, 0666);
    if (g_write_fd < 0) {
        perror("Failed to open destination file");
        close(g_read_fd);
        return 1;
    }

    // Используем SA_SIGINFO для доступа к sigval_t
    struct sigaction sa = {};
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = [](int, siginfo_t *si, void*) {
        // Делегируем вызов нашей функции из задания
        aio_completion_handler(si->si_value);
    };
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(SIGRTMIN, &sa, nullptr) < 0) {
        perror("Failed to setup signal handler");
        close(g_read_fd);
        close(g_write_fd);
        return 1;
    }

    // --- Выделение ресурсов для операций ---
    std::vector<aio_operation> operations(g_num_concurrent);
    for (int i = 0; i < g_num_concurrent; ++i) {
        void* aligned_ptr = nullptr;
        // posix_memalign возвращает 0 при успехе, а не указатель
        // Требования: выравнивание должно быть кратно sizeof(void*) и степени двойки
        int alloc_result = posix_memalign(&aligned_ptr, cluster_size, g_block_size);

        if (alloc_result != 0 || !aligned_ptr) {
            fprintf(stderr, "Failed to allocate aligned buffer: %s\n", strerror(alloc_result));
            // Освобождаем ранее выделенные буферы перед выходом
            for (int j = 0; j < i; ++j) free(operations[j].buffer);
            close(g_read_fd);
            close(g_write_fd);
            return 1;
        }
        operations[i].buffer = static_cast<char*>(aligned_ptr);
        operations[i].completed = true;
        operations[i].in_use = false;
    }

    fprintf(stderr, "[INFO] Starting copy: %d concurrent ops, %zu block size\n", 
            g_num_concurrent, g_block_size);

    // --- Замер времени начала ---
    double start_time = get_time_ms();

    // --- Инициализация конвейера: отправка первых операций чтения ---
    off_t next_read_offset = 0;
    
    for (int i = 0; i < g_num_concurrent && next_read_offset < g_file_size; ++i) {
        size_t size = std::min(g_block_size, static_cast<size_t>(g_file_size - next_read_offset));
        if (size > 0) {
            if (!submit_read(&operations[i], next_read_offset, size)) {
                break;
            }
            next_read_offset += size;
        }
    }

    while (true) {
        // Ожидание изменения состояния: освободился слот или завершились все операции
        {
            std::unique_lock<std::mutex> lock(g_mutex);
            g_cv.wait(lock, [] {
                return g_pending_ops < g_num_concurrent || 
                       g_pending_ops == 0 || 
                       g_error_occurred.load();
            });
        }

        if (g_error_occurred) {
            fprintf(stderr, "[ERROR] Aborting due to I/O error\n");
            break;
        }

        // Если есть данные для чтения и свободные слоты - отправляем новые запросы
        if (next_read_offset < g_file_size) {
            for (auto &op : operations) {
                if (!op.in_use && next_read_offset < g_file_size) {
                    size_t size = std::min(g_block_size, 
                                          static_cast<size_t>(g_file_size - next_read_offset));
                    if (size > 0 && submit_read(&op, next_read_offset, size)) {
                        next_read_offset += size;
                    }
                }
            }
        }

        // Проверка условия завершения: все данные отправлены на чтение И все операции завершены
        if (next_read_offset >= g_file_size && g_pending_ops == 0) {
            break;
        }
    }

    // Финальное ожидание (на случай гонки)
    {
        std::unique_lock<std::mutex> lock(g_mutex);
        g_cv.wait(lock, [] { return g_pending_ops.load() == 0; });
    }

    // --- Замер времени конца и очистка ---
    double end_time = get_time_ms();
    double duration_ms = end_time - start_time;

    for (auto &op : operations) {
        free(op.buffer);
    }
    close(g_read_fd);
    close(g_write_fd);

    // --- Вывод результатов ---
    double throughput_mbs = 0.0;
    if (duration_ms > 0) {
        throughput_mbs = (g_bytes_copied.load() / 1024.0 / 1024.0) / (duration_ms / 1000.0);
    }

    printf("\n================ COPY RESULTS ================\n");
    printf("Source:      %s\n", src_path);
    printf("Destination: %s\n", dst_path);
    printf("File size:   %" PRId64 " bytes (%.3f MB)\n", 
           static_cast<int64_t>(g_file_size), g_file_size / 1024.0 / 1024.0);
    printf("Block size:  %zu bytes\n", g_block_size);
    printf("Concurrent:  %d operations\n", g_num_concurrent);
    printf("Time:        %.2f ms\n", duration_ms);
    printf("Throughput:  %.2f MB/s\n", throughput_mbs);
    printf("Copied:      %" PRId64 " bytes\n", g_bytes_copied.load());
    printf("==============================================\n");

    return g_error_occurred ? 1 : 0;
}