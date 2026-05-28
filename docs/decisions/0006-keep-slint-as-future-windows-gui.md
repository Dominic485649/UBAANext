# 决策 0006: 保留 Slint 作为未来 Windows GUI

## 状态

已接受

## 背景

UBAA Next 需要一个原生 Windows GUI 选项。

## 决策

使用 **Slint** 作为未来 Windows GUI 框架（v0.8）。

## 理由

1. Slint 原生支持 C++
2. 现代声明式 UI 语法
3. 跨平台潜力（Linux、嵌入式）
4. 不需要 Qt 许可证

## 影响

- v0.8：Slint UI + C++ ViewModel
- 需要处理 Slint 许可证（GPLv3 或商业）
