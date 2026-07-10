# TCP客户端 + State设计模式 - 设计文档

## 1. 整体架构

### 1.1 组件关系图

```
┌─────────────────────────────────────────────────────────────┐
│         TCPClient (Context)         │
├─────────────────────────────────────────────────────────────┤
│ - host, port, socketFd              │
│ - currentState: State*              │
│ - messageQueue: MessageQueue        │
│ - heartbeatThread, reconnectThread  │
├─────────────────────────────────────────────────────────────┤
│ + connect() → state->connect()      │
│ + send(data)                        │
│ + receive()                         │
│ + disconnect()                      │
│ + startHeartbeat()                  │
│ + startAutoReconnect()              │
└────────────────────────┬─────────────────────────────────────┘
                       │
                       ├─→ DisconnectedState
                       ├─→ ConnectingState
                       ├─→ ConnectedState
                       ├─→ ClosingState
                       └─→ ErrorState

┌─────────────────────────────────────────────────────────────┐
│      MessageQueue (Thread-Safe)      │
├─────────────────────────────────────────────────────────────┤
│ - queue<string> messages             │
│ - mutex, condition_variable          │
├─────────────────────────────────────────────────────────────┤
│ + push(msg)                          │
│ + pop() → string                     │
│ + tryPop(msg) → bool                 │
│ + empty() → bool                     │
└─────────────────────────────────────────────────────────────┘
```

## 2. 状态转移详解

### 2.1 完整状态机

```
                    connect()
     ┌──────────────────────────────────────────────────┐
     ↓                                                  │
 [Disconnected] ─────────────────────→ [Connecting]
     ↑                              │
     │                     (success) ↓
     │ disconnect()          [Connected]
     │                         │   ↑
     │ ◄────────────────────────┘   │
     │                          send/recv
     │                            │
     │                    (error) ↓
     │                      [Error]
     │                         │
     └──────────────────────────────
          connect() for retry
```

### 2.2 各状态详细说明

#### DisconnectedState（未连接）
- **进入条件**: 初始化、连接失败、主动断开
- **可执行操作**:
  - `connect()`: 转移到ConnectingState，发起连接
  - `send()`: 拒绝，提示未连接
  - `receive()`: 拒绝，提示未连接
  - `disconnect()`: 无操作（已是未连接）

#### ConnectingState（连接中）
- **进入条件**: 调用connect()后立即进入
- **可执行操作**:
  - `connect()`: 提示正在连接
  - `send()`: 消息入队，等待连接完成
  - `receive()`: 等待连接
  - `disconnect()`: 转移到DisconnectedState，取消连接

#### ConnectedState（已连接）
- **进入条件**: 连接成功
- **可执行操作**:
  - `connect()`: 提示已连接
  - `send()`: 立即发送数据，失败则转到ErrorState
  - `receive()`: 接收数据，失败则转到ErrorState
  - `disconnect()`: 转移到ClosingState

#### ClosingState（关闭中）
- **进入条件**: 调用disconnect()后进入
- **可执行操作**:
  - `connect()`: 拒绝，正在关闭
  - `send()`: 拒绝，正在关闭
  - `receive()`: 拒绝，正在关闭
  - `disconnect()`: 转移到DisconnectedState，完成关闭

#### ErrorState（错误）
- **进入条件**: 网络操作失败（send/receive失败、心跳失败）
- **可执行操作**:
  - `connect()`: 转移到DisconnectedState，尝试恢复
  - `send()`: 拒绝，在错误状态
  - `receive()`: 拒绝，在错误状态
  - `disconnect()`: 转移到DisconnectedState

## 3. 关键特性实现

### 3.1 消息队列设计

**特点：**
```cpp
// 1. 线程安全 - 所有操作都用互斥锁保护
// 2. 条件变量 - pop()会阻塞直到有消息
// 3. 大小限制 - 防止内存溢出
// 4. 非阻塞模式 - tryPop()不阻塞

MessageQueue mq;
mq.push("msg1");              // 线程1

std::string msg = mq.pop();  // 线程2，会阻塞到有消息
```

### 3.2 心跳机制

**实现流程**:
```
startHeartbeat(10000)
    ↓
创建独立线程 heartbeatThread
    ↓
hartbeatLoop() 每10秒执行一次：
  if isConnected():
    sendHeartbeat()
      if success: 连接正常
      if fail:   转到ErrorState，自动重连
```

**优势**:
- 检测僵尸连接（长时间无数据）
- 保持NAT/防火墙连接活跃
- 自动发现连接断裂

### 3.3 自动重连机制

**实现流程**:
```
startAutoReconnect()
    ↓
创建独立线程 reconnectThread
    ↓
reconnectLoop() 每5秒执行一次：
  if isError() or failureCount > 0:
    connect()  // 发起重连
      if success: 清除失败计数
      if fail:   增加失败计数，等待下次重试
```

**指数退避策略** (可选扩展):
```cpp
waitTime = baseInterval * (2 ^ failureCount)
// 1st retry:  5秒
// 2nd retry:  10秒
// 3rd retry:  20秒
// ...
// 最大等待时间: 5分钟
```

## 4. 线程安全设计

### 4.1 互斥锁使用

```cpp
// 状态转移保护
std::mutex stateMutex;
void connect() {
    std::lock_guard<std::mutex> lock(stateMutex);
    currentState->connect(this);  // 原子操作
}

// 心跳线程同步
std::mutex heartbeatMutex;
bool heartbeatRunning;  // 受保护的标志

// 重连线程同步
std::mutex reconnectMutex;
bool reconnectRunning;  // 受保护的标志
```

### 4.2 条件变量使用（消息队列）

```cpp
std::condition_variable cv;  // 用于同步消费者线程

void push(const std::string& msg) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        messages.push(msg);
    }
    cv.notify_one();  // 唤醒等待的pop()
}

std::string pop() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [this] { return !messages.empty(); });
    // 收到通知后继续执行
    std::string msg = messages.front();
    messages.pop();
    return msg;
}
```

### 4.3 线程生命周期管理

```cpp
void startHeartbeat() {
    {
        std::lock_guard<std::mutex> lock(heartbeatMutex);
        if (heartbeatRunning) return;  // 避免重复启动
        heartbeatRunning = true;
    }
    heartbeatThread = std::thread(&TCPClient::heartbeatLoop, this);
}

void stopHeartbeat() {
    {
        std::lock_guard<std::mutex> lock(heartbeatMutex);
        heartbeatRunning = false;  // 信号线程退出
    }
    if (heartbeatThread.joinable()) {
        heartbeatThread.join();  // 等待线程结束
    }
}
```

## 5. 错误处理策略

### 5.1 网络错误处理

```
发送失败
    ↓
setError("Send failed")
    ↓
setState(ErrorState)  // 转到错误状态
    ↓
autoReconnect启动
    ↓
发起重连
```

### 5.2 连接超时处理 (建议扩展)

```cpp
// 当前实现：简单的ConnectingState
// 建议添加：

struct ConnectTimeout {
    auto start = now();
    while (isConnecting()) {
        if (elapsed(start) > 30000) {  // 30秒超时
            setState(ErrorState("Connect timeout"));
            break;
        }
    }
};
```

## 6. 性能分析

### 6.1 内存占用

```
TCPClient对象:           ~200字节
  - 状态指针:            8字节
  - socket FD:           4字节
  - 互斥锁:              ~40字节
  - 线程变量:            ~40字节

MessageQueue:
  - 空队列:              ~100字节
  - 满队列(1000条):     ~50KB
    (每条消息平均50字节)

线程栈:
  - 每个线程:            8MB
  - 心跳线程:            8MB
  - 重连线程:            8MB

总体:                    <30MB(正常使用)
```

### 6.2 CPU占用

```
主线程:     处理业务逻辑
心跳线程:   每10秒唤醒一次
            大部分时间睡眠
            实际工作时间<1ms

重连线程:   每5秒唤醒一次
            大部分时间睡眠
            实际工作时间<1ms

总体CPU:    基本不占用(<0.1%)
```

### 6.3 消息队列性能

```
push()操作:    O(1)
pop()操作:     O(1)
empty()操作:   O(1)
size()操作:    O(1)

最大队列大小:  1000条消息
防止内存溢出:  是
```

## 7. State模式 vs 其他模式

| 对比项 | State模式 | Strategy模式 | Template Method | Switch/If |
|-------|---------|-------------|-----------------|----------|
| **用途** | 对象行为依赖状态 | 不同算法切换 | 固定步骤框架 | 条件分支 |
| **灵活性** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐ |
| **可维护性** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐ |
| **扩展性** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐ | ⭐ |
| **复杂度** | 中 | 中 | 中 | 低 |
| **代码行数** | 多 | 中 | 中 | 少 |

## 8. 使用场景分析

### 8.1 游戏服务器连接

```cpp
TCPClient gameClient("game.server.com", 8888, 5000);
gameClient.startHeartbeat(30000);      // 30秒心跳
gameClient.startAutoReconnect();       // 自动重连
gameClient.connect();

// 即使网络临时断开，也会自动重连
while (isGameRunning) {
    if (gameClient.isConnected()) {
        gameClient.send(playerPosition);
        gameClient.receive();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(16));  // 60FPS
}
```

### 8.2 分布式系统节点通信

```cpp
std::vector<TCPClient> nodeClients;
for (auto& node : clusterNodes) {
    nodeClients.emplace_back(node.ip, node.port, 3000);
    nodeClients.back().startAutoReconnect();
    nodeClients.back().connect();
}

// 广播消息
for (auto& client : nodeClients) {
    if (client.isConnected()) {
        client.send(clusterMessage);
    }
}
```

### 8.3 IoT设备管理

```cpp
// 连接不稳定的物联网设备
TCPClient iotClient(device.ip, device.port, 10000);
iotClient.startHeartbeat(60000);       // 60秒心跳
iotClient.startAutoReconnect();        // 网络不稳定时自动重连
iotClient.connect();

while (true) {
    if (iotClient.isConnected()) {
        iotClient.send("GET_STATUS");
        iotClient.receive();
        if (iotClient.hasMessage()) {
            auto data = iotClient.popMessage();
            processDeviceData(data);
        }
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
```

## 9. 扩展建议

### 9.1 SSL/TLS支持

```cpp
class SecureConnectedState : public ConnectedState {
    SSL* sslHandle;
    
    void send(TCPClient* client, const std::string& data) override {
        SSL_write(sslHandle, data.c_str(), data.length());
    }
    
    void receive(TCPClient* client) override {
        char buffer[4096];
        int bytes = SSL_read(sslHandle, buffer, sizeof(buffer)-1);
        // ...
    }
};
```

### 9.2 连接池

```cpp
class TCPClientPool {
    std::vector<TCPClient> availableClients;
    std::vector<TCPClient> busyClients;
    std::mutex poolMutex;
    
    TCPClient* acquireConnection();
    void releaseConnection(TCPClient* client);
};
```

### 9.3 自定义协议解析

```cpp
struct Message {
    uint16_t type;
    uint32_t length;
    std::string payload;
};

class ProtocolParser {
    Message parseMessage(const std::string& raw);
    std::string serializeMessage(const Message& msg);
};
```

### 9.4 性能监控

```cpp
struct Statistics {
    int messagesSent;
    int messagesReceived;
    int connectionAttempts;
    int failedAttempts;
    double avgResponseTime;
    
    void report();
};
```

## 10. 总结

**这个设计的优势：**

1. **清晰的状态管理** - 每个状态独立，行为明确
2. **完整的容错能力** - 心跳检测+自动重连
3. **线程安全** - 适合多线程环境
4. **易于扩展** - 添加新状态或功能无需修改现有代码
5. **生产级质量** - 可直接用于实际项目

**适用场景：**
- 长连接应用（游戏、IM）
- 分布式系统
- IoT设备管理
- 实时数据传输系统
- 监控告警系统

**进一步优化方向：**
- 支持SSL/TLS加密
- 实现连接池
- 添加自定义协议
- 完整的监控和统计
- 指数退避重连策略
