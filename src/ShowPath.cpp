#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

struct Connection {
    std::string srcAddr;
    std::string dstAddr;
    int executorID;

    Connection(const std::string& src, const std::string& dst, int executor)
        : srcAddr(src), dstAddr(dst), executorID(executor) {}
};

// 通过递归追溯连接关系，构建完整的链路
void buildConnectionChain(const std::string& dstAddr, const std::unordered_map<std::string, std::string>& connectionMap,
                          std::vector<Connection>& chain) {
    // 如果在连接关系中找不到目的地址，则说明链路结束
    if (connectionMap.find(dstAddr) == connectionMap.end()) {
        return;
    }

    // 获取目的地址对应的源地址
    std::string srcAddr = connectionMap.at(dstAddr);

    // 将目的地址和源地址添加到链路中
    chain.emplace_back(srcAddr, dstAddr, 0);  // 这里的 executorID 可以根据实际情况设置

    // 递归追溯下一个连接关系
    buildConnectionChain(srcAddr, connectionMap, chain);
}


std::unordered_map<std::string, std::string> buildConnectionMap(const std::string& filename) {
    std::unordered_map<std::string, std::string> connectionMap;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return connectionMap;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        Connection conn;
        char comma;
        if (!(iss >> conn.srcAddr >> comma >> conn.dstAddr >> comma >> conn.executorID)) {
            std::cerr << "Failed to parse line: " << line << std::endl;
            continue;
        }
        connectionMap[conn.dstAddr] = conn.srcAddr;
    }
    return connectionMap;
}

int main() {
    std::string filename = "collectResult.csv";
    std::unordered_map<std::string, std::string> connectionMap = buildConnectionMap(filename);
    // Now you have a map of destination address to source address
    // 起始目的地址
    std::string startDstAddr = "192.168.72.129:7676";

    // 存储链路的容器
    std::vector<Connection> chain;

    // 从起始目的地址开始构建链路
    buildConnectionChain(startDstAddr, connectionMap, chain);

    // 输出链路信息
    std::cout << "Connection chain:" << std::endl;
    for (const auto& conn : chain) {
        std::cout << "Source: " << conn.srcAddr << ", Destination: " << conn.dstAddr << ", ExecutorID: " << conn.executorID << std::endl;
    }

    return 0;
}
