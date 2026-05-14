/**
 * @file Course.hpp
 * @brief 课程 / 排课数据模型
 *
 * 表示学生课表中的单个课程条目。
 * 映射到 UBAA 课表 API 的 CourseClass DTO。
 */
#pragma once

#include <string>

namespace UBAANext::Model {

/**
 * @brief 包含时间和地点信息的已排课程
 *
 * 一个 Course 代表一个重复出现的课堂会话
 * （例如 "高等数学 每周一第1-2节 在J3-101，第1-16周"）。
 */
struct Course {
    std::string id;             ///< 唯一课程实例 ID
    std::string name;           ///< 人类可读的课程名称（例如 "高等数学"）
    std::string teacher;        ///< 授课教师姓名
    std::string classroom;      ///< 上课地点（例如 "J3-101"）
    int week_start = 0;         ///< 本课程开始的学期周次
    int week_end = 0;           ///< 本课程结束的学期周次
    int day_of_week = 0;        ///< 星期几（1=周一 ... 7=周日）
    int section_start = 0;      ///< 起始节次（例如 1）
    int section_end = 0;        ///< 结束节次（例如 2）
    std::string course_code;    ///< 官方课程代码（例如 "MATH101"）
    std::string credit;         ///< 学分字符串（例如 "3.0"）
    std::string begin_time;     ///< 墙钟开始时间（例如 "08:00"）
    std::string end_time;       ///< 墙钟结束时间（例如 "09:40"）
};

} // namespace UBAANext::Model
