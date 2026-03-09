#ifndef FILESCANNER_H
#define FILESCANNER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <json/json.h>

struct FileInfo {
    std::string path;
    std::string hash;
    uint64_t size = 0;
    bool isDirectory = false;
    std::time_t modifiedTime = 0;

    // 用于比较
	//FIXEME: 简化的实现，以后可能需要改
    bool operator==(const FileInfo& other) const {
        return path==other.path&&hash==other.hash;
    }

    Json::Value ToJson() const {
        Json::Value json;
        json["path"]=path;
        json["hash"]=hash;
        json["size"]=static_cast<Json::Int64>(size);
        json["is_directory"]=isDirectory;
        json["modified_time"]=static_cast<Json::Int64>(modifiedTime);
        return json;
    }
};
//OPTIMIZE:FileScannercpp好像也维护了这个东西（files），会浪费内存
struct DirectoryInfo {
    std::string path;
    std::vector<FileInfo> files;
    std::vector<std::string> subdirectories;

    Json::Value ToJson() const {
        Json::Value json;
        json["path"]=path;

        Json::Value filesJson(Json::arrayValue);
        for(const auto& file:files) {
            filesJson.append(file.ToJson());
        }
        json["files"]=filesJson;

        Json::Value dirsJson(Json::arrayValue);
        for(const auto& dir:subdirectories) {
            dirsJson.append(dir);
        }
        json["subdirectories"]=dirsJson;

        return json;
    }
};

class FileScanner {
public:
    FileScanner(const std::string& workspace,const std::string& hashAlgorithm="sha256");

    bool Scan();
    const std::vector<FileInfo>& GetFiles() const { return files; }
    const std::vector<DirectoryInfo>& GetDirectories() const { return directories; }

    // 计算文件哈希
    static std::string CalculateFileHash(const std::string& filePath,const std::string& algorithm);

    // 从JSON加载文件列表
    bool LoadFromJson(const Json::Value& json);

    // 转换为JSON
    Json::Value ToJson() const;

private:
    std::string workspace;
    std::string hashAlgorithm;
    std::vector<FileInfo> files;
    std::vector<DirectoryInfo> directories;

    void ScanDirectory(const std::filesystem::path& currentPath,const std::string& relativePath="");
    std::string NormalizePath(const std::string& path) const;
};

#endif