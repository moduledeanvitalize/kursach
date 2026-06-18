#pragma once
#include <string>

// Перечисление статусов задачи (четыре основных состояния)
enum class TaskStatus {
    Todo,
    InProgress,
    UnderInspection,
    Done
};

// Класс, представляющий одну задачу с информацией о проекте, названии, статусе и дедлайне
class Task {
    int id;
    int projectId;
    std::string title;
    TaskStatus status;
    int deadline;
    std::string description;
public:
    // Конструктор для инициализации задачи с ID, ID проекта, названием и опциональным статусом
    Task(int id, int projectId, const std::string& title, TaskStatus status = TaskStatus::Todo)
        : id(id), projectId(projectId), title(title), status(status), deadline(0) {}

    // Получает ID задачи
    int getId() const { return id; }
    
    // Получает ID проекта, к которому относится задача
    int getProjectId() const { return projectId; }
    
    // Получает название задачи
    const std::string& getTitle() const { return title; }
    
    // Получает текущий статус задачи
    TaskStatus getStatus() const { return status; }
    
    // Получает дедлайн (дата крайнего срока) задачи
    int getDeadline() const { return deadline; }
    
    // Получает описание задачи
    const std::string& getDescription() const { return description; }

    // Устанавливает новый статус для задачи
    void setStatus(TaskStatus newStatus) { status = newStatus; }

    // Преобразует статус задачи в человеко-читаемую строку на русском языке
    static std::string statusToString(TaskStatus status) {
        switch (status) {
            case TaskStatus::Todo: return "В разработке";
            case TaskStatus::InProgress: return "В процессе";
            case TaskStatus::UnderInspection: return "На проверке";
            case TaskStatus::Done: return "Выполнено";
        }
        return "Unknown";
    }
};
