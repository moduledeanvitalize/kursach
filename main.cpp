#include <iostream>
#include <string>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "class/api_routes.h"
#include "class/server_common.h"
#include "httplib.h"

// Главная функция приложения. Инициализирует БД, регистрирует API маршруты
// и запускает HTTP сервер на порту 8080 для управления проектами и задачами.
int main() {
    httplib::Server svr;

    sqlite3* dbHandle = nullptr;
    const sqlite3_int64 defaultProjectId = 1;
    const std::string dbPath = server::getExecutableDir() + "/course.db";
    
    // Инициализация базы данных и создание схемы при необходимости
    if (!server::initializeDatabase(dbHandle, dbPath, defaultProjectId)) {
        return 1;
    }

    #ifdef _WIN32
    const int serverPid = static_cast<int>(_getpid());
    #else
    const int serverPid = static_cast<int>(getpid());
    #endif
    // Регистрация всех API маршрутов для работы с проектами, задачами и их зависимостями
    api_routes::registerRoutes(svr, dbHandle, defaultProjectId, dbPath, serverPid);

    std::cout << "Server starting: http://localhost:8080 (pid=" << serverPid << ")" << std::endl;
    // Запуск сервера на localhost:8080
    if (!svr.listen("0.0.0.0", 8080)) {
        std::cerr << "Не удалось запустить сервер на порту 8080 (возможно, порт уже занят)." << std::endl;
        sqlite3_close(dbHandle);
        return 1;
    }

    sqlite3_close(dbHandle);
    return 0;
}
