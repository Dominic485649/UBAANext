/**
 * @file Exam.hpp
 * @brief 考试数据模型
 *
 * 表示一条已排定的考试条目。
 * 映射到 UBAA 课表 API 的考试 DTO。
 */
#pragma once

#include <string>

namespace UBAANext::Model {

/**
 * @brief 考试状态枚举
 *
 * 表示考试的安排和进行状态。
 */
enum class ExamStatus {
    Pending   = 0,  ///< 待安排：尚未分配考场和座位
    Arranged  = 1,  ///< 已安排：考场、座位和时间已确定
    Finished  = 2   ///< 已结束：考试已完成
};

/**
 * @brief 包含时间、地点和座位信息的已排考试
 */
struct Exam {
    std::string id;             ///< 唯一考试 ID
    std::string course_name;    ///< 考试科目名称
    std::string location;       ///< 考场（例如 "J3-101"）
    std::string time_text;      ///< 人类可读的时间范围（例如 "2026-06-20 09:00-11:00"）
    std::string course_no;      ///< 官方课程代码（例如 "MATH101"）
    std::string exam_date;      ///< 考试日期，格式 yyyy-MM-dd
    std::string start_time;     ///< 开始时间，格式 HH:mm
    std::string end_time;       ///< 结束时间，格式 HH:mm
    std::string seat_no;        ///< 分配的座位号
    std::string exam_type;      ///< 考试类型（例如 "期末考试"、"期中考试"）
    ExamStatus status = ExamStatus::Pending;  ///< 考试状态
};

} // namespace UBAANext::Model
