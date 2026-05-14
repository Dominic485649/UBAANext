/**
 * @file Week.hpp
 * @brief 教学周数据模型
 */
#pragma once

#include <string>

namespace UBAANext::Model {

/**
 * @brief 学期内的一个教学周
 *
 * 表示一个包含日期范围的教学周，
 * 用于基于周次的课程表查询。
 */
struct Week {
    int serial_number = 0;      ///< 周序号（从 1 开始）
    std::string name;           ///< 显示名称（例如 "第1周"）
    std::string start_date;     ///< 周起始日期，格式 yyyy-MM-dd
    std::string end_date;       ///< 周结束日期，格式 yyyy-MM-dd
    bool is_current = false;    ///< 是否为当前周
};

} // namespace UBAANext::Model
