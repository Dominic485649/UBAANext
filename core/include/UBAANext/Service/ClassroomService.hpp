/**
 * @file ClassroomService.hpp
 * @brief 教室可用性查询服务
 *
 * 提供在指定校区和日期查找空闲教室的方法。
 * 通过 Mock HTTP + JSON 解析获取数据，支持缓存。
 */
#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Classroom.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <string>
#include <vector>

namespace UBAANext {

class ClassroomService {
public:
#if UBAANEXT_ENABLE_MOCKS
    ClassroomService(IHttpClient &http_client, ICacheStore &cache);
#endif
    ClassroomService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    Result<Model::ClassroomQueryResult> query_classrooms(int campus_id,
                                                         const std::string &date);
    Result<Model::ClassroomQueryResult> query_classrooms(int campus_id,
                                                         const std::string &date,
                                                         const std::vector<int> &sections);
    Result<Model::ClassroomQueryResult> query_classrooms(int campus_id,
                                                         const std::string &date,
                                                         const std::string &username,
                                                         const std::string &password);
    Result<Model::ClassroomQueryResult> query_classrooms(int campus_id,
                                                         const std::string &date,
                                                         const std::string &username,
                                                         const std::string &password,
                                                         const std::vector<int> &sections);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
};

} // namespace UBAANext
