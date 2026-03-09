#ifndef VERSIONMANAGER_H
#define VERSIONMANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <json/json.h>

struct VersionInfo {
    std::string version;
    std::time_t timestamp;
    std::string manifestHash;
    std::vector<std::string> incrementalFrom;
    std::vector<std::string> files;
    std::vector<std::string> directories;

    Json::Value ToJson() const {
        Json::Value json;
        json["version"]=version;
        json["timestamp"]=static_cast<Json::Int64>(timestamp);
        json["manifest_hash"]=manifestHash;

        Json::Value incJson(Json::arrayValue);
        for(const auto& from:incrementalFrom) {
            incJson.append(from);
        }
        json["incremental_from"]=incJson;

        Json::Value filesJson(Json::arrayValue);
        for(const auto& file:files) {
            filesJson.append(file);
        }
        json["files"]=filesJson;

        Json::Value dirsJson(Json::arrayValue);
        for(const auto& dir:directories) {
            dirsJson.append(dir);
        }
        json["directories"]=dirsJson;

        return json;
    }
};

class VersionManager {
public:
    VersionManager(const std::string& dataDir);

    bool Initialize();
    bool Save();

    // 版本管理
    bool AddVersion(const VersionInfo& version);
    bool RemoveVersion(const std::string& version);
    bool DeleteVersion(const std::string& version);
    const VersionInfo* GetVersion(const std::string& version) const;

    // 获取版本列表
    std::vector<std::string> GetVersionList() const;

    // 获取更新路径
    std::vector<std::string> GetUpdatePath(
        const std::string& fromVersion,
        const std::string& toVersion) const;

    // 查找最近的共同祖先
    std::string FindCommonAncestor(
        const std::string& version1,
        const std::string& version2) const;

private:
    std::string dataDir;
    std::unordered_map<std::string,VersionInfo> versions;

    void LoadVersions();
    bool SaveVersions() const;

    // TODO：构建版本图，目前好像没什么用
    void BuildVersionGraph();

    // 数据文件路径
    std::string GetVersionsFile() const;
};

#endif