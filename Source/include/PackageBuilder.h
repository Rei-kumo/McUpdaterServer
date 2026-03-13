#ifndef PACKAGEBUILDER_H
#define PACKAGEBUILDER_H

#include <string>
#include <vector>
#include <zip.h>
#include <json/json.h>
#include "DiffEngine.h"

class PackageBuilder {
public:
    PackageBuilder()=default;
    ~PackageBuilder()=default;

    // 创建增量更新包
    bool CreateIncrementalPackage(
        const std::string& oldVersion,
        const std::string& newVersion,
        const std::vector<ChangeRecord>& changes,
        const std::string& workspace,
        const std::string& outputPath);

    // 创建全量更新包
    bool CreateFullPackage(
        const std::string& version,
        const std::vector<FileInfo>& files,
        const std::vector<DirectoryInfo>& dirs,
        const std::string& workspace,
        const std::string& outputPath);

    // 创建目录包（仅包含直接文件和空子目录条目）
    bool CreateDirectoryPackage(
        const std::string& rootDir,
        const std::vector<DirectoryInfo>& allDirs,
        const std::vector<FileInfo>& allFiles,
        const std::string& workspace,
        const std::string& outputPath);

    // 添加清单到ZIP
    static bool AddManifestToZip(zip_t* zip,const std::string& manifest);

private:
    // 添加文件到ZIP
    bool AddFileToZip(zip_t* zip,const std::string& filePath,const std::string& zipPath);

        /*
    bool AddDirectoryToZip(
        zip_t* zip,
        const std::string& dirPath,
        const std::unordered_map<std::string,std::vector<FileInfo>>& dirFilesMap,
        const std::unordered_map<std::string,std::vector<Json::String>>& subdirsMap,
        const std::string& workspace,
        bool includeSubdirMarkers);
        */

    bool AddEmptyDirectoryMarker(zip_t* zip,const std::string& dirPath);
    bool AddDirectoryRecursively(zip_t* zip,const std::string& physicalPath,const std::string& zipPath);
};

#endif