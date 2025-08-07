# WebSocket Client for C++11

一个基于C++11实现的完整WebSocket RFC 6455客户端，支持所有标准功能。

## 特性

- ✅ 完整的RFC 6455 WebSocket协议实现
- ✅ 支持WSS (WebSocket Secure) 连接
- ✅ 支持压缩/解压 (通过zlib，可选)
- ✅ 可配置的WebSocket参数和扩展
- ✅ 详细的错误处理和描述
- ✅ 线程安全的实现
- ✅ 支持ping/pong心跳机制
- ✅ 支持自定义头部和扩展
- ✅ 跨平台支持 (Windows, Linux, macOS)

## 依赖

- C++11 编译器
- OpenSSL (必需)
- zlib (可选，用于压缩)

## 编译

### 使用CMake

```bash
mkdir build
cd build
cmake ..
make
```

### 使用Makefile

```bash
make
```

### 安装依赖

**Ubuntu/Debian:**
```bash
make install-deps
```

**CentOS/RHEL:**
```bash
make install-deps-centos
```

**macOS:**
```bash
make install-deps-macos
```

## 使用方法

### 基本使用

```cpp
#include "websocket_client.hpp"
#include <iostream>

int main() {
    // 创建WebSocket客户端
    websocket::WebSocketClient client;
    
    // 设置回调函数
    client.setMessageCallback([](const std::string& message) {
        std::cout << "Received: " << message << std::endl;
    });
    
    client.setErrorCallback([](const websocket::WebSocketError& error) {
        std::cout << "Error: " << error.toString() << std::endl;
    });
    
    // 连接到WebSocket服务器
    if (client.connect("wss://echo.websocket.org")) {
        // 发送消息
        client.send("Hello, WebSocket!");
        
        // 等待一段时间
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // 断开连接
        client.disconnect();
    }
    
    return 0;
}
```

### 高级配置

```cpp
// 创建自定义配置
websocket::WebSocketConfig config;
config.setTimeout(5000);                    // 设置超时时间
config.enableCompression(true);             // 启用压缩
config.setCompressionLevel(6);              // 设置压缩级别
config.setPingInterval(30000);              // 设置ping间隔
config.addHeader("User-Agent", "MyClient"); // 添加自定义头部
config.addExtension("permessage-deflate", "client_max_window_bits=15");

// 使用配置创建客户端
websocket::WebSocketClient client(config);
```

### 状态监控

```cpp
client.setStateChangeCallback([](websocket::WebSocketState state) {
    switch (state) {
        case websocket::WebSocketState::CONNECTING:
            std::cout << "Connecting..." << std::endl;
            break;
        case websocket::WebSocketState::OPEN:
            std::cout << "Connected!" << std::endl;
            break;
        case websocket::WebSocketState::CLOSING:
            std::cout << "Closing..." << std::endl;
            break;
        case websocket::WebSocketState::CLOSED:
            std::cout << "Closed!" << std::endl;
            break;
    }
});
```

## API 参考

### WebSocketConfig

配置类，用于设置WebSocket客户端的各种参数。

#### 主要方法：

- `setTimeout(int timeout_ms)` - 设置连接超时时间
- `setMaxFrameSize(size_t size)` - 设置最大帧大小
- `enableCompression(bool enable)` - 启用/禁用压缩
- `setCompressionLevel(int level)` - 设置压缩级别 (0-9)
- `setPingInterval(int interval_ms)` - 设置ping间隔
- `addHeader(const std::string& key, const std::string& value)` - 添加自定义头部
- `addExtension(const std::string& name, const std::string& params)` - 添加扩展

### WebSocketClient

主要的WebSocket客户端类。

#### 主要方法：

- `connect(const std::string& url)` - 连接到WebSocket服务器
- `disconnect()` - 断开连接
- `send(const std::string& message)` - 发送文本消息
- `sendBinary(const std::string& data)` - 发送二进制数据
- `ping(const std::string& data)` - 发送ping
- `setMessageCallback(MessageCallback callback)` - 设置消息回调
- `setErrorCallback(ErrorCallback callback)` - 设置错误回调
- `setStateChangeCallback(StateChangeCallback callback)` - 设置状态变化回调

### 错误处理

所有错误都通过 `WebSocketError` 类表示：

```cpp
void errorCallback(const websocket::WebSocketError& error) {
    std::cout << "Error Code: " << static_cast<int>(error.code()) << std::endl;
    std::cout << "Error Message: " << error.message() << std::endl;
    std::cout << "Full Error: " << error.toString() << std::endl;
}
```

#### 错误码：

- `SUCCESS` - 成功
- `INVALID_URL` - 无效的URL
- `CONNECTION_FAILED` - 连接失败
- `HANDSHAKE_FAILED` - 握手失败
- `INVALID_FRAME` - 无效的帧
- `COMPRESSION_ERROR` - 压缩错误
- `SSL_ERROR` - SSL错误
- `TIMEOUT` - 超时
- `CLOSED` - 连接已关闭
- `INVALID_STATE` - 无效状态
- `BUFFER_OVERFLOW` - 缓冲区溢出
- `INVALID_PARAMETER` - 无效参数

## 编译选项

### 启用压缩

要启用zlib压缩，需要：

1. 安装zlib开发库
2. 定义 `USE_ZLIB` 宏

在CMake中：
```cmake
find_package(ZLIB)
if(ZLIB_FOUND)
    add_definitions(-DUSE_ZLIB)
    target_link_libraries(your_target ZLIB::ZLIB)
endif()
```

在Makefile中：
```makefile
CXXFLAGS += -DUSE_ZLIB
LIBS += -lz
```

## 示例

运行示例程序：

```bash
./websocket_example
```

示例程序会连接到 `wss://echo.websocket.org` 并发送测试消息。

## 注意事项

1. **线程安全**: 客户端是线程安全的，可以在多个线程中使用
2. **内存管理**: 所有内存管理都是自动的，无需手动释放
3. **错误处理**: 所有错误都会通过回调函数报告
4. **连接状态**: 使用状态回调监控连接状态
5. **压缩**: 压缩功能是可选的，需要zlib库支持

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request！
