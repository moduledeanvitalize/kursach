#pragma once
#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

#include "task.h"

// Класс, представляющий проект с его ID, названием и списком задач
class Project {
    int id;
    std::string name;
    std::vector<Task> tasks;

public:
    // Конструктор для инициализации проекта с ID и названием
    Project(int id, const std::string& name)
        : id(id), name(name) {}

    ~Project() = default;

    // Добавляет новую задачу в проект с автоматическим ID
    void addTask(const std::string& title) {
        const int nextId = static_cast<int>(tasks.size()) + 1;
        tasks.emplace_back(nextId, id, title);
    }

    // Удаляет задачу по ID, возвращает true если задача была найдена и удалена
    bool removeTask(int taskId) {
        const auto oldSize = tasks.size();
        tasks.erase(std::remove_if(tasks.begin(), tasks.end(),
            [taskId](const Task& task) { return task.getId() == taskId; }), tasks.end());
        return tasks.size() != oldSize;
    }

    // Получает имя проекта
    std::string getName() const { return name; }
    
    // Получает ID проекта
    int getId() const { return id; }

    // Обновляет статус задачи по её ID
    bool updateTaskStatus(int taskId, TaskStatus newStatus) {
        for (auto& task : tasks) {
            if (task.getId() == taskId) {
                task.setStatus(newStatus);
                return true;
            }
        }
        return false;
    }

    // Получает список всех задач в проекте
    const std::vector<Task>& getTasks() const {
        return tasks;
    }

    // Возвращает отформатированный список задач в виде строки для вывода
    std::string getTaskList() const {
        std::ostringstream out;
        for (const auto& task : tasks) {
            out << "[" << task.getId() << "] "
                << task.getTitle() << " - "
                << Task::statusToString(task.getStatus()) << "\n";
        }
        return out.str();
    }
};