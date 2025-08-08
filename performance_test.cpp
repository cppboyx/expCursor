#include "websocket_client.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <iomanip>
#include <memory>

class PerformanceTest {
private:
    std::atomic<int> messages_sent_{0};
    std::atomic<int> messages_received_{0};
    std::atomic<int> errors_{0};
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;

public:
    void runLatencyTest() {
        std::cout << "=== Latency test ===" << std::endl;

        websocket::WebSocketClient client;

        client.setOnText([this](const std::string &message) {
            (void)message;
            messages_received_++;
        });

        client.setOnError([this](const websocket::WebSocketResult &err) {
            errors_++;
            std::cout << "Error: (" << static_cast<int>(err.code()) << ") " << err.message() << std::endl;
        });

        if (client.connect("wss://echo.websocket.org")) {
            std::cout << "Connected, start latency test..." << std::endl;

            start_time_ = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < 100; ++i) {
                std::string message = "Latency test message " + std::to_string(i);
                if (client.send(message)) {
                    messages_sent_++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            std::this_thread::sleep_for(std::chrono::seconds(5));

            end_time_ = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - start_time_);

            std::cout << "Latency test result:" << std::endl;
            std::cout << "Messages sent: " << messages_sent_.load() << std::endl;
            std::cout << "Messages received: " << messages_received_.load() << std::endl;
            std::cout << "Errors: " << errors_.load() << std::endl;
            std::cout << "Total time: " << duration.count() << "ms" << std::endl;
            if (messages_sent_.load() > 0) {
                std::cout << "Avg latency: " << (duration.count() / messages_sent_.load()) << " ms/msg" << std::endl;
            }

            client.disconnect();
        }
    }

    void runThroughputTest() {
        std::cout << "\n=== Throughput test ===" << std::endl;

        websocket::WebSocketClient client;

        client.setOnText([this](const std::string &message) {
            (void)message;
            messages_received_++;
        });

        client.setOnError([this](const websocket::WebSocketResult &err) {
            (void)err;
            errors_++;
        });

        if (client.connect("wss://echo.websocket.org")) {
            std::cout << "Connected, start throughput test..." << std::endl;

            start_time_ = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < 1000; ++i) {
                std::string message = "Throughput test " + std::to_string(i);
                if (client.send(message)) {
                    messages_sent_++;
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(10));

            end_time_ = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - start_time_);

            std::cout << "Throughput test result:" << std::endl;
            std::cout << "Messages sent: " << messages_sent_.load() << std::endl;
            std::cout << "Messages received: " << messages_received_.load() << std::endl;
            std::cout << "Errors: " << errors_.load() << std::endl;
            std::cout << "Total time: " << duration.count() << "ms" << std::endl;
            if (duration.count() > 0) {
                std::cout << "Throughput: " << (messages_sent_.load() * 1000.0 / duration.count()) << " msg/s" << std::endl;
            }

            client.disconnect();
        }
    }

    void runCompressionPerformanceTest() {
        std::cout << "\n=== Compression performance test ===" << std::endl;

        std::cout << "Without compression..." << std::endl;
        testCompression(false);

        std::cout << "With compression..." << std::endl;
        testCompression(true);
    }

    void testCompression(bool use_compression) {
        websocket::WebSocketConfig config;
        config.enableCompression(use_compression);
        config.setCompressionLevel(6);

        websocket::WebSocketClient client(config);

        client.setOnText([this](const std::string &message) {
            (void)message;
            messages_received_++;
        });

        client.setOnError([this](const websocket::WebSocketResult &err) {
            (void)err;
            errors_++;
        });

        if (client.connect("wss://echo.websocket.org")) {
            start_time_ = std::chrono::high_resolution_clock::now();

            std::string large_data(10000, 'A');
            for (int i = 0; i < 50; ++i) {
                if (client.send(large_data)) {
                    messages_sent_++;
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(5));

            end_time_ = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - start_time_);

            std::cout << (use_compression ? "Compression" : "No compression") << " results:" << std::endl;
            std::cout << "Messages sent: " << messages_sent_.load() << std::endl;
            std::cout << "Messages received: " << messages_received_.load() << std::endl;
            std::cout << "Total time: " << duration.count() << "ms" << std::endl;
            std::cout << "Data size: " << (messages_sent_.load() * 10000) << " bytes" << std::endl;
            if (duration.count() > 0) {
                std::cout << "Rate: " << (messages_sent_.load() * 10000.0 / duration.count()) << " bytes/ms" << std::endl;
            }

            client.disconnect();
        }
    }

    void runMemoryTest() {
        std::cout << "\n=== Memory test ===" << std::endl;

        std::vector<std::unique_ptr<websocket::WebSocketClient>> clients;

        std::cout << "Creating many WebSocket clients..." << std::endl;

        for (int i = 0; i < 10; ++i) {
            std::unique_ptr<websocket::WebSocketClient> client(new websocket::WebSocketClient());

            client->setOnText([](const std::string &message) {
                (void)message; // no-op
            });
            client->setOnError([](const websocket::WebSocketResult &err) {
                (void)err; // no-op
            });

            clients.push_back(std::move(client));
        }

        std::cout << "Created " << clients.size() << " clients" << std::endl;

        for (auto &client : clients) client->connect("wss://echo.websocket.org");

        std::this_thread::sleep_for(std::chrono::seconds(2));

        for (auto &client : clients) client->send("Memory test message");

        std::this_thread::sleep_for(std::chrono::seconds(2));

        for (auto &client : clients) client->disconnect();

        std::cout << "Memory test done" << std::endl;
    }

    void runAllPerformanceTests() {
        std::cout << "Start WebSocket client performance tests..." << std::endl;

        runLatencyTest();
        runThroughputTest();
        runCompressionPerformanceTest();
        runMemoryTest();

        std::cout << "\n=== Performance summary ===" << std::endl;
        std::cout << "All performance tests done!" << std::endl;
    }
};

int main() {
    PerformanceTest test;
    test.runAllPerformanceTests();
    return 0;
}