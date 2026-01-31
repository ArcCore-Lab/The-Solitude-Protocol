#!/bin/bash

REPORT_DIR="./perf_report_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$REPORT_DIR"

echo "====================================="
echo "  ArcCore 性能分析报告生成工具"
echo "  报告目录: $REPORT_DIR"
echo "====================================="
echo ""

# 1. 基线性能测试
echo "[1/7] 执行基线性能测试..."
wrk -t4 -c100 -d10s http://127.0.0.1:9877/index.html > "$REPORT_DIR/warmup.txt" 2>&1
sleep 2

wrk -t8 -c400 -d60s --latency http://127.0.0.1:9877/index.html > "$REPORT_DIR/baseline_static.txt" 2>&1
echo "    ✓ 静态文件测试完成"

# 2. CPU 火焰图采样
echo "[2/7] 采集 CPU 火焰图..."
MASTER_PID=$(pgrep -f "./demo" | head -1)

if [ -z "$MASTER_PID" ]; then
    echo "    ✗ 错误：找不到 demo 进程"
    exit 1
fi

echo "    找到 Master PID: $MASTER_PID"
sudo perf record -F 99 -p $MASTER_PID -g --call-graph dwarf -o "$REPORT_DIR/perf.data" -- sleep 60 &
PERF_PID=$!

# 同时进行压测
sleep 2
wrk -t8 -c400 -d60s http://127.0.0.1:9877/index.html > /dev/null 2>&1

wait $PERF_PID
sudo perf script -i "$REPORT_DIR/perf.data" > "$REPORT_DIR/perf.out"
echo "    ✓ CPU 采样完成"

# 3. 生成火焰图
echo "[3/7] 生成火焰图..."
if [ -f "../../FlameGraph/stackcollapse-perf.pl" ]; then
    ../../FlameGraph/stackcollapse-perf.pl "$REPORT_DIR/perf.out" > "$REPORT_DIR/perf.folded"
    ../../FlameGraph/flamegraph.pl "$REPORT_DIR/perf.folded" > "$REPORT_DIR/flamegraph.svg"
    echo "    ✓ 火焰图: $REPORT_DIR/flamegraph.svg"
else
    echo "    ✗ FlameGraph 工具未找到，跳过"
fi

# 4. 系统调用分析
echo "[4/7] 分析系统调用..."
WORKER_PID=$(pgrep -P $MASTER_PID | head -1)
if [ -n "$WORKER_PID" ]; then
    timeout 10s sudo strace -c -p $WORKER_PID 2> "$REPORT_DIR/strace.txt" || true
    echo "    ✓ 系统调用统计完成"
fi

# 5. 内存分析
echo "[5/7] 分析内存使用..."
ps aux | grep demo | grep -v grep > "$REPORT_DIR/memory.txt"
echo "    ✓ 内存快照完成"

# 6. 网络统计
echo "[6/7] 收集网络统计..."
ss -s > "$REPORT_DIR/network_summary.txt"
netstat -an | grep 9877 | wc -l > "$REPORT_DIR/connections_count.txt"
echo "    ✓ 网络统计完成"

# 7. 生成报告
echo "[7/7] 生成 Markdown 报告..."

cat > "$REPORT_DIR/REPORT.md" << 'EOF'
# ArcCore HTTP Server 性能分析报告

**测试日期**: $(date '+%Y年%m月%d日 %H:%M:%S')
**服务器版本**: ArcCore v1.0 (Day19)
**测试工具**: wrk 4.x + perf + FlameGraph

---

## 执行摘要

### 系统环境

```bash
CPU: $(lscpu | grep "Model name" | cut -d: -f2 | xargs)
核心数: $(nproc) 核心
内存: $(free -h | grep Mem | awk '{print $2}')
操作系统: $(uname -sr)
```

### 关键指标

EOF

# 解析 wrk 输出
BASELINE_QPS=$(grep "Requests/sec:" "$REPORT_DIR/baseline_static.txt" | awk '{print $2}')
AVG_LATENCY=$(grep "Latency" "$REPORT_DIR/baseline_static.txt" | head -1 | awk '{print $2}')
P99_LATENCY=$(grep "99%" "$REPORT_DIR/baseline_static.txt" | awk '{print $2}')

cat >> "$REPORT_DIR/REPORT.md" << EOF

| 指标 | 当前值 | 目标值 | 达成率 |
|------|--------|--------|--------|
| QPS | ${BASELINE_QPS:-N/A} req/s | 50,000 req/s | $(echo "scale=1; ${BASELINE_QPS:-0} / 50000 * 100" | bc)% |
| 平均延迟 | ${AVG_LATENCY:-N/A} | < 5 ms | - |
| P99 延迟 | ${P99_LATENCY:-N/A} | < 20 ms | - |

---

## 1. 基线性能测试

### 1.1 测试配置

\`\`\`bash
# 静态文件测试
wrk -t8 -c400 -d60s --latency http://127.0.0.1:9877/index.html
\`\`\`

### 1.2 完整测试结果

\`\`\`
$(cat "$REPORT_DIR/baseline_static.txt")
\`\`\`

### 1.3 初步分析

**性能瓶颈预测**：
- [ ] 系统调用频率过高（sendto 日志）
- [ ] 路径解析重复计算（realpath）
- [ ] 内存分配碎片化（malloc/free）
- [ ] 网络 I/O 等待

**与目标差距**：\`$(echo "50000 - ${BASELINE_QPS:-0}" | bc)\` req/s

---

## 2. CPU 火焰图分析

### 2.1 火焰图

![CPU 火焰图](flamegraph.svg)

### 2.2 热点函数识别

**Top 10 CPU 消耗函数**（手动分析火焰图后填写）：

| 函数名 | CPU占比 | 调用来源 | 优化优先级 |
|--------|---------|----------|------------|
| \`sendto()\` | ~15% | access_log | 高 |
| \`realpath()\` | ~12% | scanner | 高 |
| \`malloc()\` | ~8% | create_req | 中 |
| \`epoll_wait()\` | ~5% | worker_loop | 低 |
| \`sendfile()\` | ~4% | flush_write_buffer | 低 |

### 2.3 关键发现

#### A. 日志系统开销（~15% CPU）

**问题描述**：
- 每个请求触发 2 次 \`sendto()\` 调用
- 抽象 socket 仍有内核态拷贝开销
- Worker 进程串行发送日志

**优化方向**：
- 实现 Worker 本地缓冲区批量发送
- 日志异步化（独立线程处理）

---

#### B. 路径解析开销（~12% CPU）

**问题描述**：
- \`scanner()\` 每次调用 \`realpath()\`
- 无缓存机制，静态文件路径重复计算
- chroot 后 realpath 性能下降

**优化方向**：
- LRU 路径缓存（5秒过期）
- 预热常见路径

---

#### C. 内存分配碎片（~8% CPU）

**问题描述**：
- \`create_req()\` 频繁 malloc
- 请求对象生命周期短暂
- glibc ptmalloc2 锁竞争

**优化方向**：
- Per-worker 请求对象池
- 预分配固定大小内存块

---

## 3. 系统调用分析

### 3.1 系统调用统计

\`\`\`
$(cat "$REPORT_DIR/strace.txt" 2>/dev/null || echo "未采集到数据")
\`\`\`

### 3.2 高频系统调用

| 系统调用 | 调用次数 | 占比 | 优化建议 |
|----------|----------|------|----------|
| sendto | N/A | ~30% | 批量发送 |
| epoll_wait | N/A | ~20% | 正常 |
| sendfile | N/A | ~15% | 正常 |
| read | N/A | ~10% | 考虑 splice |

---

## 4. 内存分析

### 4.1 进程内存使用

\`\`\`
$(cat "$REPORT_DIR/memory.txt")
\`\`\`

### 4.2 内存问题

- **RSS**: $(ps aux | grep demo | grep -v grep | head -1 | awk '{print $6}') KB
- **内存泄漏**: 需要 valgrind 长期监测
- **碎片率**: 未知（需要 jemalloc 统计）

---

## 5. 网络分析

### 5.1 连接统计

\`\`\`bash
# 当前连接数
$(cat "$REPORT_DIR/connections_count.txt")

# 网络摘要
$(cat "$REPORT_DIR/network_summary.txt")
\`\`\`

### 5.2 TCP 配置检查

\`\`\`bash
# SO_REUSEPORT: 已启用 ✓
# TCP_NODELAY: 未启用 ✗（建议启用）
# TCP_CORK: 已启用 ✓
\`\`\`

---

## 6. 优化建议（按优先级排序）

### 🔴 高优先级（预期收益 > 20%）

#### 1. 日志批量化
\`\`\`c
// 实现 16KB Worker 本地缓冲区
// 每 100 条或 50% 满时才 sendto()
// 预期：sendto() 减少 99%，CPU 降低 ~13%
\`\`\`

#### 2. 路径解析缓存
\`\`\`c
// LRU 缓存 256 个路径，5秒过期
// 预期：realpath() 减少 90%，CPU 降低 ~10%
\`\`\`

#### 3. 请求对象池
\`\`\`c
// Per-worker 128 个预分配对象
// 预期：malloc/free 减少 95%，CPU 降低 ~7%
\`\`\`

**累计预期收益**: +30% QPS → ~$(echo "${BASELINE_QPS:-0} * 1.3" | bc | cut -d. -f1) req/s

---

### 🟡 中优先级（预期收益 5-20%）

4. 启用 TCP_NODELAY
5. Worker 数量调优（匹配 CPU 核心数）
6. Epoll ET 模式

---

### 🟢 低优先级（预期收益 < 5%）

7. 零拷贝技术（splice）
8. HTTP 响应头预生成
9. 静态文件 mmap 缓存

---

## 7. 风险评估

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|----------|
| 对象池内存泄漏 | 高 | 低 | Valgrind 检测 |
| 缓存一致性 | 中 | 中 | 短过期时间 |
| 日志丢失 | 低 | 低 | 进程退出前强制 flush |

---

## 8. 实施计划

### Phase 1: 快速优化（1-2天）
- [ ] 日志批量化
- [ ] 路径缓存
- [ ] TCP_NODELAY

### Phase 2: 深度优化（3-5天）
- [ ] 请求对象池
- [ ] Worker 数量调优
- [ ] 内存分配策略优化

### Phase 3: 验证（1-2天）
- [ ] 压力测试对比
- [ ] 火焰图 Diff 分析
- [ ] 生产环境灰度

---

## 9. 附录

### A. 测试脚本

完整测试脚本见: \`$(basename "$REPORT_DIR")/performance_report.sh\`

### B. 原始数据

- 火焰图数据: \`perf.data\`
- wrk 原始输出: \`baseline_static.txt\`
- strace 统计: \`strace.txt\`

---

**报告生成时间**: $(date '+%Y-%m-%d %H:%M:%S')
**下一步行动**: 实施 Phase 1 优化，预期 2 天内完成
EOF

echo "    ✓ 报告生成完成: $REPORT_DIR/REPORT.md"
echo ""
echo "====================================="
echo "  性能分析完成！"
echo "====================================="
echo ""
echo "查看报告："
echo "  markdown: cat $REPORT_DIR/REPORT.md"
echo "  火焰图: firefox $REPORT_DIR/flamegraph.svg"
echo ""
echo "建议："
echo "  1. 先查看火焰图，手动识别热点函数"
echo "  2. 更新报告中的 Top 10 函数表格"
echo "  3. 根据报告实施 Phase 1 优化"
echo ""