# WebSocket Client 项目文档

## 项目结构

```
.
├── websocket_client.hpp      # 主要的WebSocket客户端头文件
├── example.cpp              # 基本使用示例
├── test.cpp                 # 功能测试程序
├── performance_test.cpp      # 性能测试程序
├── CMakeLists.txt           # CMake构建配置
├── Makefile                 # Makefile构建配置
├── build.sh                 # 自动编译脚本
├── README.md                # 项目说明
└── DOCUMENTATION.md         # 详细文档
```

## 核心组件

### 1. WebSocketConfig 类
配置类，用于设置WebSocket客户端的各种参数。

**主要功能：**
- 超时设置
- 压缩配置
- Ping/Pong 参数
- 自定义头部
- 扩展支持

**使用示例：**
```cpp
websocket::WebSocketConfig config;
config.setTimeout(5000);
config.enableCompression(true);
config.setCompressionLevel(6);
config.addHeader("User-Agent", "MyClient");
```

### 2. WebSocketClient 类
主要的WebSocket客户端类，提供完整的WebSocket功能。

**主要功能：**
- 连接管理
- 消息发送/接收
- 错误处理
- 状态监控
- 线程安全

**使用示例：**
```cpp
websocket::WebSocketClient client(config);
client.setMessageCallback([](const std::string& msg) {
    std::cout << "收到: " << msg << std::endl;
});
client.connect("wss://echo.websocket.org");
```

### 3. 错误处理系统
完整的错误处理机制，提供详细的错误信息。

**错误类型：**
- `INVALID_URL` - 无效URL
- `CONNECTION_FAILED` - 连接失败
- `HANDSHAKE_FAILED` - 握手失败
- `INVALID_FRAME` - 无效帧
- `COMPRESSION_ERROR` - 压缩错误
- `SSL_ERROR` - SSL错误
- `TIMEOUT` - 超时
- `CLOSED` - 连接关闭
- `INVALID_STATE` - 无效状态
- `BUFFER_OVERFLOW` - 缓冲区溢出
- `INVALID_PARAMETER` - 无效参数

### 4. 压缩支持
通过zlib库提供可选的压缩功能。

**特性：**
- 可配置的压缩级别 (0-9)
- 自动压缩/解压
- 通过宏控制启用/禁用

## 技术实现

### 1. 网络层
- 使用标准socket API
- 支持TCP和SSL/TLS连接
- 跨平台支持 (Windows, Linux, macOS)

### 2. WebSocket协议
- 完整的RFC 6455实现
- 支持所有标准帧类型
- 自动掩码处理
- 握手协议实现

### 3. 线程安全
- 多线程设计
- 异步消息处理
- 线程安全的回调机制

### 4. 内存管理
- RAII设计模式
- 自动资源管理
- 无内存泄漏

## 编译选项

### 必需依赖
- C++11 编译器
- OpenSSL 开发库

### 可选依赖
- zlib 开发库 (用于压缩)

### 编译方式

**使用CMake：**
```bash
mkdir build && cd build
cmake ..
make
```

**使用Makefile：**
```bash
make
```

**使用自动脚本：**
```bash
./build.sh
```

## 测试

### 功能测试
```bash
./websocket_test
```

测试内容：
- 基本连接功能
- 消息发送/接收
- 压缩功能
- 错误处理
- 多客户端测试

### 性能测试
```bash
./websocket_performance
```

测试内容：
- 延迟测试
- 吞吐量测试
- 压缩性能对比
- 内存使用测试

### 示例程序
```bash
./websocket_example
```

演示基本使用方法。

## 性能特性

### 1. 延迟
- 低延迟连接建立
- 快速消息处理
- 优化的帧解析

### 2. 吞吐量
- 高并发支持
- 高效的内存使用
- 优化的网络I/O

### 3. 内存使用
- 紧凑的对象设计
- 智能指针管理
- 最小化内存占用

### 4. 压缩效率
- 可配置的压缩级别
- 高效的压缩算法
- 自动压缩/解压

## 最佳实践

### 1. 错误处理
```cpp
client.setErrorCallback([](const websocket::WebSocketError& error) {
    switch (error.code()) {
        case websocket::ErrorCode::CONNECTION_FAILED:
            // 处理连接失败
            break;
        case websocket::ErrorCode::TIMEOUT:
            // 处理超时
            break;
        default:
            // 处理其他错误
            break;
    }
});
```

### 2. 状态监控
```cpp
client.setStateChangeCallback([](websocket::WebSocketState state) {
    switch (state) {
        case websocket::WebSocketState::OPEN:
            // 连接已建立
            break;
        case websocket::WebSocketState::CLOSED:
            // 连接已关闭
            break;
    }
});
```

### 3. 配置优化
```cpp
websocket::WebSocketConfig config;
config.setTimeout(5000);           // 设置合理的超时时间
config.setPingInterval(30000);     // 设置心跳间隔
config.enableCompression(true);    // 启用压缩以提高效率
config.setCompressionLevel(6);     // 设置平衡的压缩级别
```

### 4. 资源管理
```cpp
{
    websocket::WebSocketClient client;
    // 使用客户端
    // 自动析构和清理
}
```

## 故障排除

### 常见问题

1. **编译错误：找不到OpenSSL**
   - 安装OpenSSL开发库
   - 确保pkg-config能找到OpenSSL

2. **连接失败**
   - 检查网络连接
   - 验证URL格式
   - 检查防火墙设置

3. **压缩功能不可用**
   - 安装zlib开发库
   - 确保定义了USE_ZLIB宏

4. **性能问题**
   - 调整压缩级别
   - 优化消息大小
   - 检查网络延迟

### 调试技巧

1. **启用详细日志**
   - 设置错误回调来捕获详细信息
   - 监控状态变化

2. **性能分析**
   - 使用性能测试程序
   - 监控内存使用

3. **网络调试**
   - 使用网络分析工具
   - 检查WebSocket握手过程

## 扩展开发

### 1. 添加新的扩展
```cpp
config.addExtension("my-extension", "param1=value1;param2=value2");
```

### 2. 自定义头部
```cpp
config.addHeader("X-Custom-Header", "custom-value");
```

### 3. 自定义配置
```cpp
class CustomConfig : public websocket::WebSocketConfig {
    // 添加自定义配置项
};
```

## 许可证

MIT License - 详见LICENSE文件。

## 贡献

欢迎提交Issue和Pull Request！

### 开发环境设置
1. 安装依赖
2. 克隆代码
3. 运行测试
4. 提交更改

### 代码规范
- 遵循C++11标准
- 使用清晰的命名约定
- 添加适当的注释
- 确保代码可读性