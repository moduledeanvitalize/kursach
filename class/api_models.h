#pragma once

#include <string>
#include <vector>

#include "sqlite3.h"

// Структура, представляющая проект с его основными свойствами (ID, название, описание, даты)
struct ApiProject {
    sqlite3_int64 id = 0;
    std::string name;
    std::string description;
    int startDate = 0;
    int endDate = 0;
    int createdAt = 0;
};

// Структура, представляющая задачу с информацией о проекте, статусе, исполнителе, сроках и зависимостях
struct ApiTask {
    sqlite3_int64 id = 0;
    sqlite3_int64 projectId = 0;
    std::string title;
    std::string description;
    std::string assignee;
    int status = 0;
    int startDate = 0;
    int endDate = 0;
    int progress = 0;
    sqlite3_int64 parentTaskId = 0;
    std::vector<sqlite3_int64> dependencies;
};
