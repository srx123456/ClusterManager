installation and running:

    cd ./bin
    cmake ../src

After that run ./server on your computer and ./client on computers on which your tasks will be run.
./server works in interactive mode. Two commands are available: run and info. After entering command
run and name of task ID of task will be shown. After entering command info list of executors and theirs
tasks will appear on the screen. In addition IDs of completed tasks will be shown.

shellParser实现的是任务的设置功能

输入的节点小于0时，可以激活自带的负载均衡（简陋版）
