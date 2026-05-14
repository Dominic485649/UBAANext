/**
 * @file Classroom.hpp
 * @brief 教室可用性数据模型
 *
 * 表示 UBAA 教室 API 返回的空闲教室信息。
 * 用于查找可供自习的空闲教室。
 */
#pragma once

#include <map>
#include <string>
#include <vector>

namespace UBAANext::Model {

/**
 * @brief 单个教室及其空闲时间段的信息
 *
 * `free_sections` 包含当前未被占用的节次编号，
 * 从 API 返回的逗号分隔字段 `kxsds` 解析而来
 * （例如 "1,2,3,4" → {1, 2, 3, 4}）。
 */
struct ClassroomInfo {
    std::string id;               ///< 教室标识符（例如 "J3-101"）
    std::string name;             ///< 人类可读的教室名称
    std::string floor_id;         ///< 楼层标识符（例如 "1"）
    std::vector<int> free_sections;  ///< 空闲节次编号列表
};

/**
 * @brief 教室可用性查询结果，按教学楼分组
 *
 * map 的键为教学楼名称（例如 "J3"），
 * 值为该教学楼内的教室列表及其空闲节次。
 */
struct ClassroomQueryResult {
    std::map<std::string, std::vector<ClassroomInfo>> buildings;
};

} // namespace UBAANext::Model
