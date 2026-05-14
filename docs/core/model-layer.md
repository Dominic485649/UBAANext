# 模型层

## 模型

### Account

```cpp
struct Account {
    std::string student_id;
    std::string display_name;
};
```

### Course

```cpp
struct Course {
    std::string id;
    std::string name;
    std::string teacher;
    std::string classroom;
    int week_start;
    int week_end;
    int day_of_week;
    int section_start;
    int section_end;
};
```

### Exam

```cpp
struct Exam {
    std::string id;
    std::string course_name;
    std::string location;
    std::string time_text;
};
```

## 规则

- 纯数据结构，不含行为
- 无 UI 依赖
- 无 JSON 库依赖
- 无平台 API 依赖
