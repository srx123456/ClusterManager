#if !defined( __AUTO_MAPPER__H__)
#define __AUTO_MAPPER__H__

#include "MsgStruct.h"

#include <iostream>
#include <string>
#include <vector>

// server端构造任务，读取csv文件自动运行tinymapper建数据通路
class AutoMapper {
public:
	std::vector<Task> readTasksFromCSV(const std::string& filename);
};

#endif