#include "FileScanner.h"
#include "Language.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <filesystem>
#include <chrono>
#include "Logger.h"

#ifdef _WIN32
#include <windows.h>
static std::wstring Utf8ToWide(const std::string& utf8) {
    if(utf8.empty()) return L"";
    int wlen=MultiByteToWideChar(CP_UTF8,0,utf8.c_str(),-1,nullptr,0);
    if(wlen<=0) return L"";
    std::wstring wstr(wlen-1,0);
    MultiByteToWideChar(CP_UTF8,0,utf8.c_str(),-1,&wstr[0],wlen);
    return wstr;
}

static std::string WideToUtf8(const std::wstring& wide) {
    if(wide.empty()) return "";
    int len=WideCharToMultiByte(CP_UTF8,0,wide.c_str(),-1,nullptr,0,nullptr,nullptr);
    if(len<=0) return "";
    std::string str(len-1,0);
    WideCharToMultiByte(CP_UTF8,0,wide.c_str(),-1,&str[0],len,nullptr,nullptr);
    return str;
}
#endif

FileScanner::FileScanner(const std::string& workspace,const std::string& hashAlgorithm)
    : workspace(workspace),hashAlgorithm(hashAlgorithm) {
}

bool FileScanner::Scan() {
    files.clear();
    directories.clear();

    if(!std::filesystem::exists(workspace)) {
        g_logger<<LANG("error_scan")<<": "<<LANG("error_file_not_found")<<": "<<workspace<<std::endl;
        return false;
    }

    g_logger<<LANG("scan_start")<<": "<<workspace<<std::endl;

    try {
        ScanDirectory(workspace);
        g_logger<<LANG("scan_complete")<<": "<<files.size()<<" files, "<<directories.size()<<" directories"<<std::endl;
        return true;
    }
    catch(const std::exception& e) {
        g_logger<<LANG("error_scan")<<": "<<e.what()<<std::endl;
        return false;
    }
}
//FIXME: 每个 FileInfo 包含完整路径、哈希、大小等，在 DirectoryInfo 中重复存储，导致内存膨胀。
// 若文件数量巨大，可能成为瓶颈。
void FileScanner::ScanDirectory(const std::filesystem::path& currentPath,const std::string& relativePath) {
    DirectoryInfo dirInfo;
    dirInfo.path=relativePath;

    try {
#ifdef _WIN32
        // Windows下使用宽字符处理目录遍历
        std::wstring wCurrentPath=Utf8ToWide(currentPath.string());
        for(const auto& entry:std::filesystem::directory_iterator(wCurrentPath)) {
            std::string entryName=WideToUtf8(entry.path().filename().wstring());
#else
        for(const auto& entry:std::filesystem::directory_iterator(currentPath)) {
            std::string entryName=entry.path().filename().string();
#endif

            std::string entryRelativePath=relativePath.empty()?
                entryName:
                relativePath+"/"+entryName;

            entryRelativePath=NormalizePath(entryRelativePath);

            if(entry.is_directory()) {
                ScanDirectory(entry.path(),entryRelativePath);
                dirInfo.subdirectories.push_back(entryName);
            }
            else if(entry.is_regular_file()) {
                FileInfo fileInfo;
                fileInfo.path=entryRelativePath;
                fileInfo.size=entry.file_size();
                fileInfo.isDirectory=false;

                try {
                    auto ftime=entry.last_write_time();
                    auto sctp=std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime-std::filesystem::file_time_type::clock::now()+
                        std::chrono::system_clock::now());
                    fileInfo.modifiedTime=std::chrono::system_clock::to_time_t(sctp);
                }
                catch(const std::exception& e) {
                    g_logger<<LANG("error_time_conversion")<<": "<<e.what()<<std::endl;
                    fileInfo.modifiedTime=0;
                }

                // 计算文件哈希（使用完整路径）
                std::string fullPath=currentPath.string()+"/"+entryName;
				//FIXME: 若 CalculateFileHash 返回空字符串（如文件无法打开），fileInfo.hash 为空，该文件仍被加入列表。
                // 后续差异计算可能误认为该文件内容为空哈希，导致错误。
                fileInfo.hash=CalculateFileHash(fullPath,hashAlgorithm);

                files.push_back(fileInfo);
                dirInfo.files.push_back(fileInfo);
            }
        }

        if(!dirInfo.path.empty()||(!dirInfo.files.empty()||!dirInfo.subdirectories.empty())) {
            directories.push_back(dirInfo);
        }
        }
    catch(const std::filesystem::filesystem_error& e) {
        g_logger<<LANG("error_scan")<<": "<<e.what()<<std::endl;
    }
    }

std::string FileScanner::CalculateFileHash(const std::string& filePath,const std::string& algorithm) {
#ifdef _WIN32
    // Windows下使用宽字符API确保中文路径正确
    std::wstring wFilePath=Utf8ToWide(filePath);
    FILE* file=_wfopen(wFilePath.c_str(),L"rb");
#else
    FILE* file=fopen(filePath.c_str(),"rb");
#endif

    if(!file) {
        return "";
    }

    const size_t bufferSize=8192;
    char buffer[bufferSize];

    if(algorithm=="md5") {
        MD5_CTX context;
        MD5_Init(&context);
		//FIXME: fread 可能因错误返回 0，此时 ferror(file) 为真，但代码未检查，导致哈希基于部分数据计算，结果错误。
        size_t bytesRead;
        while((bytesRead=fread(buffer,1,bufferSize,file))>0) {
            MD5_Update(&context,buffer,bytesRead);
        }

        unsigned char digest[MD5_DIGEST_LENGTH];
        MD5_Final(digest,&context);

        std::stringstream ss;
        for(int i=0; i<MD5_DIGEST_LENGTH; ++i) {
            ss<<std::hex<<std::setw(2)<<std::setfill('0')<<(int)digest[i];
        }

        fclose(file);
        return ss.str();
    }
    else if(algorithm=="sha1") {
        SHA_CTX context;
        SHA1_Init(&context);

        size_t bytesRead;
        while((bytesRead=fread(buffer,1,bufferSize,file))>0) {
            SHA1_Update(&context,buffer,bytesRead);
        }

        unsigned char digest[SHA_DIGEST_LENGTH];
        SHA1_Final(digest,&context);

        std::stringstream ss;
        for(int i=0; i<SHA_DIGEST_LENGTH; ++i) {
            ss<<std::hex<<std::setw(2)<<std::setfill('0')<<(int)digest[i];
        }

        fclose(file);
        return ss.str();
    }
    else if(algorithm=="sha256") {
        SHA256_CTX context;
        SHA256_Init(&context);

        size_t bytesRead;
        while((bytesRead=fread(buffer,1,bufferSize,file))>0) {
            SHA256_Update(&context,buffer,bytesRead);
        }

        // 特别处理空文件
		//OPTIMIZE: 空文件处理可优化,不必要的初始化。
        if(ftell(file)==0) {
            // 空文件的SHA256是固定的
            static const std::string emptyFileSha256=
                "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
            fclose(file);
            return emptyFileSha256;
        }

        unsigned char digest[SHA256_DIGEST_LENGTH];
        SHA256_Final(digest,&context);

        std::stringstream ss;
        for(int i=0; i<SHA256_DIGEST_LENGTH; ++i) {
            ss<<std::hex<<std::setw(2)<<std::setfill('0')<<(int)digest[i];
        }

        fclose(file);
        return ss.str();
    }

    fclose(file);
    return "";
}

std::string FileScanner::NormalizePath(const std::string& path) const {
    std::string normalized=path;
#ifdef _WIN32
    std::replace(normalized.begin(),normalized.end(),'\\','/');
#endif
    return normalized;
}

bool FileScanner::LoadFromJson(const Json::Value& json) {
    files.clear();
    directories.clear();

    if(!json.isMember("files")||!json.isMember("directories")) {
        return false;
    }

    // 加载文件
	//FIXME: 这里没有校验 JSON 结构的完整性和正确性，譬如缺失字段或类型错误可能导致异常或错误
    const Json::Value& filesJson=json["files"];
    for(const auto& fileJson:filesJson) {
        FileInfo file;
        file.path=fileJson["path"].asString();
        file.hash=fileJson["hash"].asString();
        file.size=fileJson["size"].asUInt64();
        file.isDirectory=fileJson["is_directory"].asBool();
        file.modifiedTime=fileJson["modified_time"].asUInt64();
        files.push_back(file);
    }

    // 加载目录
    const Json::Value& dirsJson=json["directories"];
    for(const auto& dirJson:dirsJson) {
        DirectoryInfo dir;
        dir.path=dirJson["path"].asString();

        const Json::Value& dirFilesJson=dirJson["files"];
        for(const auto& fileJson:dirFilesJson) {
            FileInfo file;
            file.path=fileJson["path"].asString();
            file.hash=fileJson["hash"].asString();
            file.size=fileJson["size"].asUInt64();
            file.isDirectory=false;
            file.modifiedTime=fileJson["modified_time"].asUInt64();
            dir.files.push_back(file);
        }

        const Json::Value& subdirsJson=dirJson["subdirectories"];
        for(const auto& subdirJson:subdirsJson) {
            dir.subdirectories.push_back(subdirJson.asString());
        }

        directories.push_back(dir);
    }

    return true;
}

Json::Value FileScanner::ToJson() const {
    Json::Value json;

    Json::Value filesJson(Json::arrayValue);
    for(const auto& file:files) {
        filesJson.append(file.ToJson());
    }
    json["files"]=filesJson;

    Json::Value dirsJson(Json::arrayValue);
    for(const auto& dir:directories) {
        dirsJson.append(dir.ToJson());
    }
    json["directories"]=dirsJson;

    return json;
}