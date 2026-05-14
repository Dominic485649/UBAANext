# 存储抽象

## ISecureStore 接口

```cpp
class ISecureStore {
public:
    virtual ~ISecureStore() = default;
    virtual void set_string(const std::string& key, const std::string& value) = 0;
    virtual std::optional<std::string> get_string(const std::string& key) const = 0;
    virtual void remove(const std::string& key) = 0;
    virtual void clear() = 0;
};
```

## ICacheStore 接口

```cpp
class ICacheStore {
public:
    virtual ~ICacheStore() = default;
    virtual void set(const std::string& key, const std::string& value) = 0;
    virtual std::optional<std::string> get(const std::string& key) const = 0;
    virtual void remove(const std::string& key) = 0;
    virtual void clear() = 0;
};
```

## v0.1 实现

`MockSecureStore` 使用 `std::unordered_map` 进行存储。
