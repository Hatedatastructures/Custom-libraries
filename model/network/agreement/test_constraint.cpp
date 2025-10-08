#include "protocol.hpp"
#include <iostream>

int main()
{
    // 测试 request_header 是否满足 header_constraint 约束
    protocol::request_header req_header;
    
    // 设置一些基本信息
    req_header.set_method("GET");
    req_header.set_target("/api/test");
    req_header.set_user_agent("TestAgent/1.0");
    req_header.set_header("Content-Type", "application/json");
    
    // 测试 to_string 方法
    std::string header_str = req_header.to_string();
    std::cout << "Request Header String:\n" << header_str << std::endl;
    
    // 测试 to_json 方法
    protocol::json json_obj = req_header.to_json();
    std::cout << "Request Header JSON:\n" << json_obj.to_string() << std::endl;
    
    // 测试 from_string 方法
    protocol::request_header new_header;
    bool parse_success = new_header.from_string(header_str);
    std::cout << "Parse success: " << (parse_success ? "true" : "false") << std::endl;
    
    // 测试 from_json 方法
    protocol::request_header json_header;
    bool json_success = json_header.from_json(json_obj);
    std::cout << "JSON parse success: " << (json_success ? "true" : "false") << std::endl;
    
    // 测试完整的 request 类（使用模板约束）
    protocol::request<protocol::request_header> request;
    request.header().set_method("POST");
    request.header().set_target("/api/data");
    request.set_message("Test message body");
    
    std::cout << "\nFull Request:\n" << request.to_string() << std::endl;
    
    // 测试约束是否正常工作
    std::cout << "\n模板约束测试通过！request_header 满足 header_constraint 约束。" << std::endl;
    
    return 0;
}