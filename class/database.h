#pragma once

#include <cstring>
#include <iostream>
#include "sqlite3.h"

#include "project.h"

namespace db {

// Проверяет, есть ли столбец columnName в таблице tableName.
inline bool hasColumn(sqlite3* dbHandle, const char* tableName, const char* columnName) {
    const std::string sql = std::string("PRAGMA table_info(") + tableName + ");";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(dbHandle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Ошибка подготовки table_info: " << sqlite3_errmsg(dbHandle) << std::endl;
        return false;
    }

    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* currentName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (currentName != nullptr && std::strcmp(currentName, columnName) == 0) {
            found = true;
            break;
        }
    }

    sqlite3_finalize(stmt);
    return found;
}

// Выполняет одну или несколько SQL-команд и выводит ошибку с префиксом.
// Возвращает true при успехе и false при любой ошибке sqlite.
inline bool execSql(sqlite3* dbHandle, const char* sql, const char* errorPrefix) {
    char* errMsg = nullptr;
    if (sqlite3_exec(dbHandle, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << errorPrefix << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// Создает необходимые таблицы базы данных, если их еще нет.
// Возвращает true, если схема успешно создана.
inline bool initSchema(sqlite3* dbHandle) {
    const char* schemaSql =
        "CREATE TABLE IF NOT EXISTS projects ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "description TEXT NOT NULL DEFAULT '', "
        "start_date INTEGER NOT NULL DEFAULT 0, "
        "end_date INTEGER NOT NULL DEFAULT 0, "
        "created_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
        ");"
        "CREATE TABLE IF NOT EXISTS tasks ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "project_id INTEGER NOT NULL, "
        "title TEXT NOT NULL, "
        "status INTEGER NOT NULL DEFAULT 0, "
        "deadline INTEGER NOT NULL DEFAULT 0, "
        "description TEXT NOT NULL DEFAULT '', "
        "assignee TEXT NOT NULL DEFAULT '', "
        "start_date INTEGER NOT NULL DEFAULT 0, "
        "end_date INTEGER NOT NULL DEFAULT 0, "
        "parent_task_id INTEGER, "
        "progress INTEGER NOT NULL DEFAULT 0, "
        "FOREIGN KEY(project_id) REFERENCES projects(id)"
        ",FOREIGN KEY(parent_task_id) REFERENCES tasks(id) ON DELETE SET NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS task_dependencies ("
        "task_id INTEGER NOT NULL, "
        "depends_on_task_id INTEGER NOT NULL, "
        "PRIMARY KEY(task_id, depends_on_task_id), "
        "FOREIGN KEY(task_id) REFERENCES tasks(id) ON DELETE CASCADE, "
        "FOREIGN KEY(depends_on_task_id) REFERENCES tasks(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS task_events ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "task_id INTEGER NOT NULL, "
        "command_type TEXT NOT NULL, "
        "payload TEXT NOT NULL DEFAULT '', "
        "created_at INTEGER NOT NULL DEFAULT (strftime('%s','now')), "
        "FOREIGN KEY(task_id) REFERENCES tasks(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS notifications ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "task_id INTEGER NOT NULL, "
        "assignee TEXT NOT NULL, "
        "kind TEXT NOT NULL, "
        "message TEXT NOT NULL, "
        "is_read INTEGER NOT NULL DEFAULT 0, "
        "created_at INTEGER NOT NULL DEFAULT (strftime('%s','now')), "
        "FOREIGN KEY(task_id) REFERENCES tasks(id) ON DELETE CASCADE"
        ");";

    if (!execSql(dbHandle, schemaSql, "Ошибка при создании таблиц: ")) {
        return false;
    }

    if (!execSql(dbHandle, "CREATE INDEX IF NOT EXISTS idx_tasks_project_id_id ON tasks(project_id, id);", "Ошибка создания индекса tasks(project_id, id): ")) {
        return false;
    }
    if (!execSql(dbHandle, "CREATE INDEX IF NOT EXISTS idx_task_dependencies_task_id ON task_dependencies(task_id);", "Ошибка создания индекса task_dependencies(task_id): ")) {
        return false;
    }
    if (!execSql(dbHandle, "CREATE INDEX IF NOT EXISTS idx_task_events_task_id ON task_events(task_id);", "Ошибка создания индекса task_events(task_id): ")) {
        return false;
    }

    // Миграция старых БД: добавляем недостающие столбцы в tasks.
    if (!hasColumn(dbHandle, "tasks", "deadline")) {
        if (!execSql(dbHandle, "ALTER TABLE tasks ADD COLUMN deadline INTEGER NOT NULL DEFAULT 0;", "Ошибка миграции deadline: ")) {
            return false;
        }
    }

    if (!hasColumn(dbHandle, "tasks", "description")) {
        if (!execSql(dbHandle, "ALTER TABLE tasks ADD COLUMN description TEXT NOT NULL DEFAULT '';", "Ошибка миграции description: ")) {
            return false;
        }
    }

    if (!hasColumn(dbHandle, "projects", "description")) {
        if (!execSql(dbHandle, "ALTER TABLE projects ADD COLUMN description TEXT NOT NULL DEFAULT '';", "Ошибка миграции projects.description: ")) {
            return false;
        }
    }
    if (!hasColumn(dbHandle, "projects", "start_date")) {
        if (!execSql(dbHandle, "ALTER TABLE projects ADD COLUMN start_date INTEGER NOT NULL DEFAULT 0;", "Ошибка миграции projects.start_date: ")) {
            return false;
        }
    }
    if (!hasColumn(dbHandle, "projects", "end_date")) {
        if (!execSql(dbHandle, "ALTER TABLE projects ADD COLUMN end_date INTEGER NOT NULL DEFAULT 0;", "Ошибка миграции projects.end_date: ")) {
            return false;
        }
    }
    if (!hasColumn(dbHandle, "projects", "created_at")) {
        if (!execSql(dbHandle, "ALTER TABLE projects ADD COLUMN created_at INTEGER NOT NULL DEFAULT 0;", "Ошибка миграции projects.created_at: ")) {
            return false;
        }
    }

    if (!hasColumn(dbHandle, "tasks", "assignee")) {
        if (!execSql(dbHandle, "ALTER TABLE tasks ADD COLUMN assignee TEXT NOT NULL DEFAULT '';", "Ошибка миграции tasks.assignee: ")) {
            return false;
        }
    }
    if (!hasColumn(dbHandle, "tasks", "start_date")) {
        if (!execSql(dbHandle, "ALTER TABLE tasks ADD COLUMN start_date INTEGER NOT NULL DEFAULT 0;", "Ошибка миграции tasks.start_date: ")) {
            return false;
        }
    }
    if (!hasColumn(dbHandle, "tasks", "end_date")) {
        if (!execSql(dbHandle, "ALTER TABLE tasks ADD COLUMN end_date INTEGER NOT NULL DEFAULT 0;", "Ошибка миграции tasks.end_date: ")) {
            return false;
        }
    }
    if (!hasColumn(dbHandle, "tasks", "parent_task_id")) {
        if (!execSql(dbHandle, "ALTER TABLE tasks ADD COLUMN parent_task_id INTEGER;", "Ошибка миграции tasks.parent_task_id: ")) {
            return false;
        }
    }
    if (!hasColumn(dbHandle, "tasks", "progress")) {
        if (!execSql(dbHandle, "ALTER TABLE tasks ADD COLUMN progress INTEGER NOT NULL DEFAULT 0;", "Ошибка миграции tasks.progress: ")) {
            return false;
        }
    }

    if (!execSql(dbHandle, "UPDATE tasks SET end_date = deadline WHERE end_date = 0 AND deadline > 0;", "Ошибка миграции заполнения end_date: ")) {
        return false;
    }
    if (!execSql(dbHandle, "UPDATE tasks SET start_date = CASE WHEN end_date > 86400 THEN end_date - 86400 ELSE end_date END WHERE start_date = 0 AND end_date > 0;", "Ошибка миграции заполнения start_date: ")) {
        return false;
    }
    if (!execSql(dbHandle, "UPDATE tasks SET progress = CASE WHEN status = 3 THEN 100 ELSE progress END WHERE progress < 100;", "Ошибка миграции заполнения progress: ")) {
        return false;
    }

    if (hasColumn(dbHandle, "tasks", "status")) {
        if (!execSql(dbHandle, "CREATE INDEX IF NOT EXISTS idx_tasks_project_status ON tasks(project_id, status);", "Ошибка создания индекса tasks(project_id, status): ")) {
            return false;
        }
    }
    if (hasColumn(dbHandle, "tasks", "assignee")) {
        if (!execSql(dbHandle, "CREATE INDEX IF NOT EXISTS idx_tasks_assignee ON tasks(assignee);", "Ошибка создания индекса tasks(assignee): ")) {
            return false;
        }
    }

    return true;
}

// Сохраняет один проект и все его задачи в рамках одной транзакции.
// В outProjectId записывается id добавленной строки проекта.
// Возвращает true только если все вставки и commit выполнены успешно.
inline bool saveProjectWithTasks(sqlite3* dbHandle, const Project& project, sqlite3_int64& outProjectId) {
    if (!execSql(dbHandle, "BEGIN TRANSACTION;", "Ошибка начала транзакции: ")) {
        return false;
    }

    bool hasError = false;
    sqlite3_stmt* insertProjectStmt = nullptr;
    const char* insertProjectSql = "INSERT INTO projects(name) VALUES(?);";

    if (sqlite3_prepare_v2(dbHandle, insertProjectSql, -1, &insertProjectStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Ошибка подготовки INSERT projects: " << sqlite3_errmsg(dbHandle) << std::endl;
        hasError = true;
    } else {
        sqlite3_bind_text(insertProjectStmt, 1, project.getName().c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(insertProjectStmt) != SQLITE_DONE) {
            std::cerr << "Ошибка вставки проекта: " << sqlite3_errmsg(dbHandle) << std::endl;
            hasError = true;
        }
    }
    sqlite3_finalize(insertProjectStmt);

    outProjectId = sqlite3_last_insert_rowid(dbHandle);

    sqlite3_stmt* insertTaskStmt = nullptr;
    const char* insertTaskSql = "INSERT INTO tasks(project_id, title, status, deadline, description) VALUES(?, ?, ?, ?, ?);";

    if (!hasError && sqlite3_prepare_v2(dbHandle, insertTaskSql, -1, &insertTaskStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Ошибка подготовки INSERT tasks: " << sqlite3_errmsg(dbHandle) << std::endl;
        hasError = true;
    }

    if (!hasError) {
        for (const auto& task : project.getTasks()) {
            sqlite3_reset(insertTaskStmt);
            sqlite3_clear_bindings(insertTaskStmt);

            sqlite3_bind_int64(insertTaskStmt, 1, outProjectId);
            sqlite3_bind_text(insertTaskStmt, 2, task.getTitle().c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(insertTaskStmt, 3, static_cast<int>(task.getStatus()));
            sqlite3_bind_int(insertTaskStmt, 4, task.getDeadline());
            sqlite3_bind_text(insertTaskStmt, 5, task.getDescription().c_str(), -1, SQLITE_TRANSIENT);


            if (sqlite3_step(insertTaskStmt) != SQLITE_DONE) {
                std::cerr << "Ошибка вставки задачи: " << sqlite3_errmsg(dbHandle) << std::endl;
                hasError = true;
                break;
            }
        }
    }
    sqlite3_finalize(insertTaskStmt);

    if (hasError) {
        sqlite3_exec(dbHandle, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    if (!execSql(dbHandle, "COMMIT;", "Ошибка commit: ")) {
        sqlite3_exec(dbHandle, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    return true;
}

// Читает задачи для указанного id проекта и выводит их в консоль.
// Возвращает true, если запрос выполнен успешно.
inline bool printTasksFromDb(sqlite3* dbHandle, sqlite3_int64 projectId) {
    std::cout << "\nЗадачи из БД:\n";

    sqlite3_stmt* selectStmt = nullptr;
    const char* selectSql = "SELECT id, project_id, title, status, deadline, description FROM tasks WHERE project_id = ?;";
    if (sqlite3_prepare_v2(dbHandle, selectSql, -1, &selectStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Ошибка SELECT tasks: " << sqlite3_errmsg(dbHandle) << std::endl;
        return false;
    }

    sqlite3_bind_int64(selectStmt, 1, projectId);
    while (sqlite3_step(selectStmt) == SQLITE_ROW) {
        const int id = sqlite3_column_int(selectStmt, 0);
        const int dbProjectId = sqlite3_column_int(selectStmt, 1);
        const char* title = reinterpret_cast<const char*>(sqlite3_column_text(selectStmt, 2));
        const int status = sqlite3_column_int(selectStmt, 3);
        const int deadline = sqlite3_column_int(selectStmt, 4);
        const char* description = reinterpret_cast<const char*>(sqlite3_column_text(selectStmt, 5));


        std::cout << "[" << id << "] project_id=" << dbProjectId
                  << ", title=" << (title ? title : "")
                  << ", status=" << status
                  << ", deadline=" << deadline
                  << ", description=" << (description ? description : "")
                  << std::endl;
    }

    sqlite3_finalize(selectStmt);
    return true;
}

// Находит id задачи по id проекта и названию.
// Возвращает true, если задача найдена.
inline bool findTaskIdByTitle(sqlite3* dbHandle, sqlite3_int64 projectId, const std::string& title, sqlite3_int64& outTaskId) {
    const char* findSql = "SELECT id FROM tasks WHERE project_id = ? AND title = ? LIMIT 1;";
    sqlite3_stmt* findStmt = nullptr;

    if (sqlite3_prepare_v2(dbHandle, findSql, -1, &findStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Ошибка подготовки поиска задачи: " << sqlite3_errmsg(dbHandle) << std::endl;
        return false;
    }

    sqlite3_bind_int64(findStmt, 1, projectId);
    sqlite3_bind_text(findStmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);

    const bool found = (sqlite3_step(findStmt) == SQLITE_ROW);
    if (found) {
        outTaskId = sqlite3_column_int64(findStmt, 0);
    }

    sqlite3_finalize(findStmt);
    return found;
}

// Обновляет задачу в базе данных по ее id.
// Можно изменить название и статус задачи.
// Возвращает true, если обновление выполнено успешно.
inline bool updateTaskInDb(sqlite3* dbHandle, sqlite3_int64 taskId, 
    const std::string& newTitle, TaskStatus newStatus, 
    int newDeadline, const std::string& newDescription) {
    const char* updateSql = "UPDATE tasks SET title = ?, status = ?, deadline = ?, description = ? WHERE id = ?;";
    sqlite3_stmt* updateStmt = nullptr;

    if (sqlite3_prepare_v2(dbHandle, updateSql, -1, &updateStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Ошибка подготовки UPDATE tasks: " << sqlite3_errmsg(dbHandle) << std::endl;
        return false;
    }

    sqlite3_bind_text(updateStmt, 1, newTitle.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(updateStmt, 2, static_cast<int>(newStatus));
    sqlite3_bind_int(updateStmt, 3, newDeadline);
    sqlite3_bind_text(updateStmt, 4, newDescription.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(updateStmt, 5, taskId);

    const bool ok = (sqlite3_step(updateStmt) == SQLITE_DONE);
    if (!ok) {
        std::cerr << "Ошибка обновления задачи: " << sqlite3_errmsg(dbHandle) << std::endl;
    }

    sqlite3_finalize(updateStmt);
    return ok;
}

// Удаляет задачу из базы данных по ее id.
// Возвращает true, если удаление выполнено успешно.
inline bool deleteTaskFromDb(sqlite3* dbHandle, sqlite3_int64 taskId) {
    const char* deleteSql = "DELETE FROM tasks WHERE id = ?;";
    sqlite3_stmt* deleteStmt = nullptr;

    if (sqlite3_prepare_v2(dbHandle, deleteSql, -1, &deleteStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Ошибка подготовки DELETE tasks: " << sqlite3_errmsg(dbHandle) << std::endl;
        return false;
    }

    sqlite3_bind_int64(deleteStmt, 1, taskId);

    const bool ok = (sqlite3_step(deleteStmt) == SQLITE_DONE);
    if (!ok) {
        std::cerr << "Ошибка удаления задачи: " << sqlite3_errmsg(dbHandle) << std::endl;
    }

    sqlite3_finalize(deleteStmt);
    return ok;
}

// Полный сценарий работы с БД: открыть, включить foreign keys, создать схему,
// сохранить проект и задачи, вывести результат и закрыть соединение.
inline bool runProjectFlow(const Project& project, const char* dbPath = "course.db") {
    sqlite3* dbHandle = nullptr;
    if (sqlite3_open(dbPath, &dbHandle) != SQLITE_OK) {
        std::cerr << "Ошибка открытия БД: " << sqlite3_errmsg(dbHandle) << std::endl;
        sqlite3_close(dbHandle);
        return false;
    }

    if (!execSql(dbHandle, "PRAGMA foreign_keys = ON;", "Ошибка включения foreign keys: ")) {
        sqlite3_close(dbHandle);
        return false;
    }

    if (!initSchema(dbHandle)) {
        sqlite3_close(dbHandle);
        return false;
    }

    sqlite3_int64 dbProjectId = 0;
    if (!saveProjectWithTasks(dbHandle, project, dbProjectId)) {
        sqlite3_close(dbHandle);
        return false;
    }

    if (!printTasksFromDb(dbHandle, dbProjectId)) {
        sqlite3_close(dbHandle);
        return false;
    }

    sqlite3_close(dbHandle);
    return true;
}

} // namespace db
