// UpdateGenerator.cpp - 修改GenerateFullPackage函数和CreateDirectoryPackages函数
#include <regex>
#include "UpdateGenerator.h"
#include "Language.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include "Logger.h"
#include <unordered_set>

UpdateGenerator::UpdateGenerator(const Config& config)
    : config(config) {
}
bool CompareVersion(const std::string& v1,const std::string& v2) {
    // 使用正则表达式分割版本号
    std::regex pattern(R"(\.)");
    std::sregex_token_iterator it1(v1.begin(),v1.end(),pattern,-1);
    std::sregex_token_iterator it2(v2.begin(),v2.end(),pattern,-1);
    std::sregex_token_iterator end;

    std::vector<int> parts1,parts2;

    while(it1!=end) {
        parts1.push_back(std::stoi(*it1));
        ++it1;
    }

    while(it2!=end) {
        parts2.push_back(std::stoi(*it2));
        ++it2;
    }

    // 比较版本号的每个部分
    size_t max_parts=std::max(parts1.size(),parts2.size());
    for(size_t i=0; i<max_parts; ++i) {
        int part1=(i<parts1.size())?parts1[i]:0;
        int part2=(i<parts2.size())?parts2[i]:0;

        if(part1<part2) return true;
        if(part1>part2) return false;
    }

    return false; // 所有部分都相等
}
bool UpdateGenerator::Initialize() {
    // 确保输出目录存在
    std::filesystem::create_directories(config.GetOutputDir());

    // 初始化版本管理器
    versionManager=std::make_unique<VersionManager>(config.GetOutputDir()+"/data");
    if(!versionManager->Initialize()) {
        g_logger<<LANG("error_init_version_manager")<<std::endl;
        return false;
    }

    // 初始化文件扫描器（工作空间固定为public）
    scanner=std::make_unique<FileScanner>(
        config.GetWorkspace(),  // 固定为public
        config.GetHashAlgorithm());

    return true;
}

bool UpdateGenerator::GenerateVersion(const std::string& version,const std::string& description) {
    // 验证版本号格式
    std::regex versionPattern(R"(^\d+\.\d+\.\d+$)");
    if(!std::regex_match(version,versionPattern)) {
        g_logger<<LANG("error_version_format")<<": "<<version<<std::endl;
        return false;
    }

    // 检查版本是否已存在
    const VersionInfo* existingVersion=versionManager->GetVersion(version);
    if(existingVersion!=nullptr) {
        g_logger<<LANG("error_package_exists")<<": "<<version<<std::endl;
        return false;
    }

    g_logger<<LANG("info_version")<<version<<std::endl;

    // 扫描当前工作空间
    if(!scanner->Scan()) {
        g_logger<<LANG("error_scan")<<std::endl;
        return false;
    }

    currentFiles=scanner->GetFiles();
    currentDirs=scanner->GetDirectories();

    // 获取现有版本列表（在保存新版本之前）
    auto versions=versionManager->GetVersionList();

    // 保存版本快照
    if(!SaveVersionSnapshot(version,currentFiles,currentDirs)) {
        return false;
    }

    // 如果是第一个版本，只生成全量包
    if(versions.empty()) {
        g_logger<<"[INFO] "<<LANG("info_first_starting")<<std::endl;
        if(!GenerateFullPackage(version)) {
            g_logger<<LANG("error_package")<<LANG("info_full")<<std::endl;
            return false;
        }
    }
    else {
        // 如果不是第一个版本，生成增量包
        std::string previousVersion=versions.back();

        // 验证版本顺序
        if(!CompareVersion(previousVersion,version)) {
            g_logger<<"[ERROR] "<<LANG("error_version_order")<<": "
                <<previousVersion<<" -> "<<version<<std::endl;
            return false;
        }

        if(!GenerateIncrementalPackage(previousVersion,version)) {
            g_logger<<LANG("error_package")<<LANG("info_incremental")<<std::endl;
            return false;
        }

        // 生成全量包
        if(!GenerateFullPackage(version)) {
            g_logger<<LANG("error_package")<<LANG("info_full")<<std::endl;
            return false;
        }
    }

    // 创建目录包
    if(!CreateDirectoryPackages(version,currentDirs)) {
        g_logger<<LANG("error_package")<<": "<<LANG("info_directory")<<std::endl;
        return false;
    }

    g_logger<<"[INFO] "<<LANG("info_version")<<version<<LANG("info_created_complete")<<std::endl;
    return true;
}

bool UpdateGenerator::GenerateIncrementalPackage(
    const std::string& fromVersion,
    const std::string& toVersion) {

    // 检查版本号格式
    auto validateVersionFormat=[](const std::string& version) -> bool {
        std::regex versionPattern(R"(^\d+\.\d+\.\d+$)");
        return std::regex_match(version,versionPattern);
        };

    if(!validateVersionFormat(fromVersion)||!validateVersionFormat(toVersion)) {
        g_logger<<LANG("error_version_format")<<": "<<fromVersion<<" -> "<<toVersion<<std::endl;
        return false;
    }

    // 检查是否从较低版本到较高版本
    auto compareVersions=[](const std::string& v1,const std::string& v2) -> bool {
        std::vector<int> parts1,parts2;
        std::stringstream ss1(v1),ss2(v2);
        std::string part;

        while(std::getline(ss1,part,'.')) parts1.push_back(std::stoi(part));
        while(std::getline(ss2,part,'.')) parts2.push_back(std::stoi(part));

        for(size_t i=0; i<std::min(parts1.size(),parts2.size()); ++i) {
            if(parts1[i]!=parts2[i]) {
                return parts1[i]<parts2[i];
            }
        }
        return parts1.size()<parts2.size();
        };

    if(!compareVersions(fromVersion,toVersion)) {
        g_logger<<LANG("error_version_order")<<": "<<fromVersion<<" -> "<<toVersion<<std::endl;
        return false;
    }

    g_logger<<LANG("package_building_incremental")<<fromVersion<<LANG("info_to")<<toVersion<<std::endl;

    // 检查增量包是否已存在
    std::string incrementalDir=config.GetOutputDir()+"/incremental";
    std::filesystem::create_directories(incrementalDir);
    std::string packageName=fromVersion+"_to_"+toVersion+".zip";
    std::string packagePath=incrementalDir+"/"+packageName;

    if(std::filesystem::exists(packagePath)) {
        g_logger<<LANG("info_package_exists")<<": "<<packagePath<<std::endl;
        return false;
    }

    // 获取旧版本文件
    std::vector<FileInfo> oldFiles;
    std::vector<DirectoryInfo> oldDirs;
    if(!GetPreviousVersionFiles(fromVersion,oldFiles,oldDirs)) {
        return false;
    }

    // 计算差异
    DiffEngine diffEngine;
    auto changes=diffEngine.CalculateDiff(oldFiles,currentFiles,oldDirs,currentDirs);

    if(changes.empty()) {
        g_logger<<LANG("info_no_changes")<<std::endl;
        return true;
    }

    // 创建增量包
    PackageBuilder builder;
    if(!builder.CreateIncrementalPackage(
        fromVersion,toVersion,changes,
        config.GetWorkspace(),packagePath)) {
        return false;
    }

    return true;
}

bool UpdateGenerator::GenerateFullPackage(const std::string& version) {
    g_logger<<LANG("package_building_full")<<version<<std::endl;

    PackageBuilder builder;
    std::string fullDir=config.GetOutputDir()+"/full";
    std::filesystem::create_directories(fullDir);
    std::string packagePath=fullDir+"/"+version+".zip";  // 使用版本号命名

    // 如果包已存在，询问是否覆盖
    if(std::filesystem::exists(packagePath)) {
        g_logger<<"[WARNING] "<<LANG("info_full")<<version<<LANG("error_zip_exists")<<std::endl;
        std::filesystem::remove(packagePath);
    }

    // 传递目录信息到全量包构建器
    if(!builder.CreateFullPackage(
        version,currentFiles,currentDirs,
        config.GetWorkspace(),packagePath)) {
        return false;
    }

    g_logger<<"[INFO] "<<LANG("info_full")<<version<<LANG("info_zip_becreated")<<std::endl;
    return true;
}

bool UpdateGenerator::ScanAndBuild() {
    if(!scanner->Scan()) {
        return false;
    }

    currentFiles=scanner->GetFiles();
    currentDirs=scanner->GetDirectories();

    // 将当前状态保存为JSON
    Json::Value snapshot=scanner->ToJson();

    std::string snapshotFile=config.GetOutputDir()+"/current_snapshot.json";
    std::ofstream file(snapshotFile);
    if(!file) {
        g_logger<<LANG("error_io")<<": "<<LANG("error_save_snapshot")<<std::endl;
        return false;
    }

    Json::StreamWriterBuilder writer;
    writer["indentation"]="  ";
    std::string jsonString=Json::writeString(writer,snapshot);
    file<<jsonString;

    g_logger<<LANG("info_snapshot_saved")<<snapshotFile<<std::endl;
    return true;
}

bool UpdateGenerator::GetPreviousVersionFiles(
    const std::string& version,
    std::vector<FileInfo>& files,
    std::vector<DirectoryInfo>& dirs) {

    const VersionInfo* versionInfo=versionManager->GetVersion(version);
    if(!versionInfo) {
        g_logger<<LANG("error_version_not_exist")<<version<<std::endl;
        return false;
    }

    std::string snapshotFile=config.GetOutputDir()+"/snapshots/"+version+".json";
    if(!std::filesystem::exists(snapshotFile)) {
        g_logger<<LANG("error_version_snapshot_not_exist")<<snapshotFile<<std::endl;
        return false;
    }

    std::ifstream file(snapshotFile);
    if(!file.is_open()) {
        g_logger<<LANG("error_open_file")<<snapshotFile<<std::endl;
        return false;
    }

    Json::CharReaderBuilder reader;
    std::string errors;
    Json::Value snapshot;
    if(!Json::parseFromStream(reader,file,&snapshot,&errors)) {
        g_logger<<LANG("error_parse_json")<<errors<<std::endl;
        return false;
    }

    // 使用临时 FileScanner 加载快照
    FileScanner tempScanner("");  // workspace 参数在此无关
    if(!tempScanner.LoadFromJson(snapshot)) {
        return false;
    }
    files=tempScanner.GetFiles();
    dirs=tempScanner.GetDirectories();
    return true;
}

bool UpdateGenerator::SaveVersionSnapshot(
    const std::string& version,
    const std::vector<FileInfo>& files,
    const std::vector<DirectoryInfo>& dirs) {

    // 创建快照目录
    std::string snapshotsDir=config.GetOutputDir()+"/snapshots";
    std::filesystem::create_directories(snapshotsDir);

    // 保存为JSON
    std::string snapshotFile=snapshotsDir+"/"+version+".json";
    std::ofstream file(snapshotFile);
    if(!file) {
        g_logger<<LANG("error_create_file")<<snapshotFile<<std::endl;
        return false;
    }

    Json::Value snapshot;

    // 保存文件列表
    Json::Value filesJson(Json::arrayValue);
    for(const auto& fileInfo:files) {
        filesJson.append(fileInfo.ToJson());
    }
    snapshot["files"]=filesJson;

    // 保存目录列表
    Json::Value dirsJson(Json::arrayValue);
    for(const auto& dirInfo:dirs) {
        dirsJson.append(dirInfo.ToJson());
    }
    snapshot["directories"]=dirsJson;

    snapshot["version"]=version;
    snapshot["timestamp"]=static_cast<Json::Int64>(std::time(nullptr));

    Json::StreamWriterBuilder writer;
    writer["indentation"]="  ";
    std::string jsonString=Json::writeString(writer,snapshot);
    file<<jsonString;

    // 添加到版本管理器
    VersionInfo versionInfo;
    versionInfo.version=version;
    versionInfo.timestamp=std::time(nullptr);

    for(const auto& fileInfo:files) {
        versionInfo.files.push_back(fileInfo.path);
    }

    for(const auto& dirInfo:dirs) {
        versionInfo.directories.push_back(dirInfo.path);
    }

    // 添加增量关系
    auto versions=versionManager->GetVersionList();
    if(!versions.empty()) {
        versionInfo.incrementalFrom.push_back(versions.back());
    }

    return versionManager->AddVersion(versionInfo);
}

bool UpdateGenerator::CreateDirectoryPackages(
    const std::string& version,
    const std::vector<DirectoryInfo>& dirs) {

    PackageBuilder builder;
    std::string packagesDir=config.GetOutputDir()+"/packages";
    std::filesystem::create_directories(packagesDir);

    // 1. 收集当前版本所有顶级目录对应的包名
    std::unordered_set<std::string> expectedPackages;
    for(const auto& dir:dirs) {
        // 只处理顶级目录（路径中不含 '/' 或为空）
        bool isTopLevel=(dir.path.find('/')==std::string::npos)||dir.path.empty();
        if(!isTopLevel) continue;
        std::string packageName=dir.path.empty()?"root.zip":dir.path+".zip";
        expectedPackages.insert(packageName);
    }

    // 2. 扫描 packages 目录，删除多余的包文件
    std::error_code ec;
    for(const auto& entry:std::filesystem::directory_iterator(packagesDir,ec)) {
        if(ec) break;
        if(!entry.is_regular_file()) continue;
        std::string filename=entry.path().filename().string();
        // 检查是否为 zip 文件
        if(filename.size()>4&&filename.substr(filename.size()-4)==".zip") {
            if(expectedPackages.find(filename)==expectedPackages.end()) {
                std::filesystem::remove(entry.path(),ec);
                if(ec) {
                    g_logger<<"[WARNING] "<<LANG("error_delete_morefiles")<<filename<<" - "<<ec.message()<<std::endl;
                }
                else {
                    g_logger<<"[INFO] "<<LANG("info_delet_successed")<<filename<<std::endl;
                }
            }
        }
    }

    // 3. 为每个当前顶级目录重新生成包
    const auto& allFiles=currentFiles;
    const auto& allDirs=currentDirs;

    for(const auto& dir:dirs) {
        bool isTopLevel=(dir.path.find('/')==std::string::npos)||dir.path.empty();
        if(!isTopLevel) continue;

        std::string packageName=dir.path.empty()?"root.zip":dir.path+".zip";
        std::string packagePath=packagesDir+"/"+packageName;

        if(!builder.CreateDirectoryPackage(dir.path,allDirs,allFiles,config.GetWorkspace(),packagePath)) {
            g_logger<<"[ERROR] "<<LANG("error_create_pathpackage")<<dir.path<<std::endl;
            return false;
        }
        g_logger<<"[INFO] "<<LANG("info_createdpath_package")<<packageName<<std::endl;
    }

    return true;
}
bool UpdateGenerator::RollbackToVersion(const std::string& targetVersion,
    const std::string& newVersion,
    const std::string& description) {
    // 验证目标版本存在
    const VersionInfo* targetInfo=versionManager->GetVersion(targetVersion);
    if(!targetInfo) {
        g_logger<<LANG("error_version_not_exist")<<": "<<targetVersion<<std::endl;
        return false;
    }

    // 验证新版本号格式
    std::regex versionPattern(R"(^\d+\.\d+\.\d+$)");
    if(!std::regex_match(newVersion,versionPattern)) {
        g_logger<<LANG("error_version_format")<<": "<<newVersion<<std::endl;
        return false;
    }

    // 检查新版本是否已存在
    if(versionManager->GetVersion(newVersion)!=nullptr) {
        g_logger<<LANG("error_package_exists")<<": "<<newVersion<<std::endl;
        return false;
    }

    // 获取所有版本列表并检查新版本是否大于所有现有版本（已在菜单中检查，但再次确认）
    auto versions=versionManager->GetVersionList();
    for(const auto& v:versions) {
        if(!CompareVersion(v,newVersion)) { // v >= newVersion
            g_logger<<"[ERROR] "<<LANG("error_version_small")<<std::endl;
            return false;
        }
    }

    // 获取当前最新版本
    std::string latestVersion=versions.back();

    g_logger<<"[INFO] "<<LANG("info_rollback_part1")<<latestVersion<<LANG("info_rollback_part2")<<targetVersion
        <<LANG("info_rollback_part3")<<newVersion<<std::endl;

    // 加载目标版本的快照
    std::vector<FileInfo> targetFiles;
    std::vector<DirectoryInfo> targetDirs;
    if(!GetPreviousVersionFiles(targetVersion,targetFiles,targetDirs)) {
        return false;
    }

    // 扫描当前工作空间（实际上我们只需要用目标版本的文件列表，但为了获取当前文件？不，我们并不需要当前文件，因为回退包是从最新版本到新版本）
    // 但为了生成增量包，我们需要最新版本的文件列表（即当前最新版本的快照）
    std::vector<FileInfo> latestFiles;
    std::vector<DirectoryInfo> latestDirs;
    if(!GetPreviousVersionFiles(latestVersion,latestFiles,latestDirs)) {
        return false;
    }

    // 计算从最新版本到目标版本的差异（即回退所需的更改）
    DiffEngine diffEngine;
    // 注意：DiffEngine 期望旧文件为最新版本，新文件为目标版本
    auto changes=diffEngine.CalculateDiff(latestFiles,targetFiles,latestDirs,targetDirs);

    // 如果没有变化（理论上不可能，因为 targetVersion != latestVersion，但以防万一）
    if(changes.empty()) {
        g_logger<<LANG("info_no_changes")<<std::endl;
        // 仍然可以创建一个内容相同的新版本，但无增量包
    }

    // 保存新版本的快照（使用目标版本的文件和目录）
    if(!SaveVersionSnapshot(newVersion,targetFiles,targetDirs)) {
        return false;
    }

    // 创建全量包（新版本）
    if(!GenerateFullPackage(newVersion)) {
        return false;
    }

    // 创建从最新版本到新版本的增量包（即回退包）
    std::string incrementalDir=config.GetOutputDir()+"/incremental";
    std::filesystem::create_directories(incrementalDir);
    std::string packageName=latestVersion+"_to_"+newVersion+".zip";
    std::string packagePath=incrementalDir+"/"+packageName;

    // 如果包已存在，询问是否覆盖（这里简单处理为覆盖）
    if(std::filesystem::exists(packagePath)) {
        std::filesystem::remove(packagePath);
    }

    PackageBuilder builder;
    if(!builder.CreateIncrementalPackage(
        latestVersion,newVersion,changes,
        config.GetWorkspace(),packagePath)) {
        return false;
    }

    // 创建目录包（新版本）
    if(!CreateDirectoryPackages(newVersion,targetDirs)) {
        return false;
    }

    // 更新版本信息：需要手动将新版本的 incrementalFrom 设置为最新版本
    // 但 SaveVersionSnapshot 已经添加了版本，我们需要修改它添加的 incrementalFrom
    // 我们可以在 SaveVersionSnapshot 中根据参数设置，但这里我们直接操作版本管理器
    // 由于 SaveVersionSnapshot 已经添加了一个版本（可能 incrementalFrom 为前一个版本？需检查其实现）
    // 在 SaveVersionSnapshot 中，它通过 versionManager->AddVersion 添加，其中 incrementalFrom 设置为 versions.back()
    // 但当时 versions 还未包含 newVersion，所以 incrementalFrom 会设置为之前的最新版本（即 latestVersion），这是正确的。
    // 不过我们刚刚添加了 newVersion，所以需要确保它确实被正确设置。
    // 为了保险，我们可以再次更新该版本的 incrementalFrom 为 latestVersion。
    // 但 AddVersion 已经做了，所以无需额外操作。

    g_logger<<"[INFO] "<<LANG("info_rollback_succed1")<<newVersion<<LANG("info_rollback_succed2")<<std::endl;
    return true;
}