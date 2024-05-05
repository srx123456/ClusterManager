#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>

int main() {
    std::ifstream file("./AutoTask.csv");
    if (!file.is_open()) {
        // 文件打开失败
        return -1;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string filename, args, executor;
        if (std::getline(iss, filename, ',') &&
            std::getline(iss, args, ',') &&
            std::getline(iss, executor, ',')) {
            std::cout << "Filename: " << filename << ", Args: " << args << ", Executor: " << executor << std::endl;
        }
    }

    file.close();

    return 0;
}
