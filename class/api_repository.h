#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <sqlite3.h>

#include "api_json.h"
#include "api_models.h"
#include "database.h"

namespace api_repo {

// Загружает из БД все зависимости задачи (список ID задач, от которых эта задача зависит)
inline bool loadTaskDependencies(sqlite3* db, sqlite3_int64 taskId, std::vector<sqlite3_int64>& outDeps) {
    outDeps.clear();
    const char* sql = "SELECT depends_on_task_id FROM task_dependencies WHERE task_id = ? ORDER BY depends_on_task_id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, taskId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        outDeps.push_back(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return true;
}

// Сохраняет события (команды) которые были выполнены над задачей (создание, обновление, перемещение)
inline bool logTaskCommand(sqlite3* db, sqlite3_int64 taskId, const std::string& commandType, const std::string& payload) {
    const char* sql = "INSERT INTO task_events(task_id, command_type, payload) VALUES(?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int64(stmt, 1, taskId);
    sqlite3_bind_text(stmt, 2, commandType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, payload.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// Добавляет уведомление для исполнителя о событиях задачи (назначение, приближение дедлайна)
inline bool addNotification(sqlite3* db, sqlite3_int64 taskId, const std::string& assignee, const std::string& kind, const std::string& message) {
    const char* sql = "INSERT INTO notifications(task_id, assignee, kind, message) VALUES(?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int64(stmt, 1, taskId);
    sqlite3_bind_text(stmt, 2, assignee.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, kind.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, message.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// Загружает один проект из БД по его ID
inline bool loadProject(sqlite3* db, sqlite3_int64 projectId, ApiProject& outProject) {
    const char* sql = "SELECT id, name, description, start_date, end_date, created_at FROM projects WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int64(stmt, 1, projectId);
    const bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    if (found) {
        outProject.id = sqlite3_column_int64(stmt, 0);
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        outProject.name = name ? name : "";
        outProject.description = desc ? desc : "";
        outProject.startDate = sqlite3_column_int(stmt, 3);
        outProject.endDate = sqlite3_column_int(stmt, 4);
        outProject.createdAt = sqlite3_column_int(stmt, 5);
    }
    sqlite3_finalize(stmt);
    return found;
}

// Загружает все проекты из БД
inline bool loadProjects(sqlite3* db, std::vector<ApiProject>& outProjects) {
    outProjects.clear();
    const char* sql = "SELECT id, name, description, start_date, end_date, created_at FROM projects ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ApiProject p;
        p.id = sqlite3_column_int64(stmt, 0);
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        p.name = name ? name : "";
        p.description = desc ? desc : "";
        p.startDate = sqlite3_column_int(stmt, 3);
        p.endDate = sqlite3_column_int(stmt, 4);
        p.createdAt = sqlite3_column_int(stmt, 5);
        outProjects.push_back(p);
    }
    sqlite3_finalize(stmt);
    return true;
}

// Создает новый проект в БД с указанными параметрами и возвращает его ID
inline bool createProject(sqlite3* db, const std::string& name, const std::string& description, int startDate, int endDate, sqlite3_int64& outProjectId) {
    const char* sql = "INSERT INTO projects(name, description, start_date, end_date, created_at) VALUES(?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, startDate);
    sqlite3_bind_int(stmt, 4, endDate);
    sqlite3_bind_int(stmt, 5, api_json::nowTs());
    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    if (!ok) {
        return false;
    }
    outProjectId = sqlite3_last_insert_rowid(db);
    return true;
}

// Обновляет существующий проект в БД
inline bool updateProject(sqlite3* db, sqlite3_int64 projectId, const std::string& name, const std::string& description, int startDate, int endDate) {
    const char* sql = "UPDATE projects SET name = ?, description = ?, start_date = ?, end_date = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, startDate);
    sqlite3_bind_int(stmt, 4, endDate);
    sqlite3_bind_int64(stmt, 5, projectId);
    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// Удаляет проект из БД вместе со всеми его задачами и зависимостями (выполняется в транзакции)
inline bool deleteProject(sqlite3* db, sqlite3_int64 projectId) {
    if (!::db::execSql(db, "BEGIN;", "Ошибка начала транзакции удаления проекта: ")) {
        return false;
    }
    const char* deleteDepsSql =
        "DELETE FROM task_dependencies WHERE task_id IN (SELECT id FROM tasks WHERE project_id = ?) "
        "OR depends_on_task_id IN (SELECT id FROM tasks WHERE project_id = ?);";
    sqlite3_stmt* depsStmt = nullptr;
    if (sqlite3_prepare_v2(db, deleteDepsSql, -1, &depsStmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    sqlite3_bind_int64(depsStmt, 1, projectId);
    sqlite3_bind_int64(depsStmt, 2, projectId);
    bool ok = (sqlite3_step(depsStmt) == SQLITE_DONE);
    sqlite3_finalize(depsStmt);
    if (!ok) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    const char* deleteTasksSql = "DELETE FROM tasks WHERE project_id = ?;";
    sqlite3_stmt* tasksStmt = nullptr;
    if (sqlite3_prepare_v2(db, deleteTasksSql, -1, &tasksStmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    sqlite3_bind_int64(tasksStmt, 1, projectId);
    ok = (sqlite3_step(tasksStmt) == SQLITE_DONE);
    sqlite3_finalize(tasksStmt);
    if (!ok) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    const char* deleteProjectSql = "DELETE FROM projects WHERE id = ?;";
    sqlite3_stmt* projectStmt = nullptr;
    if (sqlite3_prepare_v2(db, deleteProjectSql, -1, &projectStmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    sqlite3_bind_int64(projectStmt, 1, projectId);
    ok = (sqlite3_step(projectStmt) == SQLITE_DONE);
    sqlite3_finalize(projectStmt);
    if (!ok) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    if (!::db::execSql(db, "COMMIT;", "Ошибка commit удаления проекта: ")) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    return true;
}

// Загружает одну задачу из БД по её ID вместе со всеми её зависимостями
inline bool loadTask(sqlite3* db, sqlite3_int64 taskId, ApiTask& outTask) {
    const char* sql =
        "SELECT id, project_id, title, description, assignee, status, start_date, end_date, progress, COALESCE(parent_task_id, 0) "
        "FROM tasks WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int64(stmt, 1, taskId);
    const bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    if (found) {
        outTask.id = sqlite3_column_int64(stmt, 0);
        outTask.projectId = sqlite3_column_int64(stmt, 1);
        const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* assignee = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        outTask.title = title ? title : "";
        outTask.description = desc ? desc : "";
        outTask.assignee = assignee ? assignee : "";
        outTask.status = sqlite3_column_int(stmt, 5);
        outTask.startDate = sqlite3_column_int(stmt, 6);
        outTask.endDate = sqlite3_column_int(stmt, 7);
        outTask.progress = sqlite3_column_int(stmt, 8);
        outTask.parentTaskId = sqlite3_column_int64(stmt, 9);
    }
    sqlite3_finalize(stmt);
    if (!found) {
        return false;
    }
    return loadTaskDependencies(db, taskId, outTask.dependencies);
}

// Загружает все задачи проекта из БД вместе с их зависимостями
inline bool loadTasksByProject(sqlite3* db, sqlite3_int64 projectId, std::vector<ApiTask>& outTasks) {
    outTasks.clear();
    const char* sql =
        "SELECT id, project_id, title, description, assignee, status, start_date, end_date, progress, COALESCE(parent_task_id, 0) "
        "FROM tasks WHERE project_id = ? ORDER BY start_date, id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int64(stmt, 1, projectId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ApiTask t;
        t.id = sqlite3_column_int64(stmt, 0);
        t.projectId = sqlite3_column_int64(stmt, 1);
        const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* assignee = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        t.title = title ? title : "";
        t.description = desc ? desc : "";
        t.assignee = assignee ? assignee : "";
        t.status = sqlite3_column_int(stmt, 5);
        t.startDate = sqlite3_column_int(stmt, 6);
        t.endDate = sqlite3_column_int(stmt, 7);
        t.progress = sqlite3_column_int(stmt, 8);
        t.parentTaskId = sqlite3_column_int64(stmt, 9);
        outTasks.push_back(t);
    }
    sqlite3_finalize(stmt);

    for (auto& task : outTasks) {
        if (!loadTaskDependencies(db, task.id, task.dependencies)) {
            return false;
        }
    }
    return true;
}

// Создает новую задачу в БД с указанными параметрами и возвращает её ID
inline bool createTask(
    sqlite3* db,
    sqlite3_int64 projectId,
    const std::string& title,
    const std::string& description,
    const std::string& assignee,
    int status,
    int startDate,
    int endDate,
    int progress,
    sqlite3_int64 parentTaskId,
    sqlite3_int64& outTaskId
) {
    const char* sql =
        "INSERT INTO tasks(project_id, title, status, deadline, description, assignee, start_date, end_date, parent_task_id, progress) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int64(stmt, 1, projectId);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, status);
    sqlite3_bind_int(stmt, 4, endDate);
    sqlite3_bind_text(stmt, 5, description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, assignee.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, startDate);
    sqlite3_bind_int(stmt, 8, endDate);
    if (parentTaskId > 0) {
        sqlite3_bind_int64(stmt, 9, parentTaskId);
    } else {
        sqlite3_bind_null(stmt, 9);
    }
    sqlite3_bind_int(stmt, 10, progress);

    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    if (!ok) {
        return false;
    }
    outTaskId = sqlite3_last_insert_rowid(db);
    return true;
}

// Обновляет существующую задачу в БД со всеми её полями
inline bool updateTask(sqlite3* db, const ApiTask& task) {
    const char* sql =
        "UPDATE tasks SET title = ?, description = ?, assignee = ?, status = ?, start_date = ?, end_date = ?, deadline = ?, progress = ?, parent_task_id = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, task.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, task.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, task.assignee.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, task.status);
    sqlite3_bind_int(stmt, 5, task.startDate);
    sqlite3_bind_int(stmt, 6, task.endDate);
    sqlite3_bind_int(stmt, 7, task.endDate);
    sqlite3_bind_int(stmt, 8, task.progress);
    if (task.parentTaskId > 0) {
        sqlite3_bind_int64(stmt, 9, task.parentTaskId);
    } else {
        sqlite3_bind_null(stmt, 9);
    }
    sqlite3_bind_int64(stmt, 10, task.id);
    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// Удаляет задачу из БД вместе со всеми её зависимостями (выполняется в транзакции)
inline bool deleteTask(sqlite3* db, sqlite3_int64 taskId) {
    if (!::db::execSql(db, "BEGIN;", "Ошибка начала транзакции удаления задачи: ")) {
        return false;
    }

    const char* depsSql = "DELETE FROM task_dependencies WHERE task_id = ? OR depends_on_task_id = ?;";
    sqlite3_stmt* depsStmt = nullptr;
    if (sqlite3_prepare_v2(db, depsSql, -1, &depsStmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    sqlite3_bind_int64(depsStmt, 1, taskId);
    sqlite3_bind_int64(depsStmt, 2, taskId);
    bool ok = (sqlite3_step(depsStmt) == SQLITE_DONE);
    sqlite3_finalize(depsStmt);
    if (!ok) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    const char* taskSql = "DELETE FROM tasks WHERE id = ?;";
    sqlite3_stmt* taskStmt = nullptr;
    if (sqlite3_prepare_v2(db, taskSql, -1, &taskStmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    sqlite3_bind_int64(taskStmt, 1, taskId);
    ok = (sqlite3_step(taskStmt) == SQLITE_DONE);
    sqlite3_finalize(taskStmt);
    if (!ok) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    if (!::db::execSql(db, "COMMIT;", "Ошибка commit удаления задачи: ")) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    return true;
}

// Добавляет зависимость между двумя задачами (одна задача зависит от другой)
inline bool addDependency(sqlite3* db, sqlite3_int64 taskId, sqlite3_int64 dependsOnTaskId) {
    const char* sql = "INSERT OR IGNORE INTO task_dependencies(task_id, depends_on_task_id) VALUES(?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int64(stmt, 1, taskId);
    sqlite3_bind_int64(stmt, 2, dependsOnTaskId);
    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// Удаляет зависимость между двумя задачами
inline bool removeDependency(sqlite3* db, sqlite3_int64 taskId, sqlite3_int64 dependsOnTaskId) {
    const char* sql = "DELETE FROM task_dependencies WHERE task_id = ? AND depends_on_task_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int64(stmt, 1, taskId);
    sqlite3_bind_int64(stmt, 2, dependsOnTaskId);
    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// Вычисляет критический путь в графе задач (самый длинный путь от начала до конца проекта)
// используя метод критического пути (CPM) - это важно для определения приоритетных задач
inline std::vector<sqlite3_int64> computeCriticalPath(const std::vector<ApiTask>& tasks) {
    std::unordered_map<sqlite3_int64, ApiTask> byId;
    std::unordered_map<sqlite3_int64, std::vector<sqlite3_int64>> outgoing;
    std::unordered_map<sqlite3_int64, std::vector<sqlite3_int64>> incoming;
    std::unordered_map<sqlite3_int64, int> indegree;

    for (const auto& t : tasks) {
        byId[t.id] = t;
        indegree[t.id] = 0;
    }

    for (const auto& t : tasks) {
        for (sqlite3_int64 dep : t.dependencies) {
            if (byId.find(dep) == byId.end()) {
                continue;
            }
            outgoing[dep].push_back(t.id);
            incoming[t.id].push_back(dep);
            indegree[t.id] += 1;
        }
    }

    std::vector<sqlite3_int64> topo;
    std::vector<sqlite3_int64> q;
    for (const auto& item : indegree) {
        if (item.second == 0) {
            q.push_back(item.first);
        }
    }

    for (size_t i = 0; i < q.size(); ++i) {
        const sqlite3_int64 u = q[i];
        topo.push_back(u);
        for (sqlite3_int64 v : outgoing[u]) {
            indegree[v] -= 1;
            if (indegree[v] == 0) {
                q.push_back(v);
            }
        }
    }

    if (topo.size() != tasks.size()) {
        return {};
    }

    std::unordered_map<sqlite3_int64, int> es;
    std::unordered_map<sqlite3_int64, int> ef;
    std::unordered_map<sqlite3_int64, sqlite3_int64> parent;

    for (sqlite3_int64 id : topo) {
        int maxEf = 0;
        sqlite3_int64 maxPred = 0;
        for (sqlite3_int64 pred : incoming[id]) {
            if (ef[pred] > maxEf) {
                maxEf = ef[pred];
                maxPred = pred;
            }
        }
        es[id] = maxEf;
        const auto& task = byId[id];
        const int duration = std::max(1, task.endDate - task.startDate);
        ef[id] = es[id] + duration;
        parent[id] = maxPred;
    }

    sqlite3_int64 finishId = 0;
    int bestEf = -1;
    for (const auto& item : ef) {
        if (item.second > bestEf) {
            bestEf = item.second;
            finishId = item.first;
        }
    }

    std::vector<sqlite3_int64> path;
    while (finishId != 0) {
        path.push_back(finishId);
        finishId = parent[finishId];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

} // namespace api_repo
