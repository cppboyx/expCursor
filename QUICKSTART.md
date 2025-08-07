# WebSocket Client 快速开始指南

## 5分钟快速开始

### 1. 安装依赖

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y libssl-dev zlib1g-dev build-essential
```

**CentOS/RHEL:**
```bash
sudo yum install -y openssl-devel zlib-devel gcc-c++ make
```

**macOS:**
```bash
brew install openssl zlib
```

### 2. 编译项目

```bash
# 使用自动编译脚本（推荐）
./build.sh

# 或者使用CMake
mkdir build && cd build
cmake ..
make

# 或者使用Makefile
make
```

### 3. 运行示例

```bash
# 运行基本示例
./websocket_example

# 运行功能测试
./websocket_test

# 运行性能测试
./websocket_performance
```

## 基本使用

### 最简单的使用方式

```cpp
#include "websocket_client.hpp"
#include <iostream>

int main() {
    // 创建WebSocket客户端
    websocket::WebSocketClient client;
    
    // 设置消息回调
    client.setMessageCallback([](const std::string& message) {
        std::cout << "收到消息: " << message << std::endl;
    });
    
    // 连接到WebSocket服务器
    if (client.connect("wss://echo.websocket.org")) {
        // 发送消息
        client.send("Hello, WebSocket!");
        
        // 等待5秒
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // 断开连接
        client.disconnect();
    }
    
    return 0;
}
```

### 高级配置

```cpp
#include "websocket_client.hpp"
#include <iostream>

int main() {
    // 创建配置
    websocket::WebSocketConfig config;
    config.setTimeout(5000);                    // 5秒超时
    config.enableCompression(true);             // 启用压缩
    config.setCompressionLevel(6);              // 压缩级别
    config.setPingInterval(30000);              // 30秒ping间隔
    config.addHeader("User-Agent", "MyClient"); // 自定义头部
    
    // 创建客户端
    websocket::WebSocketClient client(config);
    
    // 设置回调
    client.setMessageCallback([](const std::string& message) {
        std::cout << "收到: " << message << std::endl;
    });
    
    client.setErrorCallback([](const websocket::WebSocketError& error) {
        std::cout << "错误: " << error.toString() << std::endl;
    });
    
    client.setStateChangeCallback([](websocket::WebSocketState state) {
        switch (state) {
            case websocket::WebSocketState::OPEN:
                std::cout << "连接已建立" << std::endl;
                break;
            case websocket::WebSocketState::CLOSED:
                std::cout << "连接已关闭" << std::endl;
                break;
        }
    });
    
    // 连接并发送消息
    if (client.connect("wss://echo.websocket.org")) {
        client.send("Hello with compression!");
        client.sendBinary("Binary data");
        client.ping("ping test");
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
        client.disconnect();
    }
    
    return 0;
}
```

## 常见用例

### 1. 实时聊天客户端

```cpp
#include "websocket_client.hpp"
#include <iostream>
#include <string>

int main() {
    websocket::WebSocketClient client;
    
    client.setMessageCallback([](const std::string& message) {
        std::cout << "聊天消息: " << message << std::endl;
    });
    
    if (client.connect("ws://your-chat-server.com/chat")) {
        std::string input;
        while (std::getline(std::cin, input)) {
            if (input == "quit") break;
            client.send(input);
        }
        client.disconnect();
    }
    
    return 0;
}
```

### 2. 数据流处理

```cpp
#include "websocket_client.hpp"
#include <iostream>
#include <vector>

int main() {
    websocket::WebSocketConfig config;
    config.enableCompression(true);
    config.setCompressionLevel(9); // 最高压缩级别
    
    websocket::WebSocketClient client(config);
    
    client.setMessageCallback([](const std::string& message) {
        // 处理接收到的数据
        std::cout << "处理数据: " << message.length() << " 字节" << std::endl;
    });
    
    if (client.connect("wss://your-data-stream.com")) {
        // 发送大量数据
        std::string large_data(10000, 'A');
        for (int i = 0; i < 100; ++i) {
            client.send(large_data);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(10));
        client.disconnect();
    }
    
    return 0;
}
```

### 3. 多客户端管理

```cpp
#include "websocket_client.hpp"
#include <iostream>
#include <vector>
#include <memory>

int main() {
    std::vector<std::unique_ptr<websocket::WebSocketClient>> clients;
    
    // 创建多个客户端
    for (int i = 0; i < 5; ++i) {
        auto client = std::make_unique<websocket::WebSocketClient>();
        
        client->setMessageCallback([i](const std::string& message) {
            std::cout << "客户端 " << i << " 收到: " << message << std::endl;
        });
        
        client->setErrorCallback([i](const websocket::WebSocketError& error) {
            std::cout << "客户端 " << i << " 错误: " << error.toString() << std::endl;
        });
        
        clients.push_back(std::move(client));
    }
    
    // 连接所有客户端
    for (auto& client : clients) {
        client->connect("wss://echo.websocket.org");
    }
    
    // 发送消息
    for (size_t i = 0; i < clients.size(); ++i) {
        clients[i]->send("Message from client " + std::to_string(i));
    }
    
    // 等待响应
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // 断开所有连接
    for (auto& client : clients) {
        client->disconnect();
    }
    
    return 0;
}
```

## 错误处理

### 完整的错误处理示例

```cpp
#include "websocket_client.hpp"
#include <iostream>

int main() {
    websocket::WebSocketClient client;
    
    client.setErrorCallback([](const websocket::WebSocketError& error) {
        switch (error.code()) {
            case websocket::ErrorCode::CONNECTION_FAILED:
                std::cout << "连接失败，请检查网络和服务器状态" << std::endl;
                break;
            case websocket::ErrorCode::HANDSHAKE_FAILED:
                std::cout << "握手失败，可能是服务器不支持WebSocket" << std::endl;
                break;
            case websocket::ErrorCode::TIMEOUT:
                std::cout << "连接超时，请检查网络延迟" << std::endl;
                break;
            case websocket::ErrorCode::SSL_ERROR:
                std::cout << "SSL错误，请检查证书和加密设置" << std::endl;
                break;
            default:
                std::cout << "未知错误: " << error.message() << std::endl;
                break;
        }
    });
    
    if (!client.connect("wss://invalid-server.com")) {
        std::cout << "连接失败" << std::endl;
    }
    
    return 0;
}
```

## 性能优化

### 1. 压缩配置

```cpp
websocket::WebSocketConfig config;
config.enableCompression(true);
config.setCompressionLevel(6); // 平衡压缩率和性能
```

### 2. 超时设置

```cpp
config.setTimeout(5000);        // 连接超时
config.setPingInterval(30000);  // ping间隔
config.setPongTimeout(10000);   // pong超时
```

### 3. 帧大小限制

```cpp
config.setMaxFrameSize(1024 * 1024); // 1MB最大帧大小
```

## 编译选项

### 启用压缩
```bash
# 使用CMake
cmake -DUSE_ZLIB=ON ..

# 使用Makefile
make CXXFLAGS="-DUSE_ZLIB" LIBS="-lz"
```

### 调试模式
```bash
# 使用CMake
cmake -DCMAKE_BUILD_TYPE=Debug ..

# 使用Makefile
make CXXFLAGS="-g -O0"
```

## 故障排除

### 编译问题
1. **找不到OpenSSL**: 安装 `libssl-dev` (Ubuntu) 或 `openssl-devel` (CentOS)
2. **找不到zlib**: 安装 `zlib1g-dev` (Ubuntu) 或 `zlib-devel` (CentOS)
3. **编译器版本**: 确保使用支持C++11的编译器

### 运行时问题
1. **连接失败**: 检查网络连接和URL格式
2. **SSL错误**: 检查证书和加密设置
3. **性能问题**: 调整压缩级别和超时设置

## 下一步

- 查看 [README.md](README.md) 了解完整功能
- 阅读 [DOCUMENTATION.md](DOCUMENTATION.md) 获取详细文档
- 运行测试程序验证功能
- 根据需求调整配置参数