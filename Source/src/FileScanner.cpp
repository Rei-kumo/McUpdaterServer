#include "FileScanner.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include "FileHasher.h"
#include "Logger.h"
#include "ZipManager.h"
#include <json/json.h>

namespace fs=std::filesystem;

FileScanner::FileScanner(const std::string& publicDir,const std::string& deleteListDir)
    : publicDir(publicDir),deleteListDir(deleteListDir) {
    hashCacheFile="cache/dir_hashes.json";
    LoadHashCache();
}

bool FileScanner::LoadHashCache() {
    try {
        std::ifstream file(hashCacheFile);
        if(!file.is_open()) {
            return false;
        }

        Json::Value cache;
        Json::Reader reader;

        if(reader.parse(file,cache)) {
            const Json::Value::Members& members=cache.getMemberNames();
            for(const auto& key:members) {
                dirHashCache[key]=cache[key].asString();
            }
            return true;
        }
    }
    catch(const std::exception& e) {
        std::cerr<<"[WARN] 无法加载哈希缓存: "<<e.what()<<std::endl;
    }
    return false;
}

bool FileScanner::SaveHashCache() {
    try {
        fs::create_directories(fs::path(hashCacheFile).parent_path());

        std::ofstream file(hashCacheFile);
        if(!file.is_open()) {
            return false;
        }

        Json::Value cache;
        for(const auto& [path,hash]:dirHashCache) {
            cache[path]=hash;
        }

        Json::StyledWriter writer;
        file<<writer.write(cache);

        return true;
    }
    catch(const std::exception& e) {
        std::cerr<<"[WARN] 无法保存哈希缓存: "<<e.what()<<std::endl;
        return false;
    }
}

std::string FileScanner::CalculateDirectoryContentHash(const std::filesystem::path& dirPath,
    const std::string& hashAlgorithm) {

    std::vector<unsigned char> combinedData;

    try {
        std::vector<std::filesystem::path> files;
        for(const auto& entry:fs::recursive_directory_iterator(dirPath)) {
            if(entry.is_regular_file()) {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(),files.end());

        for(const auto& filePath:files) {
            std::string relativePath=fs::relative(filePath,dirPath).string();
            std::replace(relativePath.begin(),relativePath.end(),'\\','/');

            for(char c:relativePath) {
                combinedData.push_back(static_cast<unsigned char>(c));
            }
        }
    }
    catch(const std::exception& e) {
        std::cerr<<"[ERROR] 计算目录哈希时出错: "<<dirPath<<" - "<<e.what()<<std::endl;
        return "";
    }

    return FileHasher::CalculateMemoryHash(combinedData,hashAlgorithm);
}

void FileScanner::SetHashCacheFile(const std::string& cacheFile) {
    hashCacheFile=cacheFile;
    LoadHashCache();
}

Json::Value FileScanner::ScanFiles(const std::string& baseUrl,const std::string& hashAlgorithm) {
    Json::Value result;
    Json::Value files(Json::arrayValue);
    Json::Value directories(Json::arrayValue);

    try {
        std::cout<<"[INFO] 扫描目录: "<<publicDir<<std::endl;
        ScanDirectory(fs::path(publicDir),"",files,directories,baseUrl,hashAlgorithm);
        std::cout<<"[INFO] 目录扫描完成"<<std::endl;

        SaveHashCache();
    }
    catch(const std::exception& e) {
        std::cerr<<"[ERROR] 扫描目录异常: "<<e.what()<<std::endl;
        g_logger<<"[ERROR]扫描目录异常: "<<e.what()<<std::endl;
    }

    result["files"]=files;
    result["directories"]=directories;
    return result;
}

Json::Value FileScanner::GenerateDeleteList() {
    Json::Value deleteList(Json::arrayValue);

    try {
        std::cout<<"[INFO] 生成删除列表..."<<std::endl;
        for(const auto& entry:fs::directory_iterator(deleteListDir)) {
            if(entry.is_regular_file()) {
                std::cout<<"[INFO] 读取删除列表文件: "<<entry.path().filename().string()<<std::endl;
                std::ifstream file(entry.path());
                std::string line;
                while(std::getline(file,line)) {
                    if(!line.empty()) {
                        deleteList.append(line);
                        std::cout<<"[INFO] 添加待删除文件: "<<line<<std::endl;
                    }
                }
                file.close();
            }
        }
        std::cout<<"[INFO] 删除列表生成完成，共 "<<deleteList.size()<<" 个文件"<<std::endl;
    }
    catch(const std::exception& e) {
        std::cerr<<"[ERROR] 生成删除列表异常: "<<e.what()<<std::endl;
        g_logger<<"[ERROR]生成删除列表异常: "<<e.what()<<std::endl;
    }

    return deleteList;
}

void FileScanner::ScanDirectory(const fs::path& dirPath,const std::string& relativePath,
    Json::Value& files,Json::Value& directories,
    const std::string& baseUrl,const std::string& hashAlgorithm) {

    try {
        if(!fs::exists(dirPath)) {
            std::cerr<<"[ERROR] 扫描目录不存在: "<<dirPath<<std::endl;
            return;
        }

        for(const auto& entry:fs::directory_iterator(dirPath)) {
            if(!fs::exists(entry.path())) {
                std::cerr<<"[WARN] 跳过无效条目: "<<entry.path()<<std::endl;
                continue;
            }

            std::string filename=entry.path().filename().string();
            std::string currentRelativePath=relativePath.empty()?filename:relativePath+"/"+filename;

            if(entry.is_regular_file()&&entry.path().extension()==".zip") {
                std::string dirName=entry.path().stem().string();
                fs::path potentialDirPath=entry.path().parent_path()/dirName;

                if(fs::exists(potentialDirPath)&&fs::is_directory(potentialDirPath)) {
                    std::cout<<"[INFO] 跳过目录ZIP文件: "<<currentRelativePath<<std::endl;
                    continue;
                }
            }

            if(entry.is_regular_file()) {
                std::cout<<"[INFO] 扫描文件: "<<currentRelativePath<<std::endl;

                try {
                    Json::Value fileInfo;
                    fileInfo["path"]=currentRelativePath;
                    fileInfo["url"]=baseUrl+currentRelativePath;
                    fileInfo["type"]="file";
                    fileInfo["hash"]=FileHasher::CalculateFileHash(entry.path().string(),hashAlgorithm);

                    files.append(fileInfo);
                    g_logger<<"[INFO] 扫描文件: "<<currentRelativePath<<std::endl;
                }
                catch(const std::exception& e) {
                    std::cerr<<"[ERROR] 处理文件失败: "<<currentRelativePath<<" - "<<e.what()<<std::endl;
                }
            }
            else if(entry.is_directory()) {
                std::cout<<"[INFO] 扫描目录: "<<currentRelativePath<<std::endl;

                std::string zipFilename=currentRelativePath+".zip";
                std::string zipPath=publicDir+"/"+zipFilename;

                bool zipExists=fs::exists(zipPath);

                std::string currentHash=CalculateDirectoryContentHash(entry.path(),hashAlgorithm);
                std::string cachedHash=dirHashCache[currentRelativePath];

                bool needCreateZip=true;

                if(zipExists&&!currentHash.empty()&&currentHash==cachedHash) {
                    std::cout<<"[INFO] 目录未变化，使用现有ZIP文件: "<<currentRelativePath
                        <<" 哈希: "<<currentHash.substr(0,8)<<std::endl;
                    needCreateZip=false;
                }
                else {
                    if(!zipExists) {
                        std::cout<<"[INFO] ZIP文件不存在，需要创建: "<<currentRelativePath<<std::endl;
                    }
                    else if(currentHash.empty()) {
                        std::cout<<"[INFO] 无法计算目录哈希，重新创建ZIP: "<<currentRelativePath<<std::endl;
                    }
                    else {
                        std::cout<<"[INFO] 目录已变化，重新创建ZIP: "<<currentRelativePath
                            <<" (旧哈希: "<<(cachedHash.empty()?"无":cachedHash.substr(0,8))
                            <<", 新哈希: "<<currentHash.substr(0,8)<<")"<<std::endl;
                    }
                }

                try {
                    if(needCreateZip) {
                        bool success=ZipManager::CreateZipFromDirectory(entry.path().string(),zipPath);
                        if(success) {
                            dirHashCache[currentRelativePath]=currentHash;
                            std::cout<<"[INFO] 已更新目录哈希缓存: "<<currentRelativePath
                                <<" -> "<<currentHash.substr(0,8)<<std::endl;
                        }
                        else {
                            std::cerr<<"[ERROR] 创建ZIP文件失败，跳过目录: "<<currentRelativePath<<std::endl;
                            continue;
                        }
                    }
                    else {
                        dirHashCache[currentRelativePath]=currentHash;
                    }

                    Json::Value dirInfo;
                    dirInfo["path"]=currentRelativePath;
                    dirInfo["url"]=baseUrl+zipFilename;
                    dirInfo["hash"]=currentHash;

                    Json::Value contents(Json::arrayValue);
                    int contentCount=0;

                    try {
                        for(const auto& subEntry:fs::recursive_directory_iterator(entry.path())) {
                            if(subEntry.is_regular_file()&&fs::exists(subEntry.path())) {
                                std::string subRelativePath=fs::relative(subEntry.path(),entry.path()).string();
                                std::replace(subRelativePath.begin(),subRelativePath.end(),'\\','/');

                                Json::Value contentInfo;
                                contentInfo["path"]=subRelativePath;
                                contentInfo["hash"]=FileHasher::CalculateFileHash(subEntry.path().string(),hashAlgorithm);

                                contents.append(contentInfo);
                                contentCount++;
                            }
                        }
                    }
                    catch(const std::exception& e) {
                        std::cerr<<"[ERROR] 扫描目录内容失败: "<<entry.path()<<" - "<<e.what()<<std::endl;
                    }

                    dirInfo["contents"]=contents;
                    directories.append(dirInfo);
                    std::cout<<"[INFO] 目录处理完成: "<<currentRelativePath<<" (包含 "<<contentCount<<" 个文件)"<<std::endl;
                }
                catch(const std::exception& e) {
                    std::cerr<<"[ERROR] 处理目录异常: "<<currentRelativePath<<" - "<<e.what()<<std::endl;
                }
            }
        }
    }
    catch(const std::exception& e) {
        std::cerr<<"[ERROR] 扫描目录过程中发生异常: "<<dirPath<<" - "<<e.what()<<std::endl;
        g_logger<<"[ERROR] 扫描目录过程中发生异常: "<<dirPath<<" - "<<e.what()<<std::endl;
    }
}