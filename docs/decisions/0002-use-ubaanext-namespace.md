# 决策 0002: 使用 UBAANext 命名空间

## 状态

已接受

## 背景

项目需要一个一致的 C++ 命名空间，要求：
- 反映项目名称
- 唯一且不易冲突
- 日常使用中长度合理

## 决策

- **公共 API**：namespace UBAANext
- **内部别名**：namespace um = UBAANext;（仅允许在 .cpp、test、CLI 文件中使用）

## 理由

1. UBAANext 与项目名称匹配且无歧义
2. um 是开发者的便捷简写，但对公共头文件来说太短
3. 防止下游项目中的命名空间污染

## 影响

- 所有公共头文件使用 namespace UBAANext {}
- namespace um = UBAANext; 在 core/include/UBAANext/**/*.hpp 中禁止使用
- CI 应验证公共头文件中无 namespace um
