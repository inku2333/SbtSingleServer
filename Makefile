# 编译器设置
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2  # 编译选项
INCLUDES = -Iinclude                     # 头文件路径

# 目录设置
SRCDIR = src
INCDIR = include
BUILDDIR = build
BINDIR = bin
TARGET = $(BINDIR)/testClient                 # 可执行文件

# 自动获取源文件（确保src目录下有.cpp文件）
SRCS = $(wildcard $(SRCDIR)/*.cpp)
ifneq ($(SRCS),)
    OBJS = $(patsubst $(SRCDIR)/%.cpp, $(BUILDDIR)/%.o, $(SRCS))
    DEPS = $(OBJS:.o=.d)
else
    $(error 未在$(SRCDIR)目录下找到.cpp源文件)
endif

# 默认目标
all: $(TARGET)

# 创建目录（修正规则冲突问题）
$(BUILDDIR):
	mkdir -p $@

$(BINDIR):
	mkdir -p $@

# 生成可执行文件
$(TARGET): $(OBJS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(OBJS)
	@echo "生成可执行文件: $@"

# 生成目标文件和依赖
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@ -MMD -MP
	@echo "编译: $<"

# 包含依赖文件
-include $(DEPS)

# 清理
clean:
	rm -rf $(BUILDDIR) $(BINDIR)
	@echo "已清理编译产物"

.PHONY: all clean