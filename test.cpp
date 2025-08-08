#include "websocket_client.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>

class WebSocketTest {
private:
    std::atomic<int> message_count_{0};
    std::atomic<int> error_count_{0};

public:
    void runBasicTest() {
        std::cout << "=== Basic functionality test ===" << std::endl;

        websocket::WebSocketClient client;

        client.setOnText([this](const std::string &message) {
            std::cout << "Received: " << message << std::endl;
            message_count_++;
        });

        client.setOnError([this](const websocket::WebSocketResult &err) {
            std::cout << "Error: (" << static_cast<int>(err.code()) << ") " << err.message() << std::endl;
            error_count_++;
        });

        client.setOnOpen([]() {
            std::cout << "State: OPEN" << std::endl;
        });
        client.setOnClose([]() {
            std::cout << "State: CLOSED" << std::endl;
        });

        if (client.connect("wss://echo.websocket.org")) {
            std::cout << "Connected!" << std::endl;

            // send text
            client.send("Hello, WebSocket!");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // send binary
            client.sendBinary("Binary test data");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // ping
            client.ping("ping test");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // wait for responses
            std::this_thread::sleep_for(std::chrono::seconds(3));

            client.disconnect();
        } else {
            std::cout << "Connect failed!" << std::endl;
        }

        std::cout << "Messages: " << message_count_.load() << std::endl;
        std::cout << "Errors: " << error_count_.load() << std::endl;
    }

    void runCompressionTest() {
        std::cout << "\n=== Compression test ===" << std::endl;

        websocket::WebSocketConfig config;
        config.enableCompression(true);
        config.setCompressionLevel(6);

        websocket::WebSocketClient client(config);

        client.setOnText([this](const std::string &message) {
            std::cout << "Compressed message: " << message << std::endl;
            message_count_++;
        });

        client.setOnError([this](const websocket::WebSocketResult &err) {
            std::cout << "Compression error: (" << static_cast<int>(err.code()) << ") " << err.message() << std::endl;
            error_count_++;
        });

        if (client.connect("wss://echo.websocket.org")) {
            std::cout << "Compression connect OK" << std::endl;

            // send large data
            std::string large_data(1000, 'A');
            client.send(large_data);

            std::this_thread::sleep_for(std::chrono::seconds(2));
            client.disconnect();
        }
    }

    void runConfigurationTest() {
        std::cout << "\n=== Configuration test ===" << std::endl;

        websocket::WebSocketConfig config;
        config.setTimeout(10000);
        config.setMaxFrameSize(1024 * 1024);
        config.setPingInterval(15000);
        config.setPongTimeout(5000);
        config.addHeader("User-Agent", "WebSocket-Test/1.0");
        config.addHeader("X-Custom-Header", "test-value");
        config.addExtension("permessage-deflate", "client_max_window_bits=15");

        websocket::WebSocketClient client(config);

        client.setOnText([this](const std::string &message) {
            std::cout << "Config test message: " << message << std::endl;
            message_count_++;
        });

        client.setOnError([this](const websocket::WebSocketResult &err) {
            std::cout << "Config test error: (" << static_cast<int>(err.code()) << ") " << err.message() << std::endl;
            error_count_++;
        });

        if (client.connect("wss://echo.websocket.org")) {
            std::cout << "Config connect OK" << std::endl;

            const auto &current = client.getConfig();
            std::cout << "Timeout: " << current.getTimeout() << "ms" << std::endl;
            std::cout << "Compression: " << (current.isCompressionEnabled() ? "enabled" : "disabled") << std::endl;

            client.send("Configuration test message");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            client.disconnect();
        }
    }

    void runErrorHandlingTest() {
        std::cout << "\n=== Error handling test ===" << std::endl;

        websocket::WebSocketClient client;

        client.setOnError([this](const websocket::WebSocketResult &err) {
            std::cout << "Error handling: (" << static_cast<int>(err.code()) << ") " << err.message() << std::endl;
            error_count_++;
        });

        // invalid URL
        std::cout << "Invalid URL..." << std::endl;
        client.connect("invalid://url");

        // non-existent server
        std::cout << "Non-existent server..." << std::endl;
        client.connect("ws://nonexistent.server.com");

        // invalid scheme
        std::cout << "Invalid scheme..." << std::endl;
        client.connect("http://echo.websocket.org");
    }

    void runMultiClientTest() {
        std::cout << "\n=== Multi-client test ===" << std::endl;

        std::vector<std::unique_ptr<websocket::WebSocketClient>> clients;
        std::atomic<int> connected{0};

        for (int i = 0; i < 3; ++i) {
            std::unique_ptr<websocket::WebSocketClient> client(new websocket::WebSocketClient());

            client->setOnText([i](const std::string &message) {
                std::cout << "Client " << i << " got: " << message << std::endl;
            });
            client->setOnError([i](const websocket::WebSocketResult &err) {
                std::cout << "Client " << i << " error: (" << static_cast<int>(err.code()) << ") " << err.message() << std::endl;
            });
            client->setOnOpen([&connected, i]() {
                connected++;
                std::cout << "Client " << i << " connected, total: " << connected.load() << std::endl;
            });

            clients.push_back(std::move(client));
        }

        for (auto &c : clients) c->connect("wss://echo.websocket.org");

        std::this_thread::sleep_for(std::chrono::seconds(2));

        for (size_t i = 0; i < clients.size(); ++i) {
            clients[i]->send("Multi-client test message " + std::to_string(i));
        }

        std::this_thread::sleep_for(std::chrono::seconds(3));

        for (auto &c : clients) c->disconnect();

        std::cout << "Multi-client test finished, connected: " << connected.load() << std::endl;
    }

    void runAllTests() {
        std::cout << "Start WebSocket client tests..." << std::endl;

        runBasicTest();
        runCompressionTest();
        runConfigurationTest();
        runErrorHandlingTest();
        runMultiClientTest();

        std::cout << "\n=== Test summary ===" << std::endl;
        std::cout << "Total messages: " << message_count_.load() << std::endl;
        std::cout << "Total errors: " << error_count_.load() << std::endl;
        std::cout << "All tests done!" << std::endl;
    }
};

int main() {
    WebSocketTest test;
    test.runAllTests();
    return 0;
}