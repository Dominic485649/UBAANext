/**
 * @file PlainFileStore.hpp
 * @brief 基于文件的键值存储实现
 *
 * 将键值对持久化到本地文件系统，使 CLI 命令之间能够共享 mock/offline 会话数据。
 *
 * @warning 不提供任何加密保护，仅适用于测试或离线开发。
 *
 * 存储位置：%LOCALAPPDATA%\UBAANext\session.dat
 * 格式：每行一个键值对，键和值之间用制表符（\t）分隔。
 */
#pragma once

#include <UBAANext/Storage/SecureStore.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>

namespace UBAANextCli {

/**
 * @brief 基于文件系统的 ISecureStore 实现，可选平台加密
 *
 * 在构造时从文件读取所有键值对到内存缓存，
 * 在析构时将内存缓存写回文件。
 * 适用于 CLI mock 模式下跨进程的会话持久化。
 *
 * @warning encrypted=false 时数据以明文存储，不提供安全保护。
 */
class PlainFileStore : public UBAANext::ISecureStore {
public:
    /**
     * @brief 构造文件存储
     * @param file_path 存储文件的路径
     */
    explicit PlainFileStore(std::filesystem::path file_path);

    /**
     * @brief 析构函数，自动将数据持久化到文件
     */
    ~PlainFileStore() override;

    // 禁止拷贝和移动（持有文件句柄语义）
    PlainFileStore(const PlainFileStore &) = delete;
    PlainFileStore &operator=(const PlainFileStore &) = delete;
    PlainFileStore(PlainFileStore &&) = delete;
    PlainFileStore &operator=(PlainFileStore &&) = delete;

    void set_string(const std::string &key, const std::string &value) override;

    [[nodiscard]] std::optional<std::string> get_string(const std::string &key) const override;

    void remove(const std::string &key) override;

    void clear() override;

private:
    /**
     * @brief 从文件加载所有键值对到内存缓存
     */
    void load_from_file();

    /**
     * @brief 将内存缓存中的所有键值对写回文件
     */
    void save_to_file() const;

    std::filesystem::path m_file_path;                       ///< 存储文件路径
    std::unordered_map<std::string, std::string> m_data;     ///< 内存缓存
};

} // namespace UBAANextCli