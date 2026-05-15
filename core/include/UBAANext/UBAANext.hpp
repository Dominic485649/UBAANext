/**
 * @file UBAANext.hpp
 * @brief UBAANextCore 库的伞形头文件
 *
 * 包含 UBAANextCore 库的所有公共头文件。
 * 为需要广泛访问的消费者提供便捷的单一头文件。
 */
#pragma once

// ── 基础类型 ───────────────────────────────────────────────────────
#include <UBAANext/Base/Error.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Base/Types.hpp>
#include <UBAANext/Version.hpp>

// ── 数据模型 ──────────────────────────────────────────────────────
#include <UBAANext/Model/Account.hpp>
#include <UBAANext/Model/Classroom.hpp>
#include <UBAANext/Model/Course.hpp>
#include <UBAANext/Model/Exam.hpp>
#include <UBAANext/Model/Grade.hpp>
#include <UBAANext/Model/Term.hpp>
#include <UBAANext/Model/Week.hpp>

// ── 网络层 ───────────────────────────────────────────────────────
#include <UBAANext/Net/CookieJar.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Net/HttpRequest.hpp>
#include <UBAANext/Net/HttpResponse.hpp>

// ── 存储层 ──────────────────────────────────────────────────────────
#include <UBAANext/Storage/CacheStore.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>
#include <UBAANext/Storage/SecureStore.hpp>
#include <UBAANext/Storage/SecureStoreAdapter.hpp>

// ── 认证 ───────────────────────────────────────────────────────────
#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Auth/Session.hpp>
#include <UBAANext/Auth/SessionManager.hpp>

// ── 解析 ──────────────────────────────────────────────────────────
#include <UBAANext/Parser/JsonParser.hpp>

// ── 服务层 ─────────────────────────────────────────────────────────
#include <UBAANext/Service/ClassroomService.hpp>
#include <UBAANext/Service/CourseService.hpp>
#include <UBAANext/Service/ExamService.hpp>
#include <UBAANext/Service/GradeService.hpp>
