#include "websocket_client.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

class WebSocketTest {
private:
    std::atomic<int> message_count_{0};
    std::atomic<int> error_count_{0};
    std::atomic<bool> test_completed_{false};

public:
    void runBasicTest() {
        std::cout << "=== 基本功能测试 ===" << std::endl;
        
        websocket::WebSocketClient client;
        
        client.setMessageCallback([this](const std::string& message) {
            std::cout << "收到消息: " << message << std::endl;
            message_count_++;
        });
        
        client.setErrorCallback([this](const websocket::WebSocketError& error) {
            std::cout << "错误: " << error.toString() << std::endl;
            error_count_++;
        });
        
        client.setStateChangeCallback([](websocket::WebSocketState state) {
            std::string state_str;
            switch (state) {
                case websocket::WebSocketState::CONNECTING:
                    state_str = "连接中";
                    break;
                case websocket::WebSocketState::OPEN:
                    state_str = "已连接";
                    break;
                case websocket::WebSocketState::CLOSING:
                    state_str = "关闭中";
                    break;
                case websocket::WebSocketState::CLOSED:
                    state_str = "已关闭";
                    break;
            }
            std::cout << "状态变化: " << state_str << std::endl;
        });
        
        if (client.connect("wss://echo.websocket.org")) {
            std::cout << "连接成功！" << std::endl;
            
            // 发送文本消息
            client.send("Hello, WebSocket!");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // 发送二进制数据
            client.sendBinary("Binary test data");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // 发送ping
            client.ping("ping test");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // 等待接收响应
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            client.disconnect();
        } else {
            std::cout << "连接失败！" << std::endl;
        }
        
        std::cout << "消息计数: " << message_count_.load() << std::endl;
        std::cout << "错误计数: " << error_count_.load() << std::endl;
    }
    
    void runCompressionTest() {
        std::cout << "\n=== 压缩功能测试 ===" << std::endl;
        
        websocket::WebSocketConfig config;
        config.enableCompression(true);
        config.setCompressionLevel(6);
        
        websocket::WebSocketClient client(config);
        
        client.setMessageCallback([this](const std::string& message) {
            std::cout << "收到压缩消息: " << message << std::endl;
            message_count_++;
        });
        
        client.setErrorCallback([this](const websocket::WebSocketError& error) {
            std::cout << "压缩测试错误: " << error.toString() << std::endl;
            error_count_++;
        });
        
        if (client.connect("wss://echo.websocket.org")) {
            std::cout << "压缩连接成功！" << std::endl;
            
            // 发送大量数据测试压缩
            std::string large_data(1000, 'A');
            client.send(large_data);
            
            std::this_thread::sleep_for(std::chrono::seconds(2));
            client.disconnect();
        }
    }
    
    void runConfigurationTest() {
        std::cout << "\n=== 配置功能测试 ===" << std::endl;
        
        websocket::WebSocketConfig config;
        config.setTimeout(10000);
        config.setMaxFrameSize(1024 * 1024);
        config.setPingInterval(15000);
        config.setPongTimeout(5000);
        config.addHeader("User-Agent", "WebSocket-Test/1.0");
        config.addHeader("X-Custom-Header", "test-value");
        config.addExtension("permessage-deflate", "client_max_window_bits=15");
        
        websocket::WebSocketClient client(config);
        
        client.setMessageCallback([this](const std::string& message) {
            std::cout << "配置测试消息: " << message << std::endl;
            message_count_++;
        });
        
        client.setErrorCallback([this](const websocket::WebSocketError& error) {
            std::cout << "配置测试错误: " << error.toString() << std::endl;
            error_count_++;
        });
        
        if (client.connect("wss://echo.websocket.org")) {
            std::cout << "配置测试连接成功！" << std::endl;
            
            // 测试配置是否正确应用
            const auto& current_config = client.getConfig();
            std::cout << "当前超时设置: " << current_config.getTimeout() << "ms" << std::endl;
            std::cout << "压缩是否启用: " << (current_config.isCompressionEnabled() ? "是" : "否") << std::endl;
            
            client.send("Configuration test message");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            client.disconnect();
        }
    }
    
    void runErrorHandlingTest() {
        std::cout << "\n=== 错误处理测试 ===" << std::endl;
        
        websocket::WebSocketClient client;
        
        client.setErrorCallback([this](const websocket::WebSocketError& error) {
            std::cout << "错误处理测试 - " << error.toString() << std::endl;
            error_count_++;
        });
        
        // 测试无效URL
        std::cout << "测试无效URL..." << std::endl;
        client.connect("invalid://url");
        
        // 测试不存在的服务器
        std::cout << "测试不存在的服务器..." << std::endl;
        client.connect("ws://nonexistent.server.com");
        
        // 测试无效的WebSocket URL
        std::cout << "测试无效的WebSocket URL..." << std::endl;
        client.connect("http://echo.websocket.org");
    }
    
    void runMultiClientTest() {
        std::cout << "\n=== 多客户端测试 ===" << std::endl;
        
        std::vector<std::unique_ptr<websocket::WebSocketClient>> clients;
        std::atomic<int> connected_clients{0};
        
        // 创建多个客户端
        for (int i = 0; i < 3; ++i) {
            auto client = std::make_unique<websocket::WebSocketClient>();
            
            client->setMessageCallback([i](const std::string& message) {
                std::cout << "客户端 " << i << " 收到: " << message << std::endl;
            });
            
            client->setErrorCallback([i](const websocket::WebSocketError& error) {
                std::cout << "客户端 " << i << " 错误: " << error.toString() << std::endl;
            });
            
            client->setStateChangeCallback([i, &connected_clients](websocket::WebSocketState state) {
                if (state == websocket::WebSocketState::OPEN) {
                    connected_clients++;
                    std::cout << "客户端 " << i << " 已连接，总连接数: " << connected_clients.load() << std::endl;
                }
            });
            
            clients.push_back(std::move(client));
        }
        
        // 同时连接所有客户端
        for (auto& client : clients) {
            client->connect("wss://echo.websocket.org");
        }
        
        // 等待连接建立
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // 发送消息
        for (size_t i = 0; i < clients.size(); ++i) {
            clients[i]->send("Multi-client test message " + std::to_string(i));
        }
        
        // 等待响应
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // 断开所有连接
        for (auto& client : clients) {
            client->disconnect();
        }
        
        std::cout << "多客户端测试完成，成功连接: " << connected_clients.load() << " 个客户端" << std::endl;
    }
    
    void runAllTests() {
        std::cout << "开始WebSocket客户端测试..." << std::endl;
        
        runBasicTest();
        runCompressionTest();
        runConfigurationTest();
        runErrorHandlingTest();
        runMultiClientTest();
        
        std::cout << "\n=== 测试总结 ===" << std::endl;
        std::cout << "总消息数: " << message_count_.load() << std::endl;
        std::cout << "总错误数: " << error_count_.load() << std::endl;
        std::cout << "所有测试完成！" << std::endl;
    }
};

int main() {
    WebSocketTest test;
    test.runAllTests();
    return 0;
}