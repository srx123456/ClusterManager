#include "AutoMapper.h"

#include <fstream>
#include <sstream>

std::vector<Task> AutoMapper::readTasksFromCSV(const std::string& filename) {
    std::vector<Task> taskList;
    std::ifstream file(filename);
    if (!file.is_open()) {
        // 文件打开失败
        return taskList;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string filename, args, executor;
        if (std::getline(iss, filename, ',') &&
            std::getline(iss, args, ',') &&
            std::getline(iss, executor, ',')) {
            // 创建 Task 对象并设置成员变量的值
            Task task;
            task.filename = filename;
            task.args = args;
            // 假设 executor 是字符串表示的整数，可以使用 std::stoi 进行转换
            task.executor = std::stoi(executor);
            // 添加 Task 对象到列表中
            taskList.push_back(task);
        }
    }

    file.close();
    return taskList;
}