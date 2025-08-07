#!/bin/bash

# WebSocket Client 编译脚本
# 支持 CMake 和 Makefile 两种编译方式

set -e

echo "WebSocket Client 编译脚本"
echo "=========================="

# 检查依赖
check_dependencies() {
    echo "检查依赖..."
    
    # 检查编译器
    if ! command -v g++ &> /dev/null; then
        echo "错误: 未找到 g++ 编译器"
        exit 1
    fi
    
    # 检查 OpenSSL
    if ! pkg-config --exists openssl; then
        echo "错误: 未找到 OpenSSL 开发库"
        echo "请安装 OpenSSL 开发包:"
        echo "  Ubuntu/Debian: sudo apt-get install libssl-dev"
        echo "  CentOS/RHEL: sudo yum install openssl-devel"
        echo "  macOS: brew install openssl"
        exit 1
    fi
    
    # 检查 zlib (可选)
    if pkg-config --exists zlib; then
        echo "✓ 找到 zlib，将启用压缩功能"
        ZLIB_AVAILABLE=true
    else
        echo "⚠ 未找到 zlib，压缩功能将被禁用"
        ZLIB_AVAILABLE=false
    fi
    
    echo "✓ 依赖检查完成"
}

# 使用 CMake 编译
build_with_cmake() {
    echo "使用 CMake 编译..."
    
    if [ ! -d "build" ]; then
        mkdir build
    fi
    
    cd build
    
    # 配置
    if [ "$ZLIB_AVAILABLE" = true ]; then
        cmake .. -DCMAKE_BUILD_TYPE=Release
    else
        cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_ZLIB=OFF
    fi
    
    # 编译
    make -j$(nproc)
    
    cd ..
    
    echo "✓ CMake 编译完成"
}

# 使用 Makefile 编译
build_with_makefile() {
    echo "使用 Makefile 编译..."
    
    # 设置环境变量
    if [ "$ZLIB_AVAILABLE" = true ]; then
        export CXXFLAGS="$CXXFLAGS -DUSE_ZLIB"
        export LIBS="$LIBS -lz"
    fi
    
    make clean
    make -j$(nproc)
    
    echo "✓ Makefile 编译完成"
}

# 运行测试
run_tests() {
    echo "运行测试..."
    
    if [ -f "./websocket_example" ]; then
        echo "运行示例程序..."
        timeout 30s ./websocket_example || echo "示例程序运行完成"
    fi
    
    if [ -f "./websocket_test" ]; then
        echo "运行功能测试..."
        timeout 60s ./websocket_test || echo "功能测试运行完成"
    fi
    
    if [ -f "./websocket_performance" ]; then
        echo "运行性能测试..."
        timeout 120s ./websocket_performance || echo "性能测试运行完成"
    fi
    
    echo "✓ 测试完成"
}

# 显示帮助信息
show_help() {
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -h, --help     显示此帮助信息"
    echo "  -c, --cmake    使用 CMake 编译 (默认)"
    echo "  -m, --make     使用 Makefile 编译"
    echo "  -t, --test     编译后运行测试"
    echo "  -a, --all      编译所有目标并运行测试"
    echo ""
    echo "示例:"
    echo "  $0             使用 CMake 编译"
    echo "  $0 -m          使用 Makefile 编译"
    echo "  $0 -a          编译所有目标并运行测试"
}

# 主函数
main() {
    local use_cmake=true
    local run_tests_flag=false
    
    # 解析命令行参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -c|--cmake)
                use_cmake=true
                shift
                ;;
            -m|--make)
                use_cmake=false
                shift
                ;;
            -t|--test)
                run_tests_flag=true
                shift
                ;;
            -a|--all)
                use_cmake=true
                run_tests_flag=true
                shift
                ;;
            *)
                echo "未知选项: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    # 检查依赖
    check_dependencies
    
    # 编译
    if [ "$use_cmake" = true ]; then
        build_with_cmake
    else
        build_with_makefile
    fi
    
    # 运行测试
    if [ "$run_tests_flag" = true ]; then
        run_tests
    fi
    
    echo ""
    echo "编译完成！"
    echo "生成的可执行文件:"
    ls -la websocket_* 2>/dev/null || echo "未找到可执行文件"
}

# 运行主函数
main "$@"