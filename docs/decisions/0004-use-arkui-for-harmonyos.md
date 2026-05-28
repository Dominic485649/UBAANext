# 决策 0004: HarmonyOS 使用 ArkUI

## 状态

已接受

## 背景

UBAA Next 需要一个 HarmonyOS 的 UI 框架。

## 决策

使用 **ArkUI**（HarmonyOS 声明式 UI 框架），通过 NAPI 绑定到 C++ 核心。

## 理由

1. ArkUI 是 HarmonyOS 推荐的 UI 框架
2. NAPI 允许从 ArkTS 调用 C++ 代码
3. 与 HarmonyOS 生态系统一致

## 影响

- v0.6：C API 边界 + NAPI 绑定
- v0.7：ArkUI 页面
