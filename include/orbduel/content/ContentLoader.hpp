#pragma once

// 内容加载器：扫描 content/ → 解析 → 必要字段检查；菜单显式重新加载。
//
// 不变量（content 契约）：
//   1. 永不向调用方抛异常；错误进 issues()
//   2. 坏包不进入 ContentPack；同 id 的旧数据继续可用

#include <cstdint>
#include <string>
#include <vector>

#include "orbduel/content/DataModel.hpp"

namespace orbduel {

enum class IssueSeverity { Warning = 0, Error };

struct LoadIssue {
    IssueSeverity severity = IssueSeverity::Error;
    std::string source;  // 例如 "characters/blaze"
    std::string message; // 英文为主，屏幕控制台直接显示
};

class ContentLoader {
  public:
    explicit ContentLoader(std::string contentRoot);
    ~ContentLoader() = default;

    ContentLoader(const ContentLoader &) = delete;
    ContentLoader &operator=(const ContentLoader &) = delete;

    // 全量扫描并加载。返回是否至少加载了一个地图。
    bool loadAll();

    const ContentPack &pack() const;

    std::vector<std::string> characterIds() const;
    std::vector<std::string> skillIds() const;
    std::vector<std::string> mapIds() const;

    const std::vector<LoadIssue> &issues() const;

    // 有 error 级问题时为 true（游戏仍可跑，只是内容不全）。
    bool hasFatal() const;

    void clearIssues();

    const std::string &root() const;

  private:
    std::string root_;
    ContentPack pack_;
    std::vector<LoadIssue> issues_;
};

} // namespace orbduel
