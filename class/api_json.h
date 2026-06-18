#pragma once

#include <ctime>
#include <string>
#include <vector>

#include "../httplib.h"
#include "api_models.h"
#include "json_body.h"
#include "server_common.h"

namespace api_json {

// Получает текущее время в формате Unix timestamp (секунды с 1970 года)
inline int nowTs() {
    return static_cast<int>(std::time(nullptr));
}

// Экранирует специальные символы в строке для корректного представления в JSON
// (кавычки, слэши, переводы строк, табуляции)
inline std::string escape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char ch : value) {
        if (ch == '\\') {
            out += "\\\\";
        } else if (ch == '"') {
            out += "\\\"";
        } else if (ch == '\n') {
            out += "\\n";
        } else if (ch == '\r') {
            out += "\\r";
        } else if (ch == '\t') {
            out += "\\t";
        } else {
            out += ch;
        }
    }
    return out;
}

// Устанавливает JSON ответ с указанным статусом и телом, отключив кеширование браузером
inline void setJson(httplib::Response& res, int status, const std::string& body) {
    res.status = status;
    server::setNoCacheHeaders(res);
    res.set_content(body, "application/json; charset=UTF-8");
}

// Парсит JSON тело запроса в объект, возвращает false и ошибку при неудаче
inline bool parseBody(const std::string& body, json_body::Object& out, std::string& error) {
    return json_body::parseObject(body, out, error);
}

// Извлекает строковое значение из JSON объекта по ключу
inline bool extractString(const json_body::Object& body, const std::string& key, std::string& out) {
    return body.getString(key, out);
}

// Извлекает целочисленное значение из JSON объекта по ключу
inline bool extractInt(const json_body::Object& body, const std::string& key, int& out) {
    return body.getInt(key, out);
}

// Извлекает булево значение из JSON объекта по ключу
inline bool extractBool(const json_body::Object& body, const std::string& key, bool& out) {
    return body.getBool(key, out);
}

// Создает JSON строку ошибки валидации с экранированным сообщением
inline std::string validationError(const std::string& message) {
    return "{\"error\":\"" + escape(message) + "\"}";
}

// Парсит JSON payload задачи и извлекает все поля (название, описание, исполнитель, статус, даты, прогресс, родительская задача)
inline bool parseTaskPayload(
    const std::string& bodyText,
    std::string& outTitle,
    std::string& outDescription,
    std::string& outAssignee,
    int& outStatus,
    int& outStartDate,
    int& outEndDate,
    int& outProgress,
    int& outParentTaskId,
    std::string& error
) {
    json_body::Object body;
    if (!parseBody(bodyText, body, error)) {
        return false;
    }

    outTitle.clear();
    outDescription.clear();
    outAssignee.clear();
    outStatus = 0;
    outStartDate = nowTs();
    outEndDate = outStartDate + 86400;
    outProgress = 0;
    outParentTaskId = 0;

    if (body.has("title")) {
        if (!extractString(body, "title", outTitle)) {
            error = "Поле title должно быть строкой";
            return false;
        }
    }
    if (body.has("description")) {
        if (!extractString(body, "description", outDescription)) {
            error = "Поле description должно быть строкой";
            return false;
        }
    }
    if (body.has("assignee")) {
        if (!extractString(body, "assignee", outAssignee)) {
            error = "Поле assignee должно быть строкой";
            return false;
        }
    }
    if (body.has("status")) {
        if (!extractInt(body, "status", outStatus)) {
            error = "Поле status должно быть числом";
            return false;
        }
    }
    if (body.has("start_date")) {
        if (!extractInt(body, "start_date", outStartDate)) {
            error = "Поле start_date должно быть числом";
            return false;
        }
    }
    if (body.has("end_date")) {
        if (!extractInt(body, "end_date", outEndDate)) {
            error = "Поле end_date должно быть числом";
            return false;
        }
    }
    if (body.has("progress")) {
        if (!extractInt(body, "progress", outProgress)) {
            error = "Поле progress должно быть числом";
            return false;
        }
    }
    if (body.has("parent_task_id")) {
        if (!extractInt(body, "parent_task_id", outParentTaskId)) {
            error = "Поле parent_task_id должно быть числом";
            return false;
        }
    }

    return true;
}

// Парсит JSON payload проекта и извлекает поля (название, описание, даты начала и конца)
inline bool parseProjectPayload(
    const std::string& bodyText,
    std::string& outName,
    std::string& outDescription,
    int& outStartDate,
    int& outEndDate,
    std::string& error
) {
    json_body::Object body;
    if (!parseBody(bodyText, body, error)) {
        return false;
    }

    outName.clear();
    outDescription.clear();
    outStartDate = nowTs();
    outEndDate = outStartDate + 7 * 86400;

    if (body.has("name") && !extractString(body, "name", outName)) {
        error = "Поле name должно быть строкой";
        return false;
    }
    if (body.has("description") && !extractString(body, "description", outDescription)) {
        error = "Поле description должно быть строкой";
        return false;
    }
    if (body.has("start_date") && !extractInt(body, "start_date", outStartDate)) {
        error = "Поле start_date должно быть числом";
        return false;
    }
    if (body.has("end_date") && !extractInt(body, "end_date", outEndDate)) {
        error = "Поле end_date должно быть числом";
        return false;
    }

    return true;
}

// Преобразует объект проекта в JSON строку для отправки клиенту
inline std::string projectToJson(const ApiProject& p) {
    return "{\"id\":" + std::to_string(p.id)
        + ",\"name\":\"" + escape(p.name)
        + "\",\"description\":\"" + escape(p.description)
        + "\",\"start_date\":" + std::to_string(p.startDate)
        + ",\"end_date\":" + std::to_string(p.endDate)
        + ",\"created_at\":" + std::to_string(p.createdAt)
        + "}";
}

// Преобразует объект задачи в JSON строку, включая все её зависимости
inline std::string taskToJson(const ApiTask& t) {
    std::string deps = "[";
    for (size_t i = 0; i < t.dependencies.size(); ++i) {
        deps += std::to_string(t.dependencies[i]);
        if (i + 1 < t.dependencies.size()) {
            deps += ",";
        }
    }
    deps += "]";

    return "{\"id\":" + std::to_string(t.id)
        + ",\"project_id\":" + std::to_string(t.projectId)
        + ",\"title\":\"" + escape(t.title)
        + "\",\"description\":\"" + escape(t.description)
        + "\",\"assignee\":\"" + escape(t.assignee)
        + "\",\"status\":" + std::to_string(t.status)
        + ",\"start_date\":" + std::to_string(t.startDate)
        + ",\"end_date\":" + std::to_string(t.endDate)
        + ",\"progress\":" + std::to_string(t.progress)
        + ",\"parent_task_id\":" + std::to_string(t.parentTaskId)
        + ",\"dependencies\":" + deps
        + "}";
}

// Преобразует вектор задач в JSON массив для отправки клиенту
inline std::string tasksArrayJson(const std::vector<ApiTask>& tasks) {
    std::string body = "[";
    for (size_t i = 0; i < tasks.size(); ++i) {
        body += taskToJson(tasks[i]);
        if (i + 1 < tasks.size()) {
            body += ",";
        }
    }
    body += "]";
    return body;
}

// Преобразует вектор проектов в JSON массив для отправки клиенту
inline std::string projectsArrayJson(const std::vector<ApiProject>& projects) {
    std::string body = "[";
    for (size_t i = 0; i < projects.size(); ++i) {
        body += projectToJson(projects[i]);
        if (i + 1 < projects.size()) {
            body += ",";
        }
    }
    body += "]";
    return body;
}

} // namespace api_json
