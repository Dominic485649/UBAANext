#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <string>
#include <vector>

namespace UBAANext {

struct TodoQuery {
    bool pending_only = true;
};

class TodoService {
public:
    TodoService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    Result<std::vector<Model::FeatureRecord>> list_todos(const TodoQuery &query = {});

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
};

} // namespace UBAANext
