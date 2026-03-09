#ifndef UPDATEGENERATOR_H
#define UPDATEGENERATOR_H

#include <string>
#include <vector>
#include <memory>
#include "Config.h"
#include "FileScanner.h"
#include "DiffEngine.h"
#include "PackageBuilder.h"
#include "VersionManager.h"

class UpdateGenerator {
public:
    UpdateGenerator(const Config& config);

    bool Initialize();

    // 生成新版本
    bool GenerateVersion(const std::string& version,const std::string& description="");

    // 生成增量更新包
    bool GenerateIncrementalPackage(
        const std::string& fromVersion,
        const std::string& toVersion);

    // 生成全量更新包
    bool GenerateFullPackage(const std::string& version);

    // 扫描并构建版本
    bool ScanAndBuild();
    bool RollbackToVersion(const std::string& targetVersion,
        const std::string& newVersion,
        const std::string& description="");    
    bool GetPreviousVersionFiles(
        const std::string& version,
        std::vector<FileInfo>& files,
        std::vector<DirectoryInfo>& dirs);   
    bool CreateDirectoryPackages(
        const std::string& version,
        const std::vector<DirectoryInfo>& dirs);
private:
    Config config;
    std::unique_ptr<FileScanner> scanner;
    std::unique_ptr<VersionManager> versionManager;

    std::vector<FileInfo> currentFiles;
    std::vector<DirectoryInfo> currentDirs;

    // 获取前一个版本的文件列表


    // 保存版本快照
    bool SaveVersionSnapshot(
        const std::string& version,
        const std::vector<FileInfo>& files,
        const std::vector<DirectoryInfo>& dirs);

    // 创建目录包

};

#endif