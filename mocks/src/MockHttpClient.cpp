/**
 * @file MockHttpClient.cpp
 * @brief 模拟 HTTP 客户端实现（URL 路由式）
 *
 * 预设了与 tests/fixtures 下 JSON 文件对应的 mock 响应数据。
 */

#include <UBAANextMocks/MockHttpClient.hpp>

namespace UBAANextMocks {

// clang-format off

static const char *kCoursesJson = R"([
  {"id":"COURSE001","name":"高等数学","teacher":"张教授","classroom":"J3-101","weekStart":1,"weekEnd":16,"dayOfWeek":1,"sectionStart":1,"sectionEnd":2,"courseCode":"MATH101","credit":"3.0","beginTime":"08:00","endTime":"09:40"},
  {"id":"COURSE002","name":"程序设计基础","teacher":"李教授","classroom":"J3-202","weekStart":1,"weekEnd":16,"dayOfWeek":1,"sectionStart":3,"sectionEnd":4,"courseCode":"CS101","credit":"2.0","beginTime":"10:00","endTime":"11:40"},
  {"id":"COURSE003","name":"大学物理","teacher":"王教授","classroom":"J3-303","weekStart":1,"weekEnd":16,"dayOfWeek":1,"sectionStart":5,"sectionEnd":6,"courseCode":"PHYS101","credit":"3.0","beginTime":"14:00","endTime":"15:40"}
])";

static const char *kExamsJson = R"([
  {"id":"EXAM001","courseName":"高等数学","location":"J3-101","timeText":"2026-06-20 09:00-11:00","courseNo":"MATH101","examDate":"2026-06-20","startTime":"09:00","endTime":"11:00","seatNo":"15","examType":"期末考试","status":1},
  {"id":"EXAM002","courseName":"程序设计基础","location":"J3-202","timeText":"2026-06-22 14:00-16:00","courseNo":"CS101","examDate":"2026-06-22","startTime":"14:00","endTime":"16:00","seatNo":"23","examType":"期末考试","status":1},
  {"id":"EXAM003","courseName":"大学物理","location":"J3-303","timeText":"2026-06-24 09:00-11:00","courseNo":"PHYS101","examDate":"2026-06-24","startTime":"09:00","endTime":"11:00","seatNo":"8","examType":"期末考试","status":1}
])";

static const char *kClassroomsJson = R"({
  "buildings": {
    "J3": [
      {"id":"J3-101","name":"J3-101","floorId":"1","freeSections":[1,2,3,4]},
      {"id":"J3-202","name":"J3-202","floorId":"2","freeSections":[5,6,7,8]},
      {"id":"J3-303","name":"J3-303","floorId":"3","freeSections":[9,10,11,12]}
    ]
  }
})";

static const char *kTermsJson = R"([
  {"code":"2025-2026-1","name":"2025-2026学年第一学期","selected":false,"index":0},
  {"code":"2025-2026-2","name":"2025-2026学年第二学期","selected":true,"index":1}
])";

static const char *kWeeksJson = R"([
  {"serialNumber":1,"name":"第1周","startDate":"2026-02-23","endDate":"2026-03-01","isCurrent":false},
  {"serialNumber":2,"name":"第2周","startDate":"2026-03-02","endDate":"2026-03-08","isCurrent":false},
  {"serialNumber":3,"name":"第3周","startDate":"2026-03-09","endDate":"2026-03-15","isCurrent":false},
  {"serialNumber":4,"name":"第4周","startDate":"2026-03-16","endDate":"2026-03-22","isCurrent":false},
  {"serialNumber":5,"name":"第5周","startDate":"2026-03-23","endDate":"2026-03-29","isCurrent":false},
  {"serialNumber":6,"name":"第6周","startDate":"2026-03-30","endDate":"2026-04-05","isCurrent":false},
  {"serialNumber":7,"name":"第7周","startDate":"2026-04-06","endDate":"2026-04-12","isCurrent":false},
  {"serialNumber":8,"name":"第8周","startDate":"2026-04-13","endDate":"2026-04-19","isCurrent":true},
  {"serialNumber":9,"name":"第9周","startDate":"2026-04-20","endDate":"2026-04-26","isCurrent":false},
  {"serialNumber":10,"name":"第10周","startDate":"2026-04-27","endDate":"2026-05-03","isCurrent":false},
  {"serialNumber":11,"name":"第11周","startDate":"2026-05-04","endDate":"2026-05-10","isCurrent":false},
  {"serialNumber":12,"name":"第12周","startDate":"2026-05-11","endDate":"2026-05-17","isCurrent":false},
  {"serialNumber":13,"name":"第13周","startDate":"2026-05-18","endDate":"2026-05-24","isCurrent":false},
  {"serialNumber":14,"name":"第14周","startDate":"2026-05-25","endDate":"2026-05-31","isCurrent":false},
  {"serialNumber":15,"name":"第15周","startDate":"2026-06-01","endDate":"2026-06-07","isCurrent":false},
  {"serialNumber":16,"name":"第16周","startDate":"2026-06-08","endDate":"2026-06-14","isCurrent":false},
  {"serialNumber":17,"name":"第17周","startDate":"2026-06-15","endDate":"2026-06-21","isCurrent":false},
  {"serialNumber":18,"name":"第18周","startDate":"2026-06-22","endDate":"2026-06-28","isCurrent":false},
  {"serialNumber":19,"name":"第19周","startDate":"2026-06-29","endDate":"2026-07-05","isCurrent":false},
  {"serialNumber":20,"name":"第20周","startDate":"2026-07-06","endDate":"2026-07-12","isCurrent":false}
])";

static const char *kCloudUserRootJson = R"({"success":true,"data":[{"docid":"cloud-root-user","doc_lib_name":"个人文档库","doc_lib_type":"user_doc_lib","totalsize":-1}]})";

static const char *kCloudListJson = R"({"success":true,"data":{"dirs":[{"docid":"cloud-dir-1","parent_docid":"cloud-root-user","name":"示例文件夹","type":"dir","size":"-1"}],"files":[{"docid":"cloud-file-1","parent_docid":"cloud-root-user","name":"示例文件.txt","type":"file","size":"1024"}]}})";

// clang-format on

MockHttpClient::MockHttpClient() {
    m_responses["/schedule/today"]  = kCoursesJson;
    m_responses["/schedule/week"]   = kCoursesJson;
    m_responses["/exam/list"]       = kExamsJson;
    m_responses["/classroom/query"] = kClassroomsJson;
    m_responses["/schedule/terms"]  = kTermsJson;
    m_responses["/schedule/weeks"]  = kWeeksJson;
    m_responses["https://bhpan.buaa.edu.cn/api/efast/v1/owned-doc-lib"] = kCloudUserRootJson;
    m_responses["https://bhpan.buaa.edu.cn/api/efast/v1/dir/list"] = kCloudListJson;
}

UBAANext::Result<UBAANext::HttpResponse> MockHttpClient::send(const UBAANext::HttpRequest &request) {
    ++m_request_counts[request.url];
    // 检查是否有为该 URL 配置的错误
    auto err_it = m_errors.find(request.url);
    if (err_it != m_errors.end()) {
        const auto &err = err_it->second;
        if (err.network_error.has_value()) {
            return UBAANext::make_error(UBAANext::ErrorCode::NetworkError, *err.network_error);
        }
        if (err.http_status.has_value()) {
            UBAANext::HttpResponse response;
            response.status_code = *err.http_status;
            response.body = err.http_body;
            return response;
        }
    }

    UBAANext::HttpResponse response;
    response.status_code = 200;

    auto it = m_responses.find(request.url);
    if (it != m_responses.end()) {
        response.body = it->second;
    } else {
        response.body = "{}";
    }

    return response;
}

void MockHttpClient::set_mock_response(const std::string &url_pattern, std::string json_body) {
    m_responses[url_pattern] = std::move(json_body);
    m_errors.erase(url_pattern);  // 清除可能存在的错误配置
}

void MockHttpClient::set_network_error(const std::string &url_pattern, std::string error_msg) {
    m_errors[url_pattern] = ErrorConfig{std::move(error_msg), std::nullopt, {}};
    m_responses.erase(url_pattern);
}

void MockHttpClient::set_http_error(const std::string &url_pattern, int status_code, std::string body) {
    m_errors[url_pattern] = ErrorConfig{std::nullopt, status_code, std::move(body)};
    m_responses.erase(url_pattern);
}

int MockHttpClient::request_count(const std::string &url_pattern) const {
    auto it = m_request_counts.find(url_pattern);
    return it == m_request_counts.end() ? 0 : it->second;
}

} // namespace UBAANextMocks
