#include "VersionManager.h"
#include "Language.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unordered_set>
#include <queue>
#include <filesystem>
#include "Logger.h"

VersionManager::VersionManager(const std::string& dataDir)
    : dataDir(dataDir) {
}

bool VersionManager::Initialize() {
    std::filesystem::create_directories(dataDir);
    LoadVersions();
    BuildVersionGraph();
    return true;
}

void VersionManager::LoadVersions() {
    std::string versionsFile=GetVersionsFile();
    if(!std::filesystem::exists(versionsFile)) {
        return;
    }

    std::ifstream file(versionsFile);
    if(!file.is_open()) {
        g_logger<<LANG("error_config")<<LANG("error_open_file")<<std::endl;
        return;
    }

    Json::CharReaderBuilder reader;
    std::string errors;
    Json::Value json;
    //FIXME:如果 JSON 损坏，versions 可能为空，但程序继续运行。后续添加版本时可能覆盖损坏的文件。
    // 至少应返回错误给调用者，但 LoadVersions 被 Initialize 调用，而 Initialize 总是返回 true。这会导致失败。
    if(!Json::parseFromStream(reader,file,&json,&errors)) {
        g_logger<<LANG("error_config")<<errors<<std::endl;
        return;
    }

    if(json.isMember("versions")&&json["versions"].isArray()) {
        for(const auto& versionJson:json["versions"]) {
            VersionInfo info;
            info.version=versionJson["version"].asString();
            info.timestamp=versionJson["timestamp"].asUInt64();
            info.manifestHash=versionJson["manifest_hash"].asString();

            if(versionJson.isMember("incremental_from")) {
                for(const auto& from:versionJson["incremental_from"]) {
                    info.incrementalFrom.push_back(from.asString());
                }
            }

            if(versionJson.isMember("files")) {
                for(const auto& file:versionJson["files"]) {
                    info.files.push_back(file.asString());
                }
            }

            if(versionJson.isMember("directories")) {
                for(const auto& dir:versionJson["directories"]) {
                    info.directories.push_back(dir.asString());
                }
            }

            versions[info.version]=info;
        }
    }
}

bool VersionManager::Save() {
    return SaveVersions();
}

bool VersionManager::SaveVersions() const {
    std::string versionsFile=GetVersionsFile();
    std::ofstream file(versionsFile);
    if(!file.is_open()) {
        g_logger<<LANG("error_config")<<LANG("error_open_file")<<std::endl;
        return false;
    }

    Json::Value json;
    Json::Value versionsArray(Json::arrayValue);

    for(const auto& [version,info]:versions) {
        versionsArray.append(info.ToJson());
    }

    json["versions"]=versionsArray;

    Json::StreamWriterBuilder writer;
    writer["indentation"]="  ";
    std::string jsonString=Json::writeString(writer,json);
	//FIXME:写入可能部分失败（如磁盘满），但函数仍返回 true。
    file<<jsonString;

    return true;
}
//TIP:版本号格式在UpdateGenerator已经验证
//FIXEME:未检查 incrementalFrom 是否已存在,但若手动调用 AddVersion 时传入重复，可能破坏数据
bool VersionManager::AddVersion(const VersionInfo& version) {
    if(versions.find(version.version)!=versions.end()) {
        return false;  // 版本已存在
    }

    versions[version.version]=version;
    BuildVersionGraph();
    return SaveVersions();
}

bool VersionManager::RemoveVersion(const std::string& version) {
    auto it=versions.find(version);
    if(it==versions.end()) {
        return false;
    }

    versions.erase(it);
    BuildVersionGraph();
    return SaveVersions();
}

const VersionInfo* VersionManager::GetVersion(const std::string& version) const {
    auto it=versions.find(version);
    if(it==versions.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> VersionManager::GetVersionList() const {
    std::vector<std::string> result;
    for(const auto& [version,_]:versions) {
        result.push_back(version);
    }

    // 按版本排序
	//FIXME: 这里的排序可能不太合理，字典序比较会认为 "1.10.0" < "1.2.0"
    std::sort(result.begin(),result.end(),[](const std::string& a,const std::string& b) {
        return a<b;
        });

    return result;
}
//FIXME: BFS 逻辑有误
/*错误的方向：toInfo->incrementalFrom 表示可以从哪些版本升级到 toVersion，即 incrementalFrom 是来源版本。但在 BFS 中，q 中存放当前版本，然后从 currentInfo->incrementalFrom 中找下一个版本，这实际上是在反向搜索：incrementalFrom 是依赖的旧版本，所以从旧版本到新版本应该是 old -> new，而 incrementalFrom 存储的是旧版本列表，因此要到达 toVersion，应该从 fromVersion 开始，不断查找哪些版本可以通过增量包升级到当前版本？不，我们需要正向路径：从 fromVersion 开始，找可以升级到的下一个版本（即 fromVersion 出现在某个版本的 incrementalFrom 中）。因此，正确的正向搜索应该维护一个映射：版本 A 可以通过增量包升级到哪些版本。但现有数据结构只有 incrementalFrom（反向索引）。若要正向搜索，需要构建反向索引或使用 BFS 从目标版本反向搜索到源版本（逆向路径），然后将路径反转。当前代码尝试正向搜索但使用了错误的方向，导致无法找到正确路径。

可达性检查有误：if(std::find(currentInfo->incrementalFrom.begin(), currentInfo->incrementalFrom.end(), current) != ...) 是在检查当前版本是否在 currentInfo->incrementalFrom 中？这实际上是检查 current 能否升级到 currentInfo？但 currentInfo 是当前版本，current 也是当前版本，逻辑混乱。

实际运行时，GetUpdatePath 可能返回空路径或错误路径，导致客户端无法正确获取更新链。*/
std::vector<std::string> VersionManager::GetUpdatePath(
    const std::string& fromVersion,
    const std::string& toVersion) const {

    std::vector<std::string> path;

    // 确保目标版本存在
    const VersionInfo* toInfo=GetVersion(toVersion);
    if(!toInfo) {
        return path;
    }

    // 检查是否可以直达
    if(std::find(toInfo->incrementalFrom.begin(),
        toInfo->incrementalFrom.end(),
        fromVersion)!=toInfo->incrementalFrom.end()) {
        path.push_back(toVersion);
        return path;
    }

    // 使用BFS查找路径
    std::queue<std::pair<std::string,std::vector<std::string>>> q;
    std::unordered_set<std::string> visited;

    q.push({fromVersion,{}});
    visited.insert(fromVersion);

    while(!q.empty()) {
        auto [current,currentPath]=q.front();
        q.pop();

        // 检查当前版本是否可以升级到目标版本
        const VersionInfo* currentInfo=GetVersion(current);
        if(!currentInfo) {
            continue;
        }

        // 找到直达路径
        if(std::find(currentInfo->incrementalFrom.begin(),
            currentInfo->incrementalFrom.end(),
            current)!=currentInfo->incrementalFrom.end()) {
            currentPath.push_back(toVersion);
            return currentPath;
        }

        // 查找下一个可升级的版本
        for(const auto& nextVersion:currentInfo->incrementalFrom) {
            if(visited.find(nextVersion)==visited.end()) {
                visited.insert(nextVersion);
                std::vector<std::string> newPath=currentPath;
                newPath.push_back(nextVersion);
                q.push({nextVersion,newPath});
            }
        }
    }

    return path;
}
//FIXME:版本号字符串比较不是语义比较，且“较小的版本”不一定是共同祖先。
std::string VersionManager::FindCommonAncestor(
    const std::string& version1,
    const std::string& version2) const {

    // 简单实现：返回版本号较小的那个
    return version1<version2?version1:version2;
}

void VersionManager::BuildVersionGraph() {
    // 这里可以构建版本依赖关系的图结构
    // 暂时使用简单的实现
}

std::string VersionManager::GetVersionsFile() const {
    return dataDir+"/versions.json";
}
//Fixme:删除后，还应清理可能存在的正向关系（已无，因拒绝删除）。
bool VersionManager::DeleteVersion(const std::string& version) {
    // 版本不存在
    if(versions.find(version)==versions.end())
        return false;

    // ---------- 新增安全检查 ----------
    // 检查是否有其他版本依赖于该版本（即该版本是其他版本的增量来源）
    for(const auto& [ver,info]:versions) {
        if(ver==version) continue;
        if(std::find(info.incrementalFrom.begin(),info.incrementalFrom.end(),version)
            !=info.incrementalFrom.end()) {
            g_logger<<"[ERROR] Cannot delete version "<<version
                <<" because it is a base for other versions."<<std::endl;
            return false;
        }
    }
    // --------------------------------

    // 从其他版本的增量来源列表中移除该版本（实际上由于上面检查，这里不会执行）
    for(auto& [ver,info]:versions) {
        if(ver==version) continue;
        auto& fromList=info.incrementalFrom;
        fromList.erase(std::remove(fromList.begin(),fromList.end(),version),fromList.end());
    }

    // 删除该版本
    versions.erase(version);

    // 保存更新后的 versions.json
    if(!SaveVersions())
        return false;

    // 获取输出目录（dataDir 的父目录）
    std::filesystem::path outDir=std::filesystem::path(dataDir).parent_path();
	//FIXME: 删除文件时未检查返回值
    // 删除快照文件
    std::filesystem::remove(outDir/"snapshots"/(version+".json"));

    // 删除全量包
    std::filesystem::remove(outDir/"full"/(version+".zip"));

    // 删除涉及该版本的增量包
    std::filesystem::path incDir=outDir/"incremental";
    if(std::filesystem::exists(incDir)) {
        std::error_code ec;
        for(const auto& entry:std::filesystem::directory_iterator(incDir,ec)) {
            if(ec) break;
            if(!entry.is_regular_file()) continue;
            std::string filename=entry.path().filename().string();
            if(filename.find(version+"_to_")!=std::string::npos||
                filename.find("_to_"+version)!=std::string::npos) {
                std::filesystem::remove(entry.path(),ec);
            }
        }
    }

    return true;
}