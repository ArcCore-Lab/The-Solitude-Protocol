# 🧱 阶段 1：手搓 Web Server（Day 1 – Day 20）

---

## **Day 1 — 空跑服务器**

**目标**：`curl localhost:8080` 输出 "Hello"

### 今日步骤

1. `socket()` → `bind()` → `listen()` → `accept()`
2. 每 accept 一个 client，`write("Hello", 5)`
3. 单进程 + 单线程即可。

### 阅读书籍

* **《UNIX 环境高级编程（APUE）》**

  * 第 15 章：进程间通信
  * 第 16 章：网络 IPC（重点 socket）
* **《UNIX 网络编程 卷1》**

  * 第 3、4 章：socket、bind、listen、accept

---

## **Day 2 — HTTP 请求解析（Method & Path）**

**目标**：打印 `GET /path`

### 今日步骤

1. 使用 `read()` 读取 request
2. 处理 partial read：只要找到 `\r\n\r\n` 就停止
3. 手写简单 parser：`strtok` 或手写 FSM。

### 阅读书籍

* **HTTP 协议**：离线的话看《HTTP 权威指南》前 5 章
* APUE：I/O 章节（偏向 read/缓冲区设计）

---

## **Day 3 — 响应静态文件**

**目标**：能返回 `GET /index.html`
找不到 → 返回 404

### 今日步骤

1. `open()` + `read()` 文件
2. 根据 errno 处理各种情况
3. 学习 SIGPIPE：如果客户端断开，write 会杀你。

### 阅读书籍

* 《APUE》文件 I/O
* UNIX 网络编程：错误处理章节

---

## **Day 4 — keep-alive**

**目标**：wrk 测 1k QPS 不崩
**问题点**：fd 泄漏、EPOLLHUP 处理不当

### 今日步骤

1. 处理 Connection: keep-alive
2. 把每个 fd 重复利用
3. 监控 close 时机

### 阅读书籍

* 《UNP 卷1》：epoll（I/O 多路复用那章）

---

## **Day 5 — epoll 重构**

**目标**：整个 server 改造成单线程 epoll
**重点**：ET vs LT 的语义差异

### 今日步骤

1. 建立事件循环
2. 非阻塞 IO（`fcntl(fd, O_NONBLOCK)`）
3. 处理边缘触发下的连续 read/write

### 阅读书籍

* 《Linux 高性能服务器编程》游双（强烈推荐）

  * epoll 章节深入浅出

---

## **Day 6 — 写缓冲区 + writev**

**目标**：解决 partial send，支持大文件
**重点**：write 返回 < len → 缓冲区剩余数据挂回 epollout

### 今日步骤

1. 建立 per-connection 写缓冲
2. 拥抱 writev + iovec
3. EAGAIN 的正确处理

### 阅读书籍

* 《UNP 卷1》：scatter/gather I/O 章节
* 《APUE》：writev 用法

---

## **Day 7 — 内存池 v1**

**目标**：替换 malloc → 自己的 align + free list

### 今日步骤

1. 实现简单单链表 free-list
2. 默认按 8 字节对齐
3. 所有连接的缓冲区都从 pool 分配

### 阅读书籍

* 《TCMalloc 源码分析》（如果离线无此书，用书籍替代）
* 《APUE》内存管理部分

---

## **Day 8 — 零拷贝**

**目标**：sendfile + TCP_CORK 优化

### 今日步骤

1. 实现静态文件 → sendfile
2. 使用 tcp_cork temporarily
3. nodelay vs cork 的交互

### 阅读书籍

* 《Linux 内核 TCP/IP 实现》
* 《UNP 卷2》零拷贝部分

---

## **Day 9 — 压测调优**

**目标**：10k QPS
**工具**：perf stat -e cache-misses（离线可用）

### 今日步骤

1. 观察 cache miss
2. 优化热路径（去掉不必要的 memcpy）
3. 加快 parser

### 阅读书籍

* 《Linux 性能优化实战》
* Brendan Gregg 《Systems Performance》

---

## **Day 10 — HTTP 管线化（难点）**

**目标**：支持 pipeline（多个 request 紧密发送）

### 今日步骤

1. per-connection request queue
2. 状态机：REQ → RESP → REQ
3. full-duplex：读写都要挂事件

### 阅读书籍

* 《HTTP 权威指南》Pipeline 章节
* 《UNP》非阻塞全双工设计

---

## **Day 11 — 目录索引**

**目标**：访问 `/dir/` 时列出文件清单（HTML）

### 今日步骤

1. opendir + readdir
2. 转成 HTML
3. 输出 UTF-8，注意中文名编码

### 阅读书籍

* 《APUE》目录操作章节

---

## **Day 12 — CGI 支持**

**目标**：fork + execve 运行外部程序

### 今日步骤

1. pipe()
2. dup2(stdin/out)
3. 设置环境变量
4. 返回输出

### 阅读书籍

* 《APUE》进程控制
* 《UNP》CGI 部分

---

## **Day 13 — POST 表单解析**

**目标**：支持提交表单→解析

### 今日步骤

1. urldecode
2. 根据 Content-Length 读取
3. 放入 key-value

### 阅读书籍

* 《HTTP 权威指南》POST 章节

---

## **Day 14 — access log**

**目标**：兼容 Nginx 格式
strftime + writev

### 阅读书籍

* 《C 标准库》strftime
* 《Nginx 官方文档》日志格式

---

## **Day 15 — 多进程 prefork**

**目标**：像 Nginx worker 一样 prefork

### 今日步骤

1. 多进程 share socket
2. accept lock（或者用 SO_REUSEPORT）
3. 父进程管理子进程

### 阅读书籍

* 《APUE》进程关系
* 《UNP》prefork 模型

---

## **Day 16 — 热升级**

**目标**：无缝 reload
execve + 传递 fd

### 今日步骤

1. 使用 environment 传 socket fd
2. 新版本继承 listen fd
3. 父子进程接力

### 阅读书籍

* 《Nginx 源码分析》
* 《APUE》exec 家族

---

## **Day 17 — 安全**

**目标**：防目录穿越
realpath + chroot

### 今日步骤

1. 检查路径是否合法
2. chroot 限制服务进程范围

---

## **Day 18 — 单元测试框架**

**目标**：一个最小的 test 框架（fork + assert）

### 今日步骤

1. test_case()
2. fork 并隔离
3. assert 打印

---

## **Day 19 — 火焰图**

**目标**：50k QPS
perf + FlameGraph

### 今日步骤

1. `perf record -g`
2. `perf script`
3. flamegraph.pl

### 阅读书籍

* Brendan Gregg《FlameGraph 文档》

---

## **Day 20 — 阶段封存**

**目标**：tag v1.0
写一篇性能报告

---

# 🧱 阶段 2：手搓 malloc（Day 21 – Day 25）

---

## Day 21 — mmap & sbrk 起步

阅读：《APUE》内存分配
目标：用 mmap 实现最小堆

## Day 22 — thread-cache

目标：线程本地缓存
阅读：TCMalloc 论文

## Day 23 — small/large size class

目标：bin + chunk 分级管理

## Day 24 — 多线程压测

目标：性能 > glibc malloc × 1.5

## Day 25 — 阶段封存

写 README（论文级）

---

# 🧱 阶段 3：手搓内核（Day 26 – Day 30）

---

## Day 26 — 内核启动

阅读：《操作系统真相还原》
目标：GRUB → ELF → long mode

## Day 27 — GDT / IDT

阅读：Intel SDM（Volume 3）
目标：键盘中断、时钟中断

## Day 28 — 分页 + kmalloc

阅读：OSDev Wiki（离线镜像）
目标：建立页表、基本内核内存池

## Day 29 — 调度 + fork

目标：多任务 + TSS + 上下文切换

## Day 30 — shell + exec

目标：一个简单用户态程序可运行

---

# 📚 离线环境：你必须提前准备的图书

这四本足够你完成几乎所有任务：

### **1. 《UNIX 环境高级编程》APUE**

网络、IO、进程控制全覆盖。

### **2. 《UNIX 网络编程 卷1、卷2》**

你的 web server 全靠它了。

### **3. 《Linux 高性能服务器编程》游双**

epoll、零拷贝、TCP 细节讲得最好。

### **4. 《操作系统真相还原》**

把 OS 核心流程讲得最清楚。

另外 2 本如果你搞得定也很强：

* **TCMalloc 原理**（任何一本讲 malloc 的书）
* **Intel SDM 第 3 卷（中断、分页）**

---
