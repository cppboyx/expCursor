CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
INCLUDES = -I.
LIBS = -lssl -lcrypto

# 检查是否安装了zlib
ifeq ($(shell pkg-config --exists zlib && echo yes),yes)
    CXXFLAGS += -DUSE_ZLIB
    LIBS += -lz
    $(info Zlib found, compression enabled)
else
    $(info Zlib not found, compression disabled)
endif

# Windows支持
ifeq ($(OS),Windows_NT)
    LIBS += -lws2_32
    CXXFLAGS += -D_WIN32
endif

EXAMPLE_TARGET = websocket_example
TEST_TARGET = websocket_test
PERFORMANCE_TARGET = websocket_performance
EXAMPLE_SOURCES = example.cpp
TEST_SOURCES = test.cpp
PERFORMANCE_SOURCES = performance_test.cpp
EXAMPLE_OBJECTS = $(EXAMPLE_SOURCES:.cpp=.o)
TEST_OBJECTS = $(TEST_SOURCES:.cpp=.o)
PERFORMANCE_OBJECTS = $(PERFORMANCE_SOURCES:.cpp=.o)

.PHONY: all clean

all: $(EXAMPLE_TARGET) $(TEST_TARGET) $(PERFORMANCE_TARGET)

$(EXAMPLE_TARGET): $(EXAMPLE_OBJECTS)
	$(CXX) $(EXAMPLE_OBJECTS) -o $(EXAMPLE_TARGET) $(LIBS)

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CXX) $(TEST_OBJECTS) -o $(TEST_TARGET) $(LIBS)

$(PERFORMANCE_TARGET): $(PERFORMANCE_OBJECTS)
	$(CXX) $(PERFORMANCE_OBJECTS) -o $(PERFORMANCE_TARGET) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(EXAMPLE_OBJECTS) $(TEST_OBJECTS) $(PERFORMANCE_OBJECTS) $(EXAMPLE_TARGET) $(TEST_TARGET) $(PERFORMANCE_TARGET)

# 安装依赖（Ubuntu/Debian）
install-deps:
	sudo apt-get update
	sudo apt-get install -y libssl-dev zlib1g-dev

# 安装依赖（CentOS/RHEL）
install-deps-centos:
	sudo yum install -y openssl-devel zlib-devel

# 安装依赖（macOS）
install-deps-macos:
	brew install openssl zlib