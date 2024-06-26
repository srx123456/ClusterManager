#include "Connection.h"

Connection::Connection(int fdConnectionArg)
: fdConnection(fdConnectionArg)
{
    pthread_mutex_init(&connectionMutex, NULL);
    last_time = time(NULL);
    status = OK;
    pthread_create(&tid, NULL, refresh, (void*)this);
}

// 创建一个连接到指定server IP地址的套接字。
Connection::Connection(const char *IP) {
    struct sockaddr_in addr;

    fdConnection = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    // 配置了server的服务端口
    addr.sin_port = htons(LISTENER_PORT);
    // 使用inet_pton函数将IP地址转换为网络字节序，并存储到addr结构体的sin_addr成员变量中。
    inet_pton(AF_INET, IP, &addr.sin_addr);

    if (connect(fdConnection, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    	perror("connect");
    	exit(1);
    }
    // 使用pthread_mutex_init函数初始化connectionMutex互斥锁。
    pthread_mutex_init(&connectionMutex, NULL);
    // 获取当前时间，并将其保存在last_time变量中。
    last_time = time(NULL);
    status = OK;
    // 使用pthread_create函数创建一个新线程，该线程调用refresh函数，并将当前对象的指针作为参数传递给该函数。
    pthread_create(&tid, NULL, refresh, (void*)this);
}

Connection::~Connection() {
    close(fdConnection);
    pthread_mutex_destroy(&connectionMutex);
    pthread_cancel(tid);
}

// 从套接字读取单个消息，并将消息的大小和内容存储到缓冲区中。
int Connection::ReadSingleMessageFromSocketToBuf() {
    // 对套接字进行轮询。
    pollfd struct_for_poll;
    struct_for_poll.fd = fdConnection;
    // 设置轮询事件为 POLLIN，表示在套接字上有数据可读。
    struct_for_poll.events = POLLIN;
    // 进行一次轮询，检查套接字上是否有数据可读。参数 1 表示要轮询的结构体数组的大小，0 表示不超时等待。
    poll(&struct_for_poll, 1, 0);

    if ((struct_for_poll.revents & POLLIN) == 0) {
    	return -1;
    }

    // 用于记录已经接收到的字节数。
	int32_t total_delivered;
    int32_t current_delivered;
    // 初始化 total_delivered 为 0，定义变量 size_of_message 用于存储消息的大小。
    total_delivered = 0;
    int32_t size_of_message;
    // 通过循环从套接字接收消息大小信息，确保接收完整的 int32_t 大小的消息长度信息。
    while (total_delivered < sizeof(int32_t)) {
        current_delivered = recv(fdConnection, (&size_of_message) + total_delivered,
        	sizeof(int32_t) - total_delivered, 0);
        if ((current_delivered == 0) && (total_delivered == 0)) {
            return -1;
        }
        total_delivered += current_delivered;
    }
    // 接收到的消息大小信息从网络字节序转换为主机字节序。
    size_of_message = ntohl(size_of_message);

    total_delivered = 0;
    // : 通过循环从套接字接收消息内容，确保接收完整的消息。
    while (total_delivered < size_of_message) {
    	current_delivered = recv(fdConnection, buf + total_delivered,
    		size_of_message - total_delivered, 0);
    	total_delivered += current_delivered;
    }

    buf[size_of_message] = '\0';
    return 0;
}

// 根据消息类型将从缓冲区中读取的 XML 消息解析为不同的数据结构，并将其存储到相应的队列中。
int Connection::DeserializeStructuresFromBufToCorrQueue() {
    // 创建一个 XML 文档对象 xmlMessage。
	tinyxml2::XMLDocument xmlMessage;
    // 将缓冲区中的数据解析为 XML 格式的消息，并存储到 xmlMessage 中。
    xmlMessage.Parse(buf);
    // 获取 XML 消息中名为 "root" 的根元素。
    tinyxml2::XMLElement *pRoot = xmlMessage.FirstChildElement("root");
    // 获取根元素下第一个名为 "message" 的子元素。
    tinyxml2::XMLElement *pElement = pRoot->FirstChildElement("message");
    while (pElement != NULL) {
        // 获取当前 "message" 元素的 "type" 属性值，用于确定消息的类型。
        const char *type;
        type = pElement->Attribute("type");
        if (strcmp(type, "task") == 0) {
        	std::shared_ptr<Task> pTask(new Task);
        	pTask->filename = std::string(pElement->FirstChildElement("name")->GetText());
        	if (pElement->FirstChildElement("arguments")->GetText() != NULL) {
                pTask->args = std::string(pElement->FirstChildElement("arguments")->GetText());
            }
            else {
                pTask->args = std::string("");
            }
        	pElement->FirstChildElement("taskID")->QueryIntText(&pTask->taskID);
        	tasks.push(pTask);
        }
        else if (strcmp(type, "report") == 0) {
            std::shared_ptr<Report> pReport(new Report);
            pReport->report = std::string(pElement->FirstChildElement("report")->GetText());
            pElement->FirstChildElement("taskID")->QueryIntText(&pReport->taskID);
            reports.push(pReport);
        }
        else if (strcmp(type, "info") == 0) {
            std::shared_ptr<Info> pInfo(new Info);
            pInfo->hostname = std::string(pElement->FirstChildElement("hostname")->GetText());
            pInfo->IP = std::string(pElement->FirstChildElement("IP")->GetText());
            info.push(pInfo);
        }
        else if (strcmp(type, "check") == 0) {
            std::shared_ptr<Check> pCheck(new Check);
            pCheck->t = time(NULL);
            checks.push(pCheck);

        }
        pElement = pElement->NextSiblingElement("message");
    }
    return 0;
}

int Connection::DumpAllFromSocketToCorrespondingQueue() {
	while (ReadSingleMessageFromSocketToBuf() == 0) {
        DeserializeStructuresFromBufToCorrQueue();
	}
}

int Connection::GetTask(std::shared_ptr<Task> &pTask) {
    pthread_mutex_lock(&connectionMutex);
    if (tasks.empty()) {
        pthread_mutex_unlock(&connectionMutex);
    	return -1;
    }
    // 队列queue<std::shared_ptr<Task>> tasks;
    // 取头元素，弹出头元素
    pTask = tasks.front();
    tasks.pop();
    pthread_mutex_unlock(&connectionMutex);
    return 0;
}

int Connection::GetReport(std::shared_ptr<Report> &pReport) {
    pthread_mutex_lock(&connectionMutex);
    if (reports.empty()) {
    	pthread_mutex_unlock(&connectionMutex);
        return -1;
    }
    pReport = reports.front();
    reports.pop();
    pthread_mutex_unlock(&connectionMutex);
    return 0;
}

int Connection::GetInfo(std::shared_ptr<Info> &pInfo) {
    pthread_mutex_lock(&connectionMutex);
    if (info.empty()) {
    	pthread_mutex_unlock(&connectionMutex);
        return -1;
    }
    pInfo = info.front();
    info.pop();
    pthread_mutex_unlock(&connectionMutex);
    return 0;
}

// server发送client需要执行的任务
int Connection::SendTask(std::shared_ptr<Task> pTask) {
    pthread_mutex_lock(&connectionMutex);
    // 将一个Task对象序列化为XML格式，并将序列化后的内容存储到buf缓冲区中。
    SerializeTaskToBuf(pTask);
    // 将buf缓冲区中的单个消息发送到套接字fdConnection。
    WriteSingleMessageFromBufToSocket();
    pthread_mutex_unlock(&connectionMutex);
    return 0;
}

// client发送任务的执行结果到server
int Connection::SendReport(std::shared_ptr<Report> pReport) {
    pthread_mutex_lock(&connectionMutex);
    // 将一个Task对象序列化为XML格式，并将序列化后的内容存储到buf缓冲区中。
    SerializeReportToBuf(pReport);
    // 将buf缓冲区中的单个消息发送到套接字fdConnection。
    WriteSingleMessageFromBufToSocket();
    pthread_mutex_unlock(&connectionMutex);
    return 0;
}

int Connection::SendInfo(std::shared_ptr<Info> pInfo) {
    pthread_mutex_lock(&connectionMutex);
    SerializeInfoToBuf(pInfo);
    WriteSingleMessageFromBufToSocket();
    pthread_mutex_unlock(&connectionMutex);
    return 0;
}

int Connection::SendCheck() {
    SerializeCheckToBuf();
    WriteSingleMessageFromBufToSocket();
    return 0;
}

// 将一个Task对象序列化为XML格式，并将序列化后的内容存储到buf缓冲区中。
int Connection::SerializeTaskToBuf(std::shared_ptr<Task> pTask) {
    // 创建一个XMLDocument对象xmlMessage，用于构建XML文档。
    tinyxml2::XMLDocument xmlMessage;
    // 创建一个根元素pRoot，并将其插入到xmlMessage中。
    tinyxml2::XMLElement *pRoot = xmlMessage.NewElement("root");
    // 创建一个message元素pMessage，并设置其type属性为"task"，然后将其插入到pRoot中。
    tinyxml2::XMLElement *pMessage = xmlMessage.NewElement("message");
    pMessage->SetAttribute("type", "task");
    // 创建name、arguments和taskID元素，并分别设置其文本内容为pTask对象的filename、args和taskID属性的值，然后将它们依次插入到pMessage中。
    tinyxml2::XMLElement *pName = xmlMessage.NewElement("name");
    tinyxml2::XMLElement *pArgs = xmlMessage.NewElement("arguments");
    tinyxml2::XMLElement *pReportID = xmlMessage.NewElement("taskID");
    pName->SetText(pTask->filename.c_str());
    pArgs->SetText(pTask->args.c_str());
    pReportID->SetText(pTask->taskID);
    // 将pRoot插入到xmlMessage的首个子元素位置。
    pMessage->InsertFirstChild(pReportID);
    pMessage->InsertFirstChild(pArgs);
    pMessage->InsertFirstChild(pName);
    pRoot->InsertFirstChild(pMessage);
    xmlMessage.InsertFirstChild(pRoot);
    // 创建一个XMLPrinter对象printer，用于将xmlMessage打印为字符串。
    tinyxml2::XMLPrinter printer;
    xmlMessage.Print(&printer);
    // 将printer打印的字符串拷贝到buf缓冲区的偏移量为sizeof(int32_t)的位置。
    strcpy(buf + sizeof(int32_t), printer.CStr());
    // 将printer打印的字符串的长度转换为网络字节序，并存储到buf缓冲区的前sizeof(int32_t)个字节中。
    *((int32_t*)buf) = htonl(strlen(printer.CStr()));
    return 0;
}

// 将检查信息序列化到缓冲区中
int Connection::SerializeCheckToBuf() {
    //创建一个名为 xmlMessage 的 XML 文档对象
    tinyxml2::XMLDocument xmlMessage;
    tinyxml2::XMLElement *pRoot = xmlMessage.NewElement("root");
    tinyxml2::XMLElement *pMessage = xmlMessage.NewElement("message");
    pMessage->SetAttribute("type", "check");
    pRoot->InsertFirstChild(pMessage);
    xmlMessage.InsertFirstChild(pRoot);
    tinyxml2::XMLPrinter printer;
    xmlMessage.Print(&printer);
    // 将 XML 打印机中的字符串内容复制到缓冲区 buf 中，跳过 int32_t 类型的长度前缀。
    strcpy(buf + sizeof(int32_t), printer.CStr());
    // 将 XML 字符串的长度（经过 strlen 函数计算得到）转换为网络字节序，并存储在缓冲区的开头，作为长度前缀。
    *((int32_t*)buf) = htonl(strlen(printer.CStr()));
    return 0;
}

// 将一个Report对象序列化为XML格式，并将序列化后的内容存储到buf缓冲区中。
int Connection::SerializeReportToBuf(std::shared_ptr<Report> pReport) {
    // 创建一个XMLDocument对象xmlMessage，用于构建XML文档。
    tinyxml2::XMLDocument xmlMessage;
    // 创建一个根元素pRoot，并将其插入到xmlMessage中。
    tinyxml2::XMLElement *pRoot = xmlMessage.NewElement("root");
    // 创建一个message元素pMessage，并设置其type属性为"report"，然后将其插入到pRoot中。
    tinyxml2::XMLElement *pMessage = xmlMessage.NewElement("message");
    // 创建report和taskID元素，并分别设置其文本内容为pReport对象的report和taskID属性的值，然后将它们依次插入到pMessage中。
    pMessage->SetAttribute("type", "report");
    tinyxml2::XMLElement *pText = xmlMessage.NewElement("report");
    tinyxml2::XMLElement *pReportID = xmlMessage.NewElement("taskID");
    pText->SetText(pReport->report.c_str());
    pReportID->SetText(pReport->taskID);
    // 将pRoot插入到xmlMessage的首个子元素位置。
    pMessage->InsertFirstChild(pReportID);
    pMessage->InsertFirstChild(pText);
    pRoot->InsertFirstChild(pMessage);
    xmlMessage.InsertFirstChild(pRoot);
    // 创建一个XMLPrinter对象printer，用于将xmlMessage打印为字符串。
    tinyxml2::XMLPrinter printer;
    xmlMessage.Print(&printer);
    
    strcpy(buf, printer.CStr());
    strcpy(buf + sizeof(int32_t), printer.CStr());
    // 将printer打印的字符串的长度转换为网络字节序，并存储到buf缓冲区的前sizeof(int32_t)个字节中。
    *((int32_t*)buf) = htonl(strlen(printer.CStr()));
    return 0;
}

int Connection::SerializeInfoToBuf(std::shared_ptr<Info> pInfo) {
	tinyxml2::XMLDocument xmlMessage;
	tinyxml2::XMLElement *pRoot = xmlMessage.NewElement("root");
    tinyxml2::XMLElement *pMessage = xmlMessage.NewElement("message");
    pMessage->SetAttribute("type", "info");
    tinyxml2::XMLElement *pIP = xmlMessage.NewElement("IP");
    pIP->SetText(pInfo->IP.c_str());
    tinyxml2::XMLElement *pHostname = xmlMessage.NewElement("hostname");
    pHostname->SetText(pInfo->hostname.c_str());
    pMessage->InsertFirstChild(pIP);
    pMessage->InsertFirstChild(pHostname);
    pRoot->InsertFirstChild(pMessage);
    xmlMessage.InsertFirstChild(pRoot);
    tinyxml2::XMLPrinter printer;
    xmlMessage.Print(&printer);
    strcpy(buf, printer.CStr());
    strcpy(buf + sizeof(int32_t), printer.CStr());
    *((int32_t*)buf) = htonl(strlen(printer.CStr()));
	return 0;
}

// 将buf缓冲区中的单个消息发送到套接字fdConnection。
int Connection::WriteSingleMessageFromBufToSocket() {
    // 将消息的大小转换为网络字节序，并存储到buf缓冲区的前sizeof(int32_t)个字节中。
    int32_t size_of_message_plus_prefix;
    int32_t size_of_message;
    size_of_message = strlen(buf + sizeof(int32_t));

    *((uint32_t*)buf) = htonl(size_of_message);

    int32_t total_sent;
    int32_t current_sent;
    
    total_sent = 0;
    size_of_message_plus_prefix = size_of_message + sizeof(int32_t);
    
    // 使用循环发送消息，直到所有字节都发送完毕。
    // 在每次发送之前，阻塞SIGPIPE信号，以避免因为连接断开而导致的SIGPIPE信号中断程序。
    while (total_sent < size_of_message_plus_prefix) {
    	sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGPIPE);
        sigprocmask(SIG_BLOCK, &sigset, NULL);
        current_sent = send(fdConnection, buf + total_sent, size_of_message_plus_prefix - total_sent, 0);
        if (errno == EPIPE) {
            return -1;
        }
    	total_sent += current_sent;
    }

    return 0;
}



void *Connection::refresh(void *arg) {
    Connection *This = (Connection*)arg;
    while (true) {
        pthread_mutex_lock(&This->connectionMutex);
        // 执行发送检查操作。
        This->SendCheck();
        // 将所有数据从套接字转移到相应的队列中。
        This->DumpAllFromSocketToCorrespondingQueue();
        time_t curr_time = time(NULL);
        // 当 checks 队列不为空时，遍历队列，更新 last_time 为队列中最新的时间。
        while (!This->checks.empty()) {
            if (This->last_time < This->checks.front()->t) {
                This->last_time = This->checks.front()->t;
            }
            This->checks.pop();
        }
        // 当前时间与 last_time 的差大于 BIG_DELAY，则将 status 设置为 FAILED，表示连接失败。
        if (curr_time - This->last_time > BIG_DELAY) {
            This->status = FAILED;
        }
        else {
            This->status = OK;
        }
        pthread_mutex_unlock(&This->connectionMutex);
        sleep(SMALL_DELAY);
        sched_yield();
    }
}

int Connection::CheckStatus() {
    // 使用pthread_mutex_lock函数对connectionMutex互斥锁进行加锁。
    pthread_mutex_lock(&connectionMutex);
    if (status == OK) {
        pthread_mutex_unlock(&connectionMutex);
        return 0;
    }
    else {
        pthread_mutex_unlock(&connectionMutex);
        return -1;
    }
}