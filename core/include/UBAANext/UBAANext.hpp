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
#include <UBAANext/Model/Bykc.hpp>
#include <UBAANext/Model/Classroom.hpp>
#include <UBAANext/Model/Course.hpp>
#include <UBAANext/Model/Evaluation.hpp>
#include <UBAANext/Model/Exam.hpp>
#include <UBAANext/Model/Grade.hpp>
#include <UBAANext/Model/Judge.hpp>
#include <UBAANext/Model/LibrarySeat.hpp>
#include <UBAANext/Model/Signin.hpp>
#include <UBAANext/Model/Spoc.hpp>
#include <UBAANext/Model/Term.hpp>
#include <UBAANext/Model/VenueReservation.hpp>
#include <UBAANext/Model/Week.hpp>
#include <UBAANext/Model/Ygdk.hpp>

// ── 网络层 ───────────────────────────────────────────────────────
#include <UBAANext/Net/CookieJar.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Net/HttpRequest.hpp>
#include <UBAANext/Net/HttpResponse.hpp>
#include <UBAANext/Net/VpnCipher.hpp>
#ifdef _WIN32
#include <UBAANext/Net/WinHttpClient.hpp>
#endif

// ── 存储层 ──────────────────────────────────────────────────────────
#include <UBAANext/Storage/CacheStore.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>
#include <UBAANext/Storage/SecureStore.hpp>
#include <UBAANext/Storage/SecureStoreAdapter.hpp>

// ── 加密 ───────────────────────────────────────────────────────────
#include <UBAANext/Crypto/CryptoProvider.hpp>

// ── 认证 ───────────────────────────────────────────────────────────
#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Auth/Session.hpp>
#include <UBAANext/Auth/SessionManager.hpp>

// ── 协议 ───────────────────────────────────────────────────────────
#include <UBAANext/Protocol/AppBuaaSession.hpp>
#include <UBAANext/Protocol/ByxtSession.hpp>
#include <UBAANext/Protocol/ScoreSession.hpp>

// ── 解析 ──────────────────────────────────────────────────────────
#include <UBAANext/Parser/BykcParser.hpp>
#include <UBAANext/Parser/EvaluationParser.hpp>
#include <UBAANext/Parser/JsonParser.hpp>
#include <UBAANext/Parser/JudgeParser.hpp>
#include <UBAANext/Parser/LibrarySeatParser.hpp>
#include <UBAANext/Parser/SigninParser.hpp>
#include <UBAANext/Parser/SpocParser.hpp>
#include <UBAANext/Parser/VenueReservationParser.hpp>
#include <UBAANext/Parser/YgdkParser.hpp>

// ── 服务层 ─────────────────────────────────────────────────────────
#include <UBAANext/Service/BykcService.hpp>
#include <UBAANext/Service/ClassroomService.hpp>
#include <UBAANext/Service/CourseService.hpp>
#include <UBAANext/Service/EvaluationService.hpp>
#include <UBAANext/Service/ExamService.hpp>
#include <UBAANext/Service/FeatureService.hpp>
#include <UBAANext/Service/GradeService.hpp>
#include <UBAANext/Service/JudgeService.hpp>
#include <UBAANext/Service/LibrarySeatService.hpp>
#include <UBAANext/Service/ResponseUtils.hpp>
#include <UBAANext/Service/SigninService.hpp>
#include <UBAANext/Service/SpocService.hpp>
#include <UBAANext/Service/TermService.hpp>
#include <UBAANext/Service/VenueReservationService.hpp>
#include <UBAANext/Service/YgdkService.hpp>
