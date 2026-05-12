# 旧示例（Boost.Asio 基础示例）
add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server PRIVATE CustomLibraries::CustomLibraries)

# Web 模块示例
add_executable(web_server web_server.cpp)
target_link_libraries(web_server PRIVATE CustomLibraries::CustomLibraries)