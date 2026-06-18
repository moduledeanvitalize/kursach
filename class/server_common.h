#pragma once

#include <fstream>
#include <iostream>
#include <limits.h>
#include <sstream>
#include <string>
#include <unistd.h>

#include "../httplib.h"
#include "database.h"

namespace server {

// Возвращает директорию запущенного бинарного файла.
inline std::string getExecutableDir() {
    char pathBuf[PATH_MAX] = {0};
    const ssize_t len = readlink("/proc/self/exe", pathBuf, sizeof(pathBuf) - 1);
    if (len <= 0) {
        return ".";
    }

    std::string fullPath(pathBuf, static_cast<size_t>(len));
    const size_t slashPos = fullPath.find_last_of('/');
    if (slashPos == std::string::npos) {
        return ".";
    }

    return fullPath.substr(0, slashPos);
}

// Отключает кеширование ответа браузером.
inline void setNoCacheHeaders(httplib::Response& res) {
    res.set_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    res.set_header("Pragma", "no-cache");
    res.set_header("Expires", "0");
}

// Читает файл целиком в строку.
inline bool readFile(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

// Универсальная отдача статического файла с обработкой ошибки.
inline void serveStaticFile(
    httplib::Response& res,
    const std::string& path,
    const char* contentType,
    const std::string& openErrorText
) {
    std::string body;
    if (!readFile(path, body)) {
        res.status = 500;
        res.set_content(openErrorText, "text/plain; charset=UTF-8");
        return;
    }

    setNoCacheHeaders(res);
    res.set_content(body, contentType);
}

// Гарантирует наличие дефолтного проекта с фиксированным id.
inline bool ensureDefaultProject(sqlite3* dbHandle, sqlite3_int64 projectId) {
    const char* sql =
        "INSERT INTO projects(id, name) "
        "SELECT ?, 'Менеджер проектов' "
        "WHERE NOT EXISTS (SELECT 1 FROM projects WHERE id = ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(dbHandle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Ошибка подготовки ensureDefaultProject: " << sqlite3_errmsg(dbHandle) << std::endl;
        return false;
    }

    sqlite3_bind_int64(stmt, 1, projectId);
    sqlite3_bind_int64(stmt, 2, projectId);

    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) {
        std::cerr << "Ошибка ensureDefaultProject: " << sqlite3_errmsg(dbHandle) << std::endl;
    }

    sqlite3_finalize(stmt);
    return ok;
}

// Открывает БД и выполняет базовые проверки/инициализацию схемы.
inline bool initializeDatabase(sqlite3*& dbHandle, const std::string& dbPath, sqlite3_int64 projectId) {
    dbHandle = nullptr;
    if (sqlite3_open(dbPath.c_str(), &dbHandle) != SQLITE_OK) {
        std::cerr << "Ошибка открытия БД: " << sqlite3_errmsg(dbHandle) << std::endl;
        sqlite3_close(dbHandle);
        return false;
    }

    if (!db::execSql(dbHandle, "PRAGMA foreign_keys = ON;", "Ошибка включения foreign keys: ")) {
        sqlite3_close(dbHandle);
        return false;
    }

    if (!db::initSchema(dbHandle)) {
        sqlite3_close(dbHandle);
        return false;
    }

    if (!ensureDefaultProject(dbHandle, projectId)) {
        sqlite3_close(dbHandle);
        return false;
    }

    return true;
}

} // namespace server
