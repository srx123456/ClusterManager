#include "ClusterShell.h"

ClusterShell::ClusterShell(ClusterManager *pClusterManagerArg)
: pClusterManager(pClusterManagerArg)
{
    pthread_mutex_init(&reports_mutex, NULL);
}

ClusterShell::~ClusterShell() {
    pthread_mutex_destroy(&reports_mutex);
}

int ClusterShell::Run() {
	// 创建一个新线程运行running_in_another_thread函数，传入当前的ClusterShell对象作为参数
	pthread_create(&tid, NULL, running_in_another_thread, (void*)this);
	return 0;
}

void *ClusterShell::running_in_another_thread(void *pArg) {
	ClusterShell *This;
	This = (ClusterShell*)pArg;

	while (true) {
		This->ReadUserCommand();
		pthread_mutex_lock(&This->reports_mutex);
        while (!This->reports.empty()) {
            std::cout << This->reports.front()->taskID << ": " << This->reports.front()->report << std::endl;
            This->reports.pop();
        }
		pthread_mutex_unlock(&This->reports_mutex);
		sched_yield();
	}

}

// 读取用户的输入的一级指令
int ClusterShell::ReadUserCommand() {
	std::string command;
	ShellParser parser;
	AutoMapper autoM;
	std::cout << "enter your command: ";
    std::getline(std::cin, command, '\n');
    if (command.length() > 0) {
		// 读取用户使用shell真实执行的二级命令
		if (command.compare("run") == 0) {
			std::shared_ptr<Task> pTask(new Task);
			// 使用shell输入框构造Task任务 name args nodeId taskId
			parser.GetTask(pTask);
			pClusterManager->AllocateNewTaskID(&pTask->taskID);
			
	    	if (pClusterManager->AddTask(pTask) != 0) {
	    		std::cout << "unable to add task: there are no executors\n";
	    	}
	    	else {
	    		std::cout << "task added: " << pTask->taskID << std::endl;
	    	}
	    }
		// 该种场景则自动按照配置csv文件建立数据传输通道
		else if (command.compare("autoCreate") == 0) {
        	//std::cout << "enter autoCreate: " << std::endl;
			// std::shared_ptr<Task> pTask(new Task);
			// 读取csv文件构造 name args nodeId taskId
			std::vector<Task> tasks = autoM.readTasksFromCSV("./AutoTask.csv");
			// 假设要循环遍历 tasks 并将其存储在 std::shared_ptr<Task> 对象中
			std::vector<std::shared_ptr<Task>> sharedTasks;
			// 使用范围-based for 循环遍历 std::vector<Task>
    		for (const auto& task : tasks) {
        		//std::cout << "Filename: " << task.filename << ", Args: " << task.args << ", Executor: " << task.executor << std::endl;
				sharedTasks.push_back(std::make_shared<Task>(task));
			}
			// std::cout << "Length of vector: " << sharedTasks.size() << std::endl;
			for(std::shared_ptr<Task> pTask : sharedTasks){
				pClusterManager->AllocateNewTaskID(&pTask->taskID);
				//std::cout << "enter sharedTasks: " << std::endl;
        		//std::cout << "Filename: " << pTask->filename << ", Args: " << pTask->args << ", Executor: " 
				//	<< pTask->executor << ", TaskID: " << pTask->taskID << std::endl;
	    		if (pClusterManager->AddTask(pTask) != 0) {
	    			std::cout << "unable to add task: there are no executors\n";
	    		}
	    		else {
	    			std::cout << "task added: " << pTask->taskID << std::endl;
	    		}
			}
	    }
	    else if (command.compare("info") == 0) {
            std::string info;
            pClusterManager->GetInfo(info);
            std::cout << info;
	    }
	    else if (command.compare("quit") == 0) {
            exit(0);
	    }
	    else {
	    	std::cout << "unknown command\n";
	    	return 0;
	  	}
    }
	return 0;
}


int ClusterShell::AddReport(std::shared_ptr<Report> pReport) {
    pthread_mutex_lock(&reports_mutex);
    reports.push(pReport);
    pthread_mutex_unlock(&reports_mutex);
    return 0;
}