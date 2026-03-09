#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <string>
#include <memory>
#include "crow.h"
#include "Config.h"
#include "VersionManager.h"
#include "FileScanner.h"

class WebServer {
public:
    WebServer(const Config& config,
        VersionManager& versionManager,
        const std::string& workspace);

    bool Start();
    void Stop();

    // API端点
    crow::response HandleUpdateInfo(const crow::request& req);
    crow::response HandleFileDownload(const crow::request& req,const std::string& filepath);
    crow::response HandlePackageDownload(const crow::request& req,const std::string& package);
    crow::response HandleVersionList(const crow::request& req);
    crow::response HandleStatus(const crow::request& req);

private:
    Config config;
    VersionManager& versionManager;
    std::string workspace;
    std::unique_ptr<crow::SimpleApp> app;

    void SetupRoutes();

    // 生成更新信息JSON
    Json::Value GenerateUpdateInfo(const std::string& version) const;

    // 检查文件存在性
    bool FileExists(const std::string& filepath) const;

    // 获取文件MIME类型
    std::string GetMimeType(const std::string& filename) const;
};

#endif