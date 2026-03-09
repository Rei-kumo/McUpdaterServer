#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <filesystem>
#include <json/json.h>

class Config {
public:
    Config(const std::string& configPath);
    ~Config()=default;

    bool Load();
    bool Save();
    bool CreateDirectories();

    // 旧接口，目前没用
    //bool Validate() const;

    // 获取配置值
    std::string GetWorkspace() const { return workspace; }
    std::string GetOutputDir() const { return outputDir; }
    std::string GetServerHost() const { return serverHost; }
    int GetServerPort() const { return serverPort; }
    std::string GetBaseUrl() const { return baseUrl; }
    std::string GetHashAlgorithm() const { return hashAlgorithm; }
    bool GetEnableWebServer() const { return enableWebServer; }
    bool GetEnableIncremental() const { return enableIncremental; }
    int GetMaxPackageVersions() const { return maxPackageVersions; }
    std::string GetLogFile() const { return logFile; }
    std::string GetLanguage() const { return language; }

    // 设置配置值
    void SetOutputDir(const std::string& dir) { outputDir=dir; }
    void SetServerHost(const std::string& host) { serverHost=host; }
    void SetServerPort(int port) { serverPort=port; }
    void SetBaseUrl(const std::string& url) { baseUrl=url; }

    // 获取默认配置
    static Json::Value GetDefaultConfig();

private:
    std::string configPath;

    // 配置项
    std::string workspace="public";  // 固定为public目录
    std::string outputDir="./updates";
    std::string serverHost="127.0.0.1";
    int serverPort=8080;
    std::string baseUrl="http://127.0.0.1:8080";
    std::string hashAlgorithm="sha256";
    bool enableWebServer=true;
    bool enableIncremental=true;
    int maxPackageVersions=10;
    std::string logFile="./logs/server.log";
    std::string language="zh_CN";

    Json::Value jsonConfig;
};

#endif