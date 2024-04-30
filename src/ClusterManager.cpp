#include "ClusterManager.h"
#include "ClusterShell.h"

ClusterManager::ClusterManager() {
    // 使用ClusterShell对象的功能
    shell = std::shared_ptr<ClusterShell>(new ClusterShell(this));

}

ClusterManager::~ClusterManager() {
}

int ClusterManager::Run() {
    // 这里通过命令行可以选择具体的功能： 打印信息 或 让client执行特定的命令行的任务
    shell->Run();

	while (true) {
        // 获取客户端的ip等信息
        std::shared_ptr<Connection> connection = listener.AcceptConnection();
        if (connection != NULL) {
            std::shared_ptr<ConnectionManager> pConnectionManager;
            ID connectionID;
            // 为新接入的节点 赋值 id号
            AllocateNewExecutorID(&connectionID);
            // 使用ConnectionManager对象来管理client
            pConnectionManager = std::shared_ptr<ConnectionManager>(new ConnectionManager (connection, connectionID, this));
            connections[connectionID] = pConnectionManager;
            pConnectionManager->Run();
        }
        sched_yield();
	}
    return 0;
}

// 赋值给接入的client一个 id 信息
// 按照连接接入的顺序赋 id 
int ClusterManager::AllocateNewExecutorID(ID *newID) {
    for (size_t i = 0; i < connections.size(); ++i) {
        if (connections[i] == NULL) {
        	*newID = i;
        	return 0;
        }
    }
    if (connections.capacity() == connections.size()) {
        connections.reserve(2 * connections.capacity());
    }
    connections.resize(connections.size() + 1);
    *newID = connections.size() - 1;
    return 0;
}

// 每次调用该方法时，counter的值都会递增，以确保每个任务都有一个唯一的ID。
int ClusterManager::AllocateNewTaskID(ID *newID) {
    // 静态局部变量counter被初始化为0。静态局部变量的生命周期从第一次调用该函数开始，直到程序结束。
    static ID counter = 0;
    *newID = counter++;
    return 0;
}

int ClusterManager::AddTask(std::shared_ptr<Task> pTask) {
    std::shared_ptr<ConnectionManager> pConnectionManager;
    // 如果输入的节点ID < 0 则由系统自动选取一个负载最少的节点执行任务
    if (pTask->executor < 0) {
        if (GetExecutorForTask(pConnectionManager) != 0) {
            return -1;
        }
    }
    else {
        // 取出输入的节点id 对应的 连接
        if (connections[pTask->executor] == NULL) {
            return -1;
        }
        pConnectionManager = connections[pTask->executor];
    }
    // 将任务发布到该节点上
    pConnectionManager->AddTask(pTask);
    return 0;
}

// 获取当前最少任务的节点来执行任务。-- 负载均衡
int ClusterManager::GetExecutorForTask(std::shared_ptr<ConnectionManager> &pConnectionManager) {
    //min_number_of_tasks用于记录当前最小的任务数量，以便找到任务数量最少的节点来执行任务。这样可以实现任务的负载均衡，将任务分配给负载较低的节点。
    int min_number_of_tasks = -1;
    int result = -1;
    // 存放ConnectionManager的 列表
    for (size_t i = 0; i < connections.size(); ++i) {
        if (connections[i] != NULL) {
            if ((min_number_of_tasks == -1) || (min_number_of_tasks > connections[i]->NumberOfTasks())) {
                min_number_of_tasks = connections[i]->NumberOfTasks();
                result = i;
            }
        }
    }
    if (result != -1) {
        pConnectionManager = connections[result];
        return 0;
    }
    else {
        return -1;
    }
}

int ClusterManager::SendToShellReport(std::shared_ptr<Report> pReport) {
    shell->AddReport(pReport);
    return 0;
}


// 获取当前连接到server的所有client节点的信息，并打印到屏幕上
int ClusterManager::GetInfo(std::string &arg) {
    std::stringstream buf;
    std::string connectionInfo;
    int number_of_nodes = 0;
    buf << "----------------\n";    
    for (size_t i = 0; i < connections.size(); ++i) {
        if (connections[i] != NULL) {
            ++number_of_nodes;
            connections[i]->GetInfo(connectionInfo);
            buf << connectionInfo << "****************" << std::endl;
        }
    }
    buf << "total number of nodes: " << number_of_nodes << std::endl;
    buf << "----------------\n\0";
    std::getline(buf, arg, '\0');
    return 0;
}

int ClusterManager::DeleteConnectionAndRearrangeTasks(ID connectionID, std::vector<std::shared_ptr<Task>> tasks) {
    connections[connectionID] = NULL;
    for (size_t i = 0; i < tasks.size(); ++i) {
        AddTask(tasks[i]);
    }
    return 0;
}