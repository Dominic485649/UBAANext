# 决策 0005: Windows 优先使用 CLI

## 状态

已接受

## 背景

UBAA Next 需要一个 v0.1 的 Windows 入口点。

## 决策

使用 **命令行界面（CLI）** 作为首个 Windows 入口点。

## 理由

1. CLI 是验证核心功能的最简方式
2. v0.3 之前不需要 UI 框架依赖
3. CLI 易于自动化和测试
4. 手写参数解析保持依赖最小化

## 影响

- v0.1：CLI 支持 version、login、course 命令
- v0.4：CLI 支持完整命令集
- v0.7：Slint GUI 作为替代入口点
