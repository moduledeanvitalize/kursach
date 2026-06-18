#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../httplib.h"
#include "api_json.h"
#include "api_repository.h"
#include "server_common.h"

namespace api_routes {

// Регистрирует все API маршруты сервера для работы с проектами, задачами, зависимостями, 
// а также для получения аналитики (Gantt диаграмма, метрики проекта, уведомления, события)
// и раздачи статических файлов фронтенда
inline void registerRoutes(
    httplib::Server& svr,
    sqlite3* dbHandle,
    sqlite3_int64 defaultProjectId,
    const std::string& dbPath,
    int serverPid
) {
    svr.Get("/", [=](const httplib::Request&, httplib::Response& res) {
        server::serveStaticFile(res, "./front/index.html", "text/html; charset=UTF-8", "Не удалось открыть front/index.html");
    });
    svr.Get("/index.html", [=](const httplib::Request&, httplib::Response& res) {
        server::serveStaticFile(res, "./front/index.html", "text/html; charset=UTF-8", "Не удалось открыть front/index.html");
    });
    svr.Get("/style.css", [=](const httplib::Request&, httplib::Response& res) {
        server::serveStaticFile(res, "./front/style.css", "text/css; charset=UTF-8", "Не удалось открыть front/style.css");
    });
    svr.Get("/script.js", [=](const httplib::Request&, httplib::Response& res) {
        server::serveStaticFile(res, "./front/script.js", "application/javascript; charset=UTF-8", "Не удалось открыть front/script.js");
    });

    svr.Get("/api/projects", [=](const httplib::Request&, httplib::Response& res) {
        std::vector<ApiProject> projects;
        if (!api_repo::loadProjects(dbHandle, projects)) {
            api_json::setJson(res, 500, "{\"error\":\"db read failed\"}");
            return;
        }
        api_json::setJson(res, 200, api_json::projectsArrayJson(projects));
    });

    svr.Post("/api/projects", [=](const httplib::Request& req, httplib::Response& res) {
        json_body::Object payload;
        std::string error;
        if (!api_json::parseBody(req.body, payload, error)) {
            api_json::setJson(res, 400, api_json::validationError(error));
            return;
        }

        std::string name;
        std::string description;
        int startDate = api_json::nowTs();
        int endDate = startDate + 7 * 86400;

        if (!api_json::extractString(payload, "name", name) || name.empty()) {
            api_json::setJson(res, 400, api_json::validationError("Заполните название проекта"));
            return;
        }
        api_json::extractString(payload, "description", description);
        api_json::extractInt(payload, "start_date", startDate);
        api_json::extractInt(payload, "end_date", endDate);
        if (endDate < startDate) {
            api_json::setJson(res, 400, api_json::validationError("Дата окончания не может быть раньше даты начала"));
            return;
        }

        sqlite3_int64 projectId = 0;
        if (!api_repo::createProject(dbHandle, name, description, startDate, endDate, projectId)) {
            api_json::setJson(res, 500, "{\"error\":\"db write failed\"}");
            return;
        }

        ApiProject p;
        if (!api_repo::loadProject(dbHandle, projectId, p)) {
            api_json::setJson(res, 500, "{\"error\":\"db read failed\"}");
            return;
        }

        api_json::setJson(res, 201, api_json::projectToJson(p));
    });

    svr.Get(R"(/api/projects/(\d+))", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 projectId = std::stoll(req.matches[1]);
        ApiProject p;
        if (!api_repo::loadProject(dbHandle, projectId, p)) {
            api_json::setJson(res, 404, "{\"error\":\"project not found\"}");
            return;
        }
        api_json::setJson(res, 200, api_json::projectToJson(p));
    });

    svr.Put(R"(/api/projects/(\d+))", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 projectId = std::stoll(req.matches[1]);
        ApiProject current;
        if (!api_repo::loadProject(dbHandle, projectId, current)) {
            api_json::setJson(res, 404, "{\"error\":\"project not found\"}");
            return;
        }

        json_body::Object payload;
        std::string error;
        if (!api_json::parseBody(req.body, payload, error)) {
            api_json::setJson(res, 400, api_json::validationError(error));
            return;
        }

        std::string name = current.name;
        std::string description = current.description;
        int startDate = current.startDate;
        int endDate = current.endDate;

        api_json::extractString(payload, "name", name);
        api_json::extractString(payload, "description", description);
        api_json::extractInt(payload, "start_date", startDate);
        api_json::extractInt(payload, "end_date", endDate);

        if (name.empty()) {
            api_json::setJson(res, 400, api_json::validationError("Заполните название проекта"));
            return;
        }
        if (endDate < startDate) {
            api_json::setJson(res, 400, api_json::validationError("Дата окончания не может быть раньше даты начала"));
            return;
        }

        if (!api_repo::updateProject(dbHandle, projectId, name, description, startDate, endDate)) {
            api_json::setJson(res, 500, "{\"error\":\"db update failed\"}");
            return;
        }

        ApiProject updated;
        api_repo::loadProject(dbHandle, projectId, updated);
        api_json::setJson(res, 200, api_json::projectToJson(updated));
    });

    svr.Delete(R"(/api/projects/(\d+))", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 projectId = std::stoll(req.matches[1]);
        if (!api_repo::deleteProject(dbHandle, projectId)) {
            api_json::setJson(res, 500, "{\"error\":\"project delete failed\"}");
            return;
        }
        api_json::setJson(res, 200, "{\"ok\":true}");
    });

    svr.Get(R"(/api/projects/(\d+)/tasks)", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 projectId = std::stoll(req.matches[1]);
        std::vector<ApiTask> tasks;
        if (!api_repo::loadTasksByProject(dbHandle, projectId, tasks)) {
            api_json::setJson(res, 500, "{\"error\":\"db read failed\"}");
            return;
        }
        api_json::setJson(res, 200, api_json::tasksArrayJson(tasks));
    });

    svr.Post(R"(/api/projects/(\d+)/tasks)", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 projectId = std::stoll(req.matches[1]);
        json_body::Object payload;
        std::string parseError;
        if (!api_json::parseBody(req.body, payload, parseError)) {
            api_json::setJson(res, 400, api_json::validationError(parseError));
            return;
        }

        std::string title;
        std::string description;
        std::string assignee;
        int status = 0;
        int startDate = api_json::nowTs();
        int endDate = startDate + 86400;
        int progress = 0;
        int parentTaskId = 0;

        if (!api_json::extractString(payload, "title", title) || title.empty()) {
            api_json::setJson(res, 400, api_json::validationError("Заполните название задачи"));
            return;
        }

        api_json::extractString(payload, "description", description);
        api_json::extractString(payload, "assignee", assignee);
        api_json::extractInt(payload, "status", status);
        api_json::extractInt(payload, "start_date", startDate);
        api_json::extractInt(payload, "end_date", endDate);
        api_json::extractInt(payload, "progress", progress);
        api_json::extractInt(payload, "parent_task_id", parentTaskId);

        status = std::max(0, std::min(3, status));
        progress = std::max(0, std::min(100, progress));
        if (status == 3) {
            progress = 100;
        }
        if (endDate < startDate) {
            api_json::setJson(res, 400, api_json::validationError("Дата окончания не может быть раньше даты начала"));
            return;
        }

        sqlite3_int64 taskId = 0;
        if (!api_repo::createTask(dbHandle, projectId, title, description, assignee, status, startDate, endDate, progress, parentTaskId, taskId)) {
            api_json::setJson(res, 500, "{\"error\":\"db write failed\"}");
            return;
        }

        api_repo::logTaskCommand(dbHandle, taskId, "create", "{\"title\":\"" + api_json::escape(title) + "\"}");
        if (!assignee.empty()) {
            api_repo::addNotification(dbHandle, taskId, assignee, "assignment", "Вам назначена задача: " + title);
        }

        ApiTask task;
        if (!api_repo::loadTask(dbHandle, taskId, task)) {
            api_json::setJson(res, 500, "{\"error\":\"db read failed\"}");
            return;
        }

        api_json::setJson(res, 201, api_json::taskToJson(task));
    });

    svr.Get(R"(/api/tasks/(\d+))", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 taskId = std::stoll(req.matches[1]);
        ApiTask task;
        if (!api_repo::loadTask(dbHandle, taskId, task)) {
            api_json::setJson(res, 404, "{\"error\":\"task not found\"}");
            return;
        }
        api_json::setJson(res, 200, api_json::taskToJson(task));
    });

    svr.Put(R"(/api/tasks/(\d+))", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 taskId = std::stoll(req.matches[1]);
        ApiTask task;
        if (!api_repo::loadTask(dbHandle, taskId, task)) {
            api_json::setJson(res, 404, "{\"error\":\"task not found\"}");
            return;
        }

        json_body::Object payload;
        std::string parseError;
        if (!api_json::parseBody(req.body, payload, parseError)) {
            api_json::setJson(res, 400, api_json::validationError(parseError));
            return;
        }

        const std::string oldAssignee = task.assignee;

        api_json::extractString(payload, "title", task.title);
        api_json::extractString(payload, "description", task.description);
        api_json::extractString(payload, "assignee", task.assignee);
        api_json::extractInt(payload, "status", task.status);
        api_json::extractInt(payload, "start_date", task.startDate);
        api_json::extractInt(payload, "end_date", task.endDate);
        api_json::extractInt(payload, "progress", task.progress);

        task.status = std::max(0, std::min(3, task.status));
        task.progress = std::max(0, std::min(100, task.progress));
        if (task.status == 3) {
            task.progress = 100;
        }

        if (task.title.empty()) {
            api_json::setJson(res, 400, api_json::validationError("Заполните название задачи"));
            return;
        }
        if (task.endDate < task.startDate) {
            api_json::setJson(res, 400, api_json::validationError("Дата окончания не может быть раньше даты начала"));
            return;
        }

        if (!api_repo::updateTask(dbHandle, task)) {
            api_json::setJson(res, 500, "{\"error\":\"task update failed\"}");
            return;
        }

        api_repo::logTaskCommand(dbHandle, task.id, "update", "{\"status\":" + std::to_string(task.status) + "}");

        if (!task.assignee.empty() && task.assignee != oldAssignee) {
            api_repo::addNotification(dbHandle, task.id, task.assignee, "assignment", "Вам переназначена задача: " + task.title);
        }
        if (!task.assignee.empty() && task.endDate > 0 && task.endDate - api_json::nowTs() < 2 * 86400 && task.status != 3) {
            api_repo::addNotification(dbHandle, task.id, task.assignee, "deadline", "Приближается дедлайн по задаче: " + task.title);
        }

        ApiTask updated;
        api_repo::loadTask(dbHandle, task.id, updated);
        api_json::setJson(res, 200, api_json::taskToJson(updated));
    });

    svr.Post(R"(/api/tasks/(\d+)/move)", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 taskId = std::stoll(req.matches[1]);
        ApiTask task;
        if (!api_repo::loadTask(dbHandle, taskId, task)) {
            api_json::setJson(res, 404, "{\"error\":\"task not found\"}");
            return;
        }

        json_body::Object payload;
        std::string parseError;
        if (!api_json::parseBody(req.body, payload, parseError)) {
            api_json::setJson(res, 400, api_json::validationError(parseError));
            return;
        }

        int status = task.status;
        if (!api_json::extractInt(payload, "status", status)) {
            api_json::setJson(res, 400, api_json::validationError("Выберите статус задачи"));
            return;
        }
        task.status = std::max(0, std::min(3, status));
        if (task.status == 3) {
            task.progress = 100;
        }

        if (!api_repo::updateTask(dbHandle, task)) {
            api_json::setJson(res, 500, "{\"error\":\"task move failed\"}");
            return;
        }
        api_repo::logTaskCommand(dbHandle, task.id, "move", "{\"status\":" + std::to_string(task.status) + "}");

        ApiTask updated;
        api_repo::loadTask(dbHandle, task.id, updated);
        api_json::setJson(res, 200, api_json::taskToJson(updated));
    });

    svr.Delete(R"(/api/tasks/(\d+))", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 taskId = std::stoll(req.matches[1]);
        if (!api_repo::deleteTask(dbHandle, taskId)) {
            api_json::setJson(res, 500, "{\"error\":\"task delete failed\"}");
            return;
        }
        api_json::setJson(res, 200, "{\"ok\":true}");
    });

    svr.Post(R"(/api/tasks/(\d+)/dependencies)", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 taskId = std::stoll(req.matches[1]);
        json_body::Object payload;
        std::string parseError;
        if (!api_json::parseBody(req.body, payload, parseError)) {
            api_json::setJson(res, 400, api_json::validationError(parseError));
            return;
        }
        int depId = 0;
        if (!api_json::extractInt(payload, "depends_on_task_id", depId) || depId <= 0) {
            api_json::setJson(res, 400, api_json::validationError("Выберите задачу, от которой зависит текущая"));
            return;
        }
        if (taskId == depId) {
            api_json::setJson(res, 400, api_json::validationError("Нельзя добавить зависимость на саму задачу"));
            return;
        }

        if (!api_repo::addDependency(dbHandle, taskId, depId)) {
            api_json::setJson(res, 500, "{\"error\":\"dependency add failed\"}");
            return;
        }

        api_repo::logTaskCommand(dbHandle, taskId, "add_dependency", "{\"depends_on_task_id\":" + std::to_string(depId) + "}");

        ApiTask task;
        if (!api_repo::loadTask(dbHandle, taskId, task)) {
            api_json::setJson(res, 404, "{\"error\":\"task not found\"}");
            return;
        }
        api_json::setJson(res, 200, api_json::taskToJson(task));
    });

    svr.Delete(R"(/api/tasks/(\d+)/dependencies/(\d+))", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 taskId = std::stoll(req.matches[1]);
        const sqlite3_int64 depId = std::stoll(req.matches[2]);
        if (!api_repo::removeDependency(dbHandle, taskId, depId)) {
            api_json::setJson(res, 500, "{\"error\":\"dependency delete failed\"}");
            return;
        }
        api_repo::logTaskCommand(dbHandle, taskId, "remove_dependency", "{\"depends_on_task_id\":" + std::to_string(depId) + "}");
        api_json::setJson(res, 200, "{\"ok\":true}");
    });

    svr.Get(R"(/api/projects/(\d+)/gantt)", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 projectId = std::stoll(req.matches[1]);
        std::vector<ApiTask> tasks;
        if (!api_repo::loadTasksByProject(dbHandle, projectId, tasks)) {
            api_json::setJson(res, 500, "{\"error\":\"db read failed\"}");
            return;
        }

        const std::vector<sqlite3_int64> criticalPath = api_repo::computeCriticalPath(tasks);

        std::set<sqlite3_int64> bottlenecks;
        for (const auto& t : tasks) {
            if (t.status != 3 && t.endDate > 0 && t.endDate < api_json::nowTs()) {
                bottlenecks.insert(t.id);
            }
            if (t.dependencies.size() >= 2) {
                bottlenecks.insert(t.id);
            }
        }

        std::string criticalJson = "[";
        for (size_t i = 0; i < criticalPath.size(); ++i) {
            criticalJson += std::to_string(criticalPath[i]);
            if (i + 1 < criticalPath.size()) {
                criticalJson += ",";
            }
        }
        criticalJson += "]";

        std::string bottlenecksJson = "[";
        size_t idx = 0;
        for (sqlite3_int64 id : bottlenecks) {
            bottlenecksJson += std::to_string(id);
            if (idx + 1 < bottlenecks.size()) {
                bottlenecksJson += ",";
            }
            ++idx;
        }
        bottlenecksJson += "]";

        const std::string body =
            "{\"tasks\":" + api_json::tasksArrayJson(tasks)
            + ",\"critical_path\":" + criticalJson
            + ",\"bottlenecks\":" + bottlenecksJson
            + "}";

        api_json::setJson(res, 200, body);
    });

    svr.Get(R"(/api/projects/(\d+)/analytics)", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 projectId = std::stoll(req.matches[1]);
        std::vector<ApiTask> tasks;
        if (!api_repo::loadTasksByProject(dbHandle, projectId, tasks)) {
            api_json::setJson(res, 500, "{\"error\":\"db read failed\"}");
            return;
        }

        const int tsNow = api_json::nowTs();
        int done = 0;
        int overdue = 0;
        double progressSum = 0.0;
        int latestEnd = 0;
        std::map<std::string, int> loadByAssignee;

        for (const auto& t : tasks) {
            progressSum += t.progress;
            if (t.status == 3) {
                done += 1;
            }
            if (t.status != 3 && t.endDate > 0 && t.endDate < tsNow) {
                overdue += 1;
            }
            latestEnd = std::max(latestEnd, t.endDate);
            const std::string key = t.assignee.empty() ? "(не назначено)" : t.assignee;
            loadByAssignee[key] += 1;
        }

        const int total = static_cast<int>(tasks.size());
        const int percentDone = (total == 0) ? 0 : static_cast<int>((done * 100.0) / total);
        const int avgProgress = (total == 0) ? 0 : static_cast<int>(progressSum / total);
        const int forecast = latestEnd + overdue * 43200;

        std::string assigneeLoadJson = "[";
        size_t pos = 0;
        for (const auto& item : loadByAssignee) {
            assigneeLoadJson += "{\"assignee\":\"" + api_json::escape(item.first)
                + "\",\"tasks\":" + std::to_string(item.second) + "}";
            if (pos + 1 < loadByAssignee.size()) {
                assigneeLoadJson += ",";
            }
            ++pos;
        }
        assigneeLoadJson += "]";

        const std::string body =
            "{\"total_tasks\":" + std::to_string(total)
            + ",\"done_tasks\":" + std::to_string(done)
            + ",\"percent_done\":" + std::to_string(percentDone)
            + ",\"avg_progress\":" + std::to_string(avgProgress)
            + ",\"overdue_tasks\":" + std::to_string(overdue)
            + ",\"forecast_completion\":" + std::to_string(forecast)
            + ",\"assignee_load\":" + assigneeLoadJson
            + "}";

        api_json::setJson(res, 200, body);
    });

    svr.Get(R"(/api/projects/(\d+)/notifications)", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 projectId = std::stoll(req.matches[1]);
        const char* sql =
            "SELECT n.id, n.task_id, n.assignee, n.kind, n.message, n.is_read, n.created_at "
            "FROM notifications n "
            "JOIN tasks t ON t.id = n.task_id "
            "WHERE t.project_id = ? "
            "ORDER BY n.created_at DESC, n.id DESC LIMIT 200;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(dbHandle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            api_json::setJson(res, 500, "{\"error\":\"db read failed\"}");
            return;
        }
        sqlite3_bind_int64(stmt, 1, projectId);

        std::string body = "[";
        bool first = true;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (!first) {
                body += ",";
            }
            first = false;
            const char* assignee = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* kind = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const char* message = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

            body += "{\"id\":" + std::to_string(sqlite3_column_int64(stmt, 0))
                + ",\"task_id\":" + std::to_string(sqlite3_column_int64(stmt, 1))
                + ",\"assignee\":\"" + api_json::escape(assignee ? assignee : "")
                + "\",\"kind\":\"" + api_json::escape(kind ? kind : "")
                + "\",\"message\":\"" + api_json::escape(message ? message : "")
                + "\",\"is_read\":" + std::to_string(sqlite3_column_int(stmt, 5))
                + ",\"created_at\":" + std::to_string(sqlite3_column_int(stmt, 6))
                + "}";
        }
        sqlite3_finalize(stmt);
        body += "]";
        api_json::setJson(res, 200, body);
    });

    svr.Get(R"(/api/projects/(\d+)/commands)", [=](const httplib::Request& req, httplib::Response& res) {
        const sqlite3_int64 projectId = std::stoll(req.matches[1]);
        const char* sql =
            "SELECT e.id, e.task_id, e.command_type, e.payload, e.created_at "
            "FROM task_events e "
            "JOIN tasks t ON t.id = e.task_id "
            "WHERE t.project_id = ? "
            "ORDER BY e.created_at DESC, e.id DESC LIMIT 300;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(dbHandle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            api_json::setJson(res, 500, "{\"error\":\"db read failed\"}");
            return;
        }
        sqlite3_bind_int64(stmt, 1, projectId);

        std::string body = "[";
        bool first = true;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (!first) {
                body += ",";
            }
            first = false;
            const char* commandType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* payload = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            body += "{\"id\":" + std::to_string(sqlite3_column_int64(stmt, 0))
                + ",\"task_id\":" + std::to_string(sqlite3_column_int64(stmt, 1))
                + ",\"command_type\":\"" + api_json::escape(commandType ? commandType : "")
                + "\",\"payload\":\"" + api_json::escape(payload ? payload : "")
                + "\",\"created_at\":" + std::to_string(sqlite3_column_int(stmt, 4))
                + "}";
        }
        sqlite3_finalize(stmt);
        body += "]";
        api_json::setJson(res, 200, body);
    });

    svr.Get("/tasks", [=](const httplib::Request&, httplib::Response& res) {
        std::vector<ApiTask> tasks;
        if (!api_repo::loadTasksByProject(dbHandle, defaultProjectId, tasks)) {
            api_json::setJson(res, 500, "{\"error\":\"db read failed\"}");
            return;
        }
        api_json::setJson(res, 200, api_json::tasksArrayJson(tasks));
    });

    svr.Get("/debug/source", [=](const httplib::Request&, httplib::Response& res) {
        const std::string body =
            "{\"mode\":\"sqlite\",\"pid\":" + std::to_string(serverPid)
            + ",\"dbPath\":\"" + api_json::escape(dbPath) + "\"}";
        api_json::setJson(res, 200, body);
    });
}

} // namespace api_routes
