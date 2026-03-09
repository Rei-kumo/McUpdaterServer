#include "WebServer.h"
#include "Language.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "Logger.h"
#include <windows.h>
#include <cctype>
/*
未处理 + 号：应解码为空格，但当前未做。

十六进制解析失败：遇到非法 % 序列（如 %G0）时静默保留原样，可能产生错误路径。

UTF-8 多字节字符：转换方式没问题，但需确保后续按 UTF-8 处理。

编码范围错误：UrlEncode 将非字母数字全部转义，包括 / 等路径分隔符（违反 RFC 3986），导致路径结构丢失（如 /files/ 后的 / 被转成 %2F），服务器无法正确解析。
*/

static std::string UrlDecode(const std::string& encoded) {
    std::string res;
    for(size_t i=0; i<encoded.length(); ++i) {
        if(encoded[i]=='%'&&i+2<encoded.length()) {
            int value;
            std::istringstream iss(encoded.substr(i+1,2));
            if(iss>>std::hex>>value) {
                res+=static_cast<char>(value);
                i+=2;
            }
            else {
                res+='%';
            }
        }
        else {
            res+=encoded[i];
        }
    }
    return res;
}
static std::string UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped<<std::hex;

    for(unsigned char c:value) {
        // 保留字母数字和部分安全字符
        if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~') {
            escaped<<c;
        }
        else {
            escaped<<'%'<<std::setw(2)<<int((unsigned char)c);
        }
    }
    return escaped.str();
}

static std::wstring Utf8ToWide(const std::string& utf8) {
    if(utf8.empty()) return L"";
    int wlen=MultiByteToWideChar(CP_UTF8,0,utf8.c_str(),-1,nullptr,0);
    if(wlen<=0) return L"";
    std::wstring wstr(wlen-1,0);
    MultiByteToWideChar(CP_UTF8,0,utf8.c_str(),-1,&wstr[0],wlen);
    return wstr;
}

WebServer::WebServer(const Config& config,
    VersionManager& versionManager,
    const std::string& workspace)
    : config(config),versionManager(versionManager),workspace(workspace) {
    app=std::make_unique<crow::SimpleApp>();
}

bool WebServer::Start() {
    SetupRoutes();

    g_logger<<LANG("server_start_at")
        <<config.GetServerHost()<<":"
        <<config.GetServerPort()<<std::endl;

    try {
        app->bindaddr(config.GetServerHost())
            .port(config.GetServerPort())
            .multithreaded()
            .run();
        return true;
    }
    catch(const std::exception& e) {
        g_logger<<LANG("error_server")<<": "<<e.what()<<std::endl;
        return false;
    }
}

void WebServer::Stop() {
    g_logger<<LANG("server_stop")<<std::endl;
    // Crow没有正式的stop方法，可以通过其他方式停止
}

void WebServer::SetupRoutes() {
    CROW_ROUTE((*app),"/api/update")
        .methods("GET"_method)([this](const crow::request& req) {
        return this->HandleUpdateInfo(req);
            });

    CROW_ROUTE((*app),"/api/versions")
        .methods("GET"_method)([this](const crow::request& req) {
        return this->HandleVersionList(req);
            });

    CROW_ROUTE((*app),"/api/status")
        .methods("GET"_method)([this](const crow::request& req) {
        return this->HandleStatus(req);
            });

    CROW_ROUTE((*app),"/files/<string>")
        .methods("GET"_method)([this](const crow::request& req,const std::string& filepath) {
        return this->HandleFileDownload(req,filepath);
            });

    CROW_ROUTE((*app),"/packages/<string>")
        .methods("GET"_method)([this](const crow::request& req,const std::string& package) {
        return this->HandlePackageDownload(req,package);
            });

    // 静态文件服务
    CROW_ROUTE((*app),"/")
        ([]() {
        return crow::response(200,"Minecraft Updater Server v1.0.0");
            });
}

crow::response WebServer::HandleUpdateInfo(const crow::request& req) {
    g_logger<<LANG("request_update")<<std::endl;

    // 获取目标版本
    std::string version="latest";
    auto versionParam=req.url_params.get("version");
    if(versionParam) {
        version=versionParam;
    }

    // 如果请求最新版本，获取最新的版本号
    //FIXME:若指定版本不存在，应返回 404。当前代码未处理指定版本不存在的情况
    if(version=="latest") {
        auto versions=versionManager.GetVersionList();
        if(!versions.empty()) {
            version=versions.back();
        }
        else {
            crow::response res(404);
            res.write(LANG("error_no_versions"));
            return res;
        }
    }

    // 生成更新信息
    Json::Value updateInfo=GenerateUpdateInfo(version);

    crow::response res;
    res.set_header("Content-Type","application/json");
    res.write(Json::FastWriter().write(updateInfo));
    return res;
}
//FIXME:攻击者可构造 ../ 跳出工作空间目录，访问系统任意文件。例如 /files/../../etc/passwd。crow 不会自动清理路径参数..
crow::response WebServer::HandleFileDownload(const crow::request& req,const std::string& filepath) {
    // 先进行 URL 解码
    std::string decodedPath=UrlDecode(filepath);
    g_logger<<LANG("request_download")<<": "<<decodedPath<<std::endl;

    std::string fullPath=workspace+"/"+decodedPath;
    g_logger<<"[DEBUG] FullPath: "<<fullPath<<std::endl;

#ifdef _WIN32
    std::wstring wFullPath=Utf8ToWide(fullPath);
    FILE* file=_wfopen(wFullPath.c_str(),L"rb");
#else
    FILE* file=fopen(fullPath.c_str(),"rb");
#endif

    if(!file) {
        g_logger<<"[ERROR] Failed to open file: "<<fullPath<<std::endl;
        crow::response res(404);
        res.write(LANG("info_file_not_found_simple")+": "+decodedPath);
        return res;
    }

    // 获取文件大小
    fseek(file,0,SEEK_END);
    long fileSize=ftell(file);
    fseek(file,0,SEEK_SET);

    // 读取文件内容
    //FIXME:未处理大文件内存问题
    std::vector<char> buffer(fileSize);
    size_t bytesRead=fread(buffer.data(),1,fileSize,file);
    fclose(file);

    if(bytesRead!=static_cast<size_t>(fileSize)) {
        crow::response res(500);
        res.write(LANG("error_read_file")+": "+decodedPath);
        return res;
    }

    crow::response res;
    res.set_header("Content-Type",GetMimeType(decodedPath));
    res.set_header("Content-Disposition","attachment; filename=\""+
        std::filesystem::path(decodedPath).filename().string()+"\"");
    res.write(std::string(buffer.data(),buffer.size()));
    return res;
}

crow::response WebServer::HandlePackageDownload(const crow::request& req,const std::string& package) {
    // 1. URL 解码
    std::string decodedPackage=UrlDecode(package);
    g_logger<<LANG("request_package")<<": "<<decodedPackage<<std::endl;

    std::string fullPath;

    // 2. 根据包名判断子目录
    if(decodedPackage.find("_to_")!=std::string::npos) {
        // 增量包
        fullPath=config.GetOutputDir()+"/incremental/"+decodedPackage;
    }
    else if(decodedPackage=="root.zip"||decodedPackage.find(".zip")!=std::string::npos) {
        // 可能是全量包（版本号.zip）或目录包（目录名.zip）
        // 先尝试 full 目录
        fullPath=config.GetOutputDir()+"/full/"+decodedPackage;
        // 如果不存在，再尝试 packages 目录（目录包）
        // 直接使用文件打开尝试，不依赖 FileExists
    }
    else {
        crow::response res(400);
        res.write("Invalid package name");
        return res;
    }

    // 3. 尝试打开文件（先尝试 full 路径，若失败再试 packages 路径）
    FILE* file=nullptr;
#ifdef _WIN32
    std::wstring wFullPath=Utf8ToWide(fullPath);
    file=_wfopen(wFullPath.c_str(),L"rb");
#else
    file=fopen(fullPath.c_str(),"rb");
#endif

    // 如果第一次打开失败，且包名可能是目录包，则尝试 packages 目录
    /*
    路径遍历漏洞：未对用户输入进行充分校验，可能允许恶意路径访问（如 ../）。

    回退逻辑缺陷：当 /packages/full/1.0.0.zip 不存在时，错误地回退到 /packages/1.0.0.zip，可能返回同名的目录包（如 /packages/dir/1.0.0.zip），导致客户端下载错误内容。

    */
    if(!file&&decodedPackage.find("_to_")==std::string::npos&&decodedPackage!="root.zip") {
        // 尝试 packages 目录
        std::string altPath=config.GetOutputDir()+"/packages/"+decodedPackage;
#ifdef _WIN32
        std::wstring wAltPath=Utf8ToWide(altPath);
        file=_wfopen(wAltPath.c_str(),L"rb");
#else
        file=fopen(altPath.c_str(),"rb");
#endif
        if(file) fullPath=altPath;
    }

    if(!file) {
        g_logger<<"[ERROR] Failed to open package: "<<decodedPackage<<std::endl;
        crow::response res(404);
        res.write(LANG("info_file_not_found_simple")+": "+decodedPackage);
        return res;
    }

    // 4. 读取文件内容
    fseek(file,0,SEEK_END);
    long fileSize=ftell(file);
    fseek(file,0,SEEK_SET);
    std::vector<char> buffer(fileSize);
    size_t bytesRead=fread(buffer.data(),1,fileSize,file);
    fclose(file);

    if(bytesRead!=static_cast<size_t>(fileSize)) {
        crow::response res(500);
        res.write(LANG("error_read_file")+": "+decodedPackage);
        return res;
    }

    // 5. 返回响应
    crow::response res;
    res.set_header("Content-Type","application/zip");
    res.set_header("Content-Disposition","attachment; filename=\""+decodedPackage+"\"");
    res.write(std::string(buffer.data(),buffer.size()));
    return res;
}

crow::response WebServer::HandleVersionList(const crow::request& req) {
    auto versions=versionManager.GetVersionList();

    Json::Value json;
    Json::Value versionsArray(Json::arrayValue);
    for(const auto& version:versions) {
        versionsArray.append(version);
    }
    json["versions"]=versionsArray;

    crow::response res;
    res.set_header("Content-Type","application/json");
    res.write(Json::FastWriter().write(json));
    return res;
}

crow::response WebServer::HandleStatus(const crow::request& req) {
    Json::Value json;
    json["status"]=LANG("info_running");
    json["workspace"]=workspace;
    json["output_dir"]=config.GetOutputDir();
    json["versions_count"]=(int)versionManager.GetVersionList().size();

    crow::response res;
    res.set_header("Content-Type","application/json");
    res.write(Json::FastWriter().write(json));
    return res;
}

Json::Value WebServer::GenerateUpdateInfo(const std::string& version) const {
    const VersionInfo* versionInfo=versionManager.GetVersion(version);
    if(!versionInfo) {
        return Json::Value();
    }

    // 加载版本快照
    std::string snapshotFile=config.GetOutputDir()+"/snapshots/"+version+".json";
    std::ifstream snapshotStream(snapshotFile);
    Json::Value snapshot;
    if(snapshotStream.is_open()) {
        Json::CharReaderBuilder reader;
        std::string errors;
        Json::parseFromStream(reader,snapshotStream,&snapshot,&errors);
    }

    Json::Value updateInfo;
    updateInfo["version"]=versionInfo->version;
    updateInfo["update_mode"]="hash";

    // 文件列表
    Json::Value filesArray(Json::arrayValue);
    if(snapshot.isMember("files")) {
        for(const auto& fileJson:snapshot["files"]) {
            if(!fileJson["is_directory"].asBool()) {
                Json::Value fileInfo;
                fileInfo["path"]=fileJson["path"];
                fileInfo["hash"]=fileJson["hash"];
                std::string encodedPath=UrlEncode(fileJson["path"].asString());
                fileInfo["url"]=config.GetBaseUrl()+"/files/"+encodedPath;
                fileInfo["size"]=fileJson["size"];
                filesArray.append(fileInfo);
            }
        }
    }
    updateInfo["files"]=filesArray;

    // 目录列表
    Json::Value dirsArray(Json::arrayValue);
    if(snapshot.isMember("directories")) {
        for(const auto& dirJson:snapshot["directories"]) {
            Json::Value dirInfo;
            dirInfo["path"]=dirJson["path"];

            // 检查空目录
            bool isEmpty=true;
            if(dirJson.isMember("files")) {
                for(const auto& fileJson:dirJson["files"]) {
                    if(!fileJson["is_directory"].asBool()) {
                        isEmpty=false;
                        break;
                    }
                }
            }
            if(isEmpty&&dirJson.isMember("subdirectories")&&!dirJson["subdirectories"].empty()) {
                isEmpty=false;
            }
            dirInfo["is_empty"]=isEmpty;

            // 目录包（位于 packages/ 下）
            std::string packageName=dirJson["path"].asString().empty()?"root.zip":dirJson["path"].asString()+".zip";
            std::string packagePath=config.GetOutputDir()+"/packages/"+packageName;
            if(std::filesystem::exists(packagePath)) {
                std::string encodedPackageName=UrlEncode(packageName);
                dirInfo["url"]=config.GetBaseUrl()+"/packages/"+encodedPackageName;
            }

            // 目录内容
            Json::Value contentsArray(Json::arrayValue);
            if(dirJson.isMember("files")) {
                for(const auto& fileJson:dirJson["files"]) {
                    if(!fileJson["is_directory"].asBool()) {
                        Json::Value content;
                        content["path"]=fileJson["path"];
                        content["hash"]=fileJson["hash"];
                        contentsArray.append(content);
                    }
                }
            }
            dirInfo["contents"]=contentsArray;
            dirsArray.append(dirInfo);
        }
    }
    updateInfo["directories"]=dirsArray;

    // 增量包列表（位于 incremental/ 下）
    Json::Value incrementalArray(Json::arrayValue);
    auto versions=versionManager.GetVersionList();
    auto currentIt=std::find(versions.begin(),versions.end(),version);
    if(currentIt!=versions.end()) {
        for(auto it=versions.begin(); it!=currentIt; ++it) {
            std::string fromVersion=*it;
            std::string packageName=fromVersion+"_to_"+version+".zip";
            std::string packagePath=config.GetOutputDir()+"/incremental/"+packageName;
            if(std::filesystem::exists(packagePath)) {
                Json::Value packageInfo;
                packageInfo["from_version"]=fromVersion;
                packageInfo["to_version"]=version;
                packageInfo["hash"]=FileScanner::CalculateFileHash(packagePath,"md5");
                packageInfo["archive"]=config.GetBaseUrl()+"/packages/"+packageName; // 注意：URL 仍使用 /packages/ 前缀
                packageInfo["manifest"]="update_manifest.txt";
                incrementalArray.append(packageInfo);
            }
        }
    }
    updateInfo["incremental_packages"]=incrementalArray;

    // 全量包信息（位于 full/ 下）
    std::string fullPackageName=version+".zip";
    std::string fullPackagePath=config.GetOutputDir()+"/full/"+fullPackageName;
    if(std::filesystem::exists(fullPackagePath)) {
        Json::Value fullPackageInfo;
        fullPackageInfo["version"]=version;
        fullPackageInfo["hash"]=FileScanner::CalculateFileHash(fullPackagePath,"md5");
        fullPackageInfo["archive"]=config.GetBaseUrl()+"/packages/"+fullPackageName; // URL 仍使用 /packages/
        fullPackageInfo["manifest"]="update_manifest.txt";
        updateInfo["full_package"]=fullPackageInfo;
    }

    // 包哈希映射
    Json::Value packageHashes;
    for(const auto& package:incrementalArray) {
        std::string url=package["archive"].asString();
        std::string hash=package["hash"].asString();
        packageHashes[url]=hash;
    }
    updateInfo["package_hashes"]=packageHashes;

    // 启动器信息（可配置）
    Json::Value launcher;
    launcher["version"]="0.0.8";
    launcher["url"]="https://server.com/launcher/McUpdaterClient.exe";
    launcher["hash"]="sha256:abcd1234...";
    updateInfo["launcher"]=launcher;

    // 更新日志
    Json::Value changelogArray(Json::arrayValue);
    for(const auto& ver:versions) {
        if(ver<=version) {
            changelogArray.append("版本 "+ver+": 更新");
        }
    }
    updateInfo["changelog"]=changelogArray;

    return updateInfo;
}

bool WebServer::FileExists(const std::string& filepath) const {
    return std::filesystem::exists(filepath);
}

std::string WebServer::GetMimeType(const std::string& filename) const {
    std::filesystem::path path(filename);
    std::string ext=path.extension().string();

    if(ext==".txt") return "text/plain";
    if(ext==".json") return "application/json";
    if(ext==".zip") return "application/zip";
    if(ext==".exe") return "application/octet-stream";
    if(ext==".jar") return "application/java-archive";
    if(ext==".png") return "image/png";
    if(ext==".jpg"||ext==".jpeg") return "image/jpeg";
    if(ext==".properties") return "text/plain";
    if(ext==".yml"||ext==".yaml") return "text/yaml";

    return "application/octet-stream";
}