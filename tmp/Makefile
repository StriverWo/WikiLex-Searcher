# Makefile for WikiLex_Searcher

# 编译器及选项（根据需要可以调整 -O2、-std=c++17 等选项）
CXX = g++
CXXFLAGS = -std=c++17 -O0 -g -Iinclude

# 链接选项：需要链接 onnxruntime、jsoncpp、pthread、dl 等库（根据实际情况调整）
LDFLAGS = -ljsoncpp -lpthread

# 源码目录和目标可执行文件列表（假设 src 下每个 .cpp 文件都要编译为独立程序）
SRC_DIR = src
# 可以使用通配符获取所有 .cpp 文件，然后去掉扩展名得到可执行文件名
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
TARGETS := $(patsubst $(SRC_DIR)/%.cpp, %, $(SOURCES))

.PHONY: all clean

all: $(TARGETS)

# 规则：每个目标由相应的 .cpp 编译生成
%: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)
