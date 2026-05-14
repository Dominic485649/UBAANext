/**
 * @file Term.hpp
 * @brief 学期数据模型
 */
#pragma once

#include <string>

namespace UBAANext::Model {

/**
 * @brief 一个学期 / 学年学期
 *
 * 表示 UBAA 学期 API 返回的某一学期。
 */
struct Term {
    std::string code;       ///< 学期代码（例如 "2025-2026-1"）
    std::string name;       ///< 显示名称（例如 "2025-2026学年第一学期"）
    bool selected = false;  ///< 是否为当前选中的学期
    int index = 0;          ///< 在学期列表中的序号索引
};

} // namespace UBAANext::Model
