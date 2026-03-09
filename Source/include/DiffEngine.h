#ifndef DIFFENGINE_H
#define DIFFENGINE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <json/json.h>
#include "FileScanner.h"

enum class ChangeType {
    ADDED,          // A - 新增文件
    MODIFIED,       // M - 修改文件
    DELETED,        // D - 删除文件
    MOVED,          // R - 移动/重命名
    DIRECTORY_ADDED,    // AD - 新增目录（空目录）
    DIRECTORY_DELETED   // DD - 删除目录（空目录）
};

struct ChangeRecord {
    ChangeType type = ChangeType::ADDED;
    std::string path;
    std::string oldPath;  // 用于移动操作
    std::string hash;
	//FIXME: 这里的 size 可能会有问题，他没意义
    uint64_t size = 0;
	//FIXME: 潜在风险,其他类型误设 oldPath 也可能被输出。
    Json::Value ToJson() const {
        Json::Value json;

        switch(type) {
        case ChangeType::ADDED: json["type"]="A"; break;
        case ChangeType::MODIFIED: json["type"]="M"; break;
        case ChangeType::DELETED: json["type"]="D"; break;
        case ChangeType::MOVED: json["type"]="R"; break;
        case ChangeType::DIRECTORY_ADDED: json["type"]="AD"; break;
        case ChangeType::DIRECTORY_DELETED: json["type"]="DD"; break;
        }

        json["path"]=path;
        if(!oldPath.empty()) {
            json["old_path"]=oldPath;
        }
        json["hash"]=hash;
        json["size"]=static_cast<Json::Int64>(size);

        return json;
    }
};
class DiffEngine {
public:
    DiffEngine()=default;

    // 计算差异
    std::vector<ChangeRecord> CalculateDiff(
        const std::vector<FileInfo>& oldFiles,
        const std::vector<FileInfo>& newFiles,
        const std::vector<DirectoryInfo>& oldDirs,
        const std::vector<DirectoryInfo>& newDirs);

    // 生成更新清单
    static std::string GenerateManifest(const std::vector<ChangeRecord>& changes);

    // 从清单解析
    static std::vector<ChangeRecord> ParseManifest(const std::string& manifest);

private:
    // 检测文件移动
    void DetectFileMovements(
        const std::vector<FileInfo>& oldFiles,
        const std::vector<FileInfo>& newFiles,
        std::vector<ChangeRecord>& changes);

    // 构建哈希映射
    std::unordered_map<std::string,std::vector<const FileInfo*>>
        BuildHashMap(const std::vector<FileInfo>& files);
};

#endif