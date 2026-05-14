# 网络抽象

## IHttpClient 接口

```cpp
class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse send(const HttpRequest& request) = 0;
};
```

## HttpRequest

```cpp
struct HttpRequest {
    HttpMethod method;
    std::string url;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};
```

## HttpResponse

```cpp
struct HttpResponse {
    int status_code;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};
```

## v0.1 实现

`MockHttpClient` 始终返回 HTTP 200 及空 JSON 响应体。
