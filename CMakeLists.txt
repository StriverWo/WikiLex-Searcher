cmake_minimum_required(VERSION 3.10)
project(WikiLex_Searcher VERSION 1.0 LANGUAGES CXX)

# 设置 C++ 标准为 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 默认构建类型为 Debug（调试信息，并关闭优化）
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# 如果需要添加调试选项，可以在这里设置（例如：-O0 -g）
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")

# 添加头文件搜索路径
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

file(COPY ${CMAKE_SOURCE_DIR}/src/runlemdebug.py DESTINATION ${CMAKE_BINARY_DIR})

# 查找 MySQL Connector/C++ 库的路径
find_library(MYSQLCPPCONN_LIB NAMES mysqlcppconn PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib)
if(NOT MYSQLCPPCONN_LIB)
    message(FATAL_ERROR "MySQL Connector/C++ library not found.")
else()
    message(STATUS "Found MySQL Connector/C++ library: ${MYSQLCPPCONN_LIB}")
    include_directories(/usr/include/mysql-cppconn)  # 包含 MySQL Connector 的头文件路径
endif()

# 查找 jsoncpp 库
find_library(JSONCPP_LIB jsoncpp)
if(NOT JSONCPP_LIB)
    message(FATAL_ERROR "jsoncpp library not found.")
else()
    message(STATUS "Found jsoncpp library: ${JSONCPP_LIB}")
endif()

# 查找 pthread 库
find_package(Threads REQUIRED)
if(NOT Threads_FOUND)
    message(FATAL_ERROR "pthread library not found.")
else()
    message(STATUS "Found pthread library")
endif()

# 查找 hiredis 库
find_library(HIREDIS_LIB hiredis)
if(NOT HIREDIS_LIB)
    message(FATAL_ERROR "hiredis library not found.")
else()
    message(STATUS "Found hiredis library: ${HIREDIS_LIB}")
endif()

# LINUX系统下构建符号链接
if(UNIX)
    add_custom_target(create_symlinks ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/data ${CMAKE_BINARY_DIR}/data
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/model ${CMAKE_BINARY_DIR}/model
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/lemwwwroot ${CMAKE_BINARY_DIR}/lemwwwroot
        COMMENT "Creating symbolic links for 'src' and 'data' and 'model' directories in the build directory"
    )
endif()

# 获取 src 目录下所有 .cpp 文件
file(GLOB SRC_FILES ${CMAKE_SOURCE_DIR}/src/*.cpp)

# 对于每个 .cpp 文件生成一个可执行文件，目标名称与文件名相同（不含扩展名）
foreach(src_file ${SRC_FILES})
    get_filename_component(exe_name ${src_file} NAME_WE)
    add_executable(${exe_name} ${src_file})
    # 链接找到的库
    target_link_libraries(${exe_name} 
        ${JSONCPP_LIB} 
        ${CMAKE_THREAD_LIBS_INIT} 
        ${HIREDIS_LIB} 
        ${MYSQLCPPCONN_LIB} 
        stdc++   # 确保链接 C++ 标准库
    )
endforeach()
