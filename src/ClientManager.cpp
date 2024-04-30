#include "ClientManager.h"

// 获取本机的主机名和IP地址
// /etc/hostname 或 /etc/sysconfig/network获取主机名
// 根据传入的主机名查询主机的IP地址 /etc/network/interfaces、/etc/sysconfig/network-scripts/ifcfg-<interface>
ClientManager::ClientManager() {
    char buf[HOSTNAME_MAX];
    gethostname(buf, HOSTNAME_MAX);
    hostname = buf;
    struct hostent *host_info = 0;
    // gethostbyname函数是一个过时的函数，只能返回主机的默认IP地址。在现代的网络编程中，更常用的是使用getaddrinfo函数来获取主机的IP地址。
    host_info = gethostbyname(buf);
    char *ip;
    ip = host_info->h_addr_list[0];
    std::stringstream ss;
    ss << unsigned(ip[0]) << '.' << unsigned(ip[1]) << '.' << unsigned(ip[2]) << '.' << unsigned(ip[3]);
    ss >> IP;
}

ClientManager::~ClientManager() {
}


int ClientManager::AllocateNewProcessID(ID *newProcessID) {
    for (size_t i = 0; i < running_tasks.size(); ++i) {
        if (running_tasks[i] == NULL) {
        	*newProcessID = i;
        	return 0;
        }
    }
    if (running_tasks.capacity() == running_tasks.size()) {
        running_tasks.reserve(2 * running_tasks.capacity());
    }
    running_tasks.resize(running_tasks.size() + 1);
    *newProcessID = running_tasks.size() - 1;
    return 0;
}

int ClientManager::LaunchTask(std::shared_ptr<Task> pTask) {
    // 创建任务进程id by running_tasks.size()
    ID processID;
    AllocateNewProcessID(&processID);
    running_tasks[processID] = pTask;
    
    // 创建一个pid号
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "/bin/sh", "-c", (pTask->filename + std::string(" ") + pTask->args).c_str(), (char*)NULL);
    }
    running_tasks[processID]->pid = pid;
    return 0;
}

int ClientManager::CheckTasksStatus() {
	for (size_t i = 0; i < running_tasks.size(); ++i) {
		if (running_tasks[i] == NULL) {
			continue;
		}
		int status;
        // waitpid函数用于等待指定的子进程状态，并返回子进程的退出状态。返回非零值，表示子进程的状态发生了变化
		if (waitpid(running_tasks[i]->pid, &status, WNOHANG) != 0) {
            // 子进程正常退出（使用WIFEXITED宏判断），并且退出状态为0（使用WEXITSTATUS宏判断），表示任务成功完成。
			if (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) {
				std::shared_ptr<Report> pReport(new Report);
				pReport->taskID = running_tasks[i]->taskID;
				pReport->report = "finished";
                // client将task执行的结果上报给server端
				connection->SendReport(pReport);
				running_tasks[i] = NULL;
				continue;
			}
            // 调用LaunchTask函数重新启动当前任务。
			else if (WIFEXITED(status) && (WEXITSTATUS(status) != 0)) {
				printf("restarted\n");
			    LaunchTask(running_tasks[i]);
			    running_tasks[i] = NULL;
			}
		}
	}
	return 0;
}

int ClientManager::Run(const char *serverIP) {
    RegisterInCluster(serverIP);
    while (true) {
        // 使用互斥锁来保护对共享资源的访问，避免多个线程同时访问导致的竞态条件。
        if (connection->CheckStatus() != 0) {
            exit(0);
        }
        // 一个指向Task类型对象的智能指针。
        std::shared_ptr<Task> task;
        // 将任务加入到running_tasks数组，并将所有需要的条件都赋值
        while (connection->GetTask(task) == 0) {
            LaunchTask(task); 
        }
        // 执行running_tasks数组的任务
        CheckTasksStatus();
        sched_yield();
    }
    return 0;
}

// client端 向 server端 注册
int ClientManager::RegisterInCluster(const char *serverIP) {
	connection = std::shared_ptr<Connection>(new Connection (serverIP));
    
    std::shared_ptr<Info> pInfo(new Info);
    // ClientManager构造函数中得到的主机名和ip地址 -- 解决server端不显示host主机名与ip
    pInfo->hostname = hostname;
    pInfo->IP = IP;
    connection->SendInfo(pInfo);
    return 0;
}