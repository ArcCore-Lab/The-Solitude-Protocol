<div align="center">

# 🧠 ARC-Core Lab
### **The Solitude Protocol · Howell Stark**

[EN](README_EN.md) | [CN](README.md) | [PLAN](PLAN.md) | [LOG](LOG.md) | [README](README.md)

> **“孤独是构造力的燃料，沉默是思想的放大器。”**  
> **“当你不再被输入定义，你才开始输出自己。”**

---

</div>

## 🎯 Mission Briefing

**“孤狼协议”** 是一项为期 31 天的极限工程实验。  
本人将进入完全的社交隔离状态，以「哲学 × 工程 × 艺术」为核心驱动，旨在完成三项可运行、可测量、可复现的系统级构建，以此探索专注与创造力的极限。

1.  ⚙️ **C10K 级 HTTP 服务器** (`epoll` + 内存池 + 零拷贝)
2.  🧵 **线程缓存 `malloc` 实现** (性能超越 glibc 1.5 倍)
3.  💻 **可引导至 Shell 的 x86 内核** (于 `QEMU` 中跑通 `fork` 与 `exec`)

---

## 🗺️ Repository Layout

```
.
├── src/            # 核心源代码
├── LOG.md          # 每日技术与思想日志
├── PLAN.md         # 项目计划与执行细则
└── README.md       # 项目总纲 (本文件)
```

---

## ⏱️ Execution Protocol

1.  **作战时间**: 每日 09:00 – 23:00。
2.  **日志归档**: 每日 **23:30** 前，于 `LOG.md` 中记录当日进展、关键提交、性能指标及思考。不求数量，只求深度。
3.  **核心戒律**: 所有代码必须手动编写。允许查阅 `man` 手册及文档，但严禁复制粘贴 AI 生成的代码。
4.  **熔断机制**: 每缺席一次日志归档，项目暂停一天，不予补时。
5.  **最终交付**: 协议期满后，进行性能演示，并发布火焰图分析及论文级技术报告。

---

## 🚀 Project Milestones

| Phase | Module | Target Objective |
|:-----:|:---|:---|
| v1.0 | HTTP Server | C10K, Keep-Alive, Zero-Copy I/O |
| v2.0 | `malloc` | Thread-Local Cache, Bin/Chunk Mgmt, Benchmark Supremacy |
| v3.0 | Kernel | GRUB → ELF Loader → `fork()` → `exec()` |

---

## ⚖️ License

**MIT License © 2026 Howell Stark**  
> *Built in Silence. Released with Fire.*

---

### 📌 Additional Information

这个项目只是我个人专注开发实验的记录。如果你想了解我完成每一步的细节，请参考每日更新的 [LOG.md](LOG.md) 文件以及每日源码所在的 `src/` 目录。

---

<div align="center">

### 🔺 ARC-Core Signature

```
    _    ____   ____    ____ ___  ____  _____ 
   / \  |  _ \ / ___|  / ___/ _ \|  _ \| ____|
  / _ \ | |_) | |     | |  | | | | |_) |  _|  
 / ___ \|  _ <| |___  | |__| |_| |  _ <| |___ 
/_/   \_\_| \_\\____|  \____\___/|_| \_\_____|
```

---

**Status:** Preparing Environment...  
**Countdown:** 31 days to ignition.  
**Silence Level:** 100%  
**Contact:** _Temporarily Offline_

</div>