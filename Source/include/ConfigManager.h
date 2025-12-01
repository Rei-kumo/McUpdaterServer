#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <string>
#include <fstream>
#include <filesystem>
#include "json/json.h"  // 修改包含路径
#include "Logger.h"

class ConfigManager {
private:
    std::string configPath;
    Json::Value cachedConfig;
    bool configLoaded;

public:
    ConfigManager(const std::string& configPath);
    ~ConfigManager();

    bool InitializeDefaultConfig();
    bool ConfigExists();

    std::string ReadVersion();
    bool WriteVersion(const std::string& version);
    std::string ReadUpdateUrl();
    bool WriteUpdateUrl(const std::string& url);
    std::string ReadGameDirectory();
    bool WriteGameDirectory(const std::string& dir);
    bool ReadAutoUpdate();
    bool WriteAutoUpdate(bool autoUpdate);
    std::string ReadLogFile();
    bool WriteLogFile(const std::string& logPath);

    Json::Value ReadConfig();
    bool WriteConfig(const Json::Value& config);

    std::string ReadUpdateMode();
    bool WriteUpdateMode(const std::string& mode);
    std::string ReadHashAlgorithm();
    bool WriteHashAlgorithm(const std::string& algorithm);
    bool ReadEnableFileDeletion();
    bool WriteEnableFileDeletion(bool enable);
    bool ReadSkipMajorVersionCheck();
    bool WriteSkipMajorVersionCheck(bool skip);
    bool ReadEnableApiCache();
    bool WriteEnableApiCache(bool enable);
    int ReadApiTimeout();
    bool WriteApiTimeout(int timeout);

    std::string ReadFileBaseUrl();
    bool WriteFileBaseUrl(const std::string& url);
    Json::Value ReadChangelog();
    bool WriteChangelog(const Json::Value& changelog);

    // 添加缺失的方法声明
    bool ReadEnableHashCache();
    bool WriteEnableHashCache(bool enable);
    std::string ReadCacheDir();
    bool WriteCacheDir(const std::string& dir);
    int ReadCacheExpiryHours();
    bool WriteCacheExpiryHours(int hours);

private:
    bool EnsureConfigDirectory();
    Json::Value CreateDefaultConfig();
    bool LoadConfig();
};

#endif