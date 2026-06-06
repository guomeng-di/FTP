# FTP Server & Client

## 环境配置步骤

### 1. 系统要求

操作系统：Ubuntu/CentOS/Debian 等任意 Linux 发行版

编译器：g++（支持 C++11 及以上标准

### 2. 安装编译环境

执行以下命令安装 g++ 编译器：
```c++
# Ubuntu/Debian 系统
sudo apt update
sudo apt install g++ -y

# CentOS/RHEL 系统
sudo yum install gcc-c++ -y
```

### 3. 项目文件说明

将代码保存为以下两个文件：

ftp_server.cpp：FTP 服务端代码

ftp_client.cpp：FTP 客户端代码

注意保存在不同目录下

客户端可以get服务器所在目录的文件

客户端可以put自己所在的目录的文件给服务器


### 4. 项目启动流程

#### 1执行服务端程序：

1>终端进入服务器所在的目录
2>
```c++
./ftp_server
```
3>当提示:
```c++
FTP Server Started On Port 2100
```
时启动成功

#### 2执行客户端程序：

1>新的终端进入客户端所在的目录
2>
```c++
./ftp_client
```
3>当提示:
```c++
===== FTP CLIENT =====
1. LIST
2. GET
3. PUT
4. QUIT

choice:
```
时启动成功(自动连接本地服务端并完成登录)

#### 3测试:

输入对应数字即可


