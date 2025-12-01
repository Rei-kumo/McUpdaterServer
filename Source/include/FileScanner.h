#ifndef FILESCANNER_H
#define FILESCANNER_H

#include <string>
#include <json/value.h>
#include <filesystem>
#include <unordered_map>

class FileScanner {
private:
    std::string publicDir;
    std::string deleteListDir;
    std::string hashCacheFile;
    std::unordered_map<std::string,std::string> dirHashCache;

    void ScanDirectory(const std::filesystem::path& dirPath,const std::string& relativePath,
        Json::Value& files,Json::Value& directories,
        const std::string& baseUrl,const std::string& hashAlgorithm);

    std::string CalculateDirectoryContentHash(const std::filesystem::path& dirPath,
        const std::string& hashAlgorithm);

    bool LoadHashCache();
    bool SaveHashCache();

public:
    FileScanner(const std::string& publicDir,const std::string& deleteListDir);

    void SetHashCacheFile(const std::string& cacheFile);

    Json::Value ScanFiles(const std::string& baseUrl,const std::string& hashAlgorithm);
    Json::Value GenerateDeleteList();
};

#endif