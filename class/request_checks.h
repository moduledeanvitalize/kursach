#pragma once

#include <string>

#include "api_json.h"

namespace server {

// Парсит и проверяет payload при создании новой задачи, извлекая обязательные и опциональные поля
inline bool parseTaskCreatePayload(
    const std::string& body,
    std::string& outTitle,
    int& outStatus,
    int& outDeadline,
    std::string& outDescription
) {
    std::string error;
    std::string assignee;
    int startDate = 0;
    int endDate = 0;
    int progress = 0;
    int parentTaskId = 0;

    if (!api_json::parseTaskPayload(
            body,
            outTitle,
            outDescription,
            assignee,
            outStatus,
            startDate,
            outDeadline,
            progress,
            parentTaskId,
            error)) {
        return false;
    }

    return !outTitle.empty();
}

} // namespace server
