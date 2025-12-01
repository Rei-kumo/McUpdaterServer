#include "ConfigManager.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <json/json.h>

namespace fs=std::filesystem;

ConfigManager::ConfigManager(const std::string& configPath):configPath(configPath),configLoaded(false) {
    if(configPath.empty()) {
        g_logger<<"[ERROR]配置文件路径为空!"<<std::endl;
        return;
    }

    if(!EnsureConfigDirectory()) {
        g_logger<<"[ERROR]无法创建配置目录"<<std::endl;
        return;
    }

    LoadConfig();
}

ConfigManager::~ConfigManager() {
    cachedConfig.clear();
    configLoaded=false;
}

bool ConfigManager::LoadConfig() {
    if(configPath.empty()) {
        g_logger<<"[ERROR]配置文件路径为空"<<std::endl;
        return false;
    }

    std::ifstream file(configPath);
    if(!file.is_open()) {
        g_logger<<"[WARN]无法打开配置文件: "<<configPath<<std::endl;
        return false;
    }

    Json::Reader reader;
    std::string errors;

    if(!reader.parse(file,cachedConfig)) {
        errors=reader.getFormattedErrorMessages();
        g_logger<<"[ERROR]配置解析错误: "<<errors<<std::endl;
        file.close();
        return false;
    }

    file.close();
    configLoaded=true;
    return true;
}

Json::Value ConfigManager::ReadConfig(){
    if(configLoaded) {
        return cachedConfig;
    }

    if(LoadConfig()) {
        return cachedConfig;
    }

    return Json::Value();
}

std::string ConfigManager::ReadVersion(){
    Json::Value config=ReadConfig();
    if(config.isMember("version")){
        return config["version"].asString();
    }
    return "1.0.0";
}

std::string ConfigManager::ReadUpdateUrl(){
    Json::Value config=ReadConfig();
    if(config.isMember("update_url")){
        return config["update_url"].asString();
    }
    return "";
}

std::string ConfigManager::ReadGameDirectory(){
    Json::Value config=ReadConfig();
    if(config.isMember("game_directory")){
        return config["game_directory"].asString();
    }
    return "./.minecraft";
}

bool ConfigManager::ReadAutoUpdate(){
    Json::Value config=ReadConfig();
    if(config.isMember("auto_update")){
        return config["auto_update"].asBool();
    }
    return true;
}

std::string ConfigManager::ReadLogFile(){
    Json::Value config=ReadConfig();
    if(config.isMember("log_file")){
        return config["log_file"].asString();
    }
    return "./logs/updater.log";
}

std::string ConfigManager::ReadUpdateMode() {
    Json::Value config=ReadConfig();
    if(config.isMember("update_mode")) {
        return config["update_mode"].asString();
    }
    return "hash";
}

std::string ConfigManager::ReadHashAlgorithm() {
    Json::Value config=ReadConfig();
    if(config.isMember("hash_algorithm")) {
        return config["hash_algorithm"].asString();
    }
    return "md5";
}

bool ConfigManager::ReadEnableFileDeletion() {
    Json::Value config=ReadConfig();
    if(config.isMember("enable_file_deletion")) {
        return config["enable_file_deletion"].asBool();
    }
    return true;
}

bool ConfigManager::ReadSkipMajorVersionCheck() {
    Json::Value config=ReadConfig();
    if(config.isMember("skip_major_version_check")) {
        return config["skip_major_version_check"].asBool();
    }
    return false;
}

bool ConfigManager::WriteVersion(const std::string& version){
    Json::Value config=ReadConfig();
    config["version"]=version;
    return WriteConfig(config);
}

bool ConfigManager::WriteUpdateUrl(const std::string& url){
    Json::Value config=ReadConfig();
    config["update_url"]=url;
    return WriteConfig(config);
}

bool ConfigManager::WriteGameDirectory(const std::string& dir){
    Json::Value config=ReadConfig();
    config["game_directory"]=dir;
    return WriteConfig(config);
}

bool ConfigManager::WriteAutoUpdate(bool autoUpdate){
    Json::Value config=ReadConfig();
    config["auto_update"]=autoUpdate;
    return WriteConfig(config);
}

bool ConfigManager::WriteLogFile(const std::string& logPath){
    Json::Value config=ReadConfig();
    config["log_file"]=logPath;
    return WriteConfig(config);
}

bool ConfigManager::WriteUpdateMode(const std::string& mode) {
    Json::Value config=ReadConfig();
    config["update_mode"]=mode;
    return WriteConfig(config);
}

bool ConfigManager::WriteHashAlgorithm(const std::string& algorithm) {
    Json::Value config=ReadConfig();
    config["hash_algorithm"]=algorithm;
    return WriteConfig(config);
}

bool ConfigManager::WriteEnableFileDeletion(bool enable) {
    Json::Value config=ReadConfig();
    config["enable_file_deletion"]=enable;
    return WriteConfig(config);
}

bool ConfigManager::WriteSkipMajorVersionCheck(bool skip) {
    Json::Value config=ReadConfig();
    config["skip_major_version_check"]=skip;
    return WriteConfig(config);
}

bool ConfigManager::ConfigExists(){
    return fs::exists(configPath);
}

bool ConfigManager::InitializeDefaultConfig(){
    if(!EnsureConfigDirectory()){
        g_logger<<"[ERROR]无法创建配置目录"<<std::endl;
        return false;
    }

    Json::Value defaultConfig=CreateDefaultConfig();
    bool result=WriteConfig(defaultConfig);
    if(result){
        g_logger<<"[INFO]已创建默认配置文件:"<<configPath<<std::endl;
    }
    else{
        g_logger<<"[ERROR]创建默认配置文件失败"<<std::endl;
    }
    return result;
}

Json::Value ConfigManager::CreateDefaultConfig(){
    Json::Value config;
    config["version"]="1.0.0";
    config["file_base_url"]="http://play.reikumo.cn:24567/";
    config["update_mode"]="hash";
    config["hash_algorithm"]="md5";
    config["enable_hash_cache"]=true;
    config["cache_dir"]="./cache";
    config["cache_expiry_hours"]=24;

    Json::Value changelog(Json::arrayValue);
    changelog.append("Fixed game crash issues");
    changelog.append("Added new biomes");
    changelog.append("Optimized performance");
    config["changelog"]=changelog;

    return config;
}

bool ConfigManager::WriteConfig(const Json::Value& config){
    if(!EnsureConfigDirectory()){
        return false;
    }

    if(configPath.empty()) {
        g_logger<<"[ERROR]配置文件路径为空"<<std::endl;
        return false;
    }

    std::ofstream file(configPath);
    if(!file.is_open()){
        g_logger<<"[ERROR]无法打开配置文件进行写入: "<<configPath<<std::endl;
        return false;
    }
    Json::StyledWriter writer;
    std::string jsonString=writer.write(config);
    file<<jsonString;
    file.close();

    cachedConfig=config;
    configLoaded=true;

    return true;
}

bool ConfigManager::EnsureConfigDirectory(){
    fs::path path(configPath);
    fs::path dir=path.parent_path();

    if(!dir.empty()&&!fs::exists(dir)){
        return fs::create_directories(dir);
    }

    return true;
}

std::string ConfigManager::ReadFileBaseUrl() {
    Json::Value config=ReadConfig();
    if(config.isMember("file_base_url")) {
        return config["file_base_url"].asString();
    }
    return "http://your-cilet-server:0721/";
}

bool ConfigManager::WriteFileBaseUrl(const std::string& url) {
    Json::Value config=ReadConfig();
    config["file_base_url"]=url;
    return WriteConfig(config);
}

Json::Value ConfigManager::ReadChangelog() {
    Json::Value config=ReadConfig();
    if(config.isMember("changelog")) {
        return config["changelog"];
    }
    return Json::Value(Json::arrayValue);
}

bool ConfigManager::WriteChangelog(const Json::Value& changelog) {
    Json::Value config=ReadConfig();
    config["changelog"]=changelog;
    return WriteConfig(config);
}

bool ConfigManager::ReadEnableHashCache() {
    Json::Value config=ReadConfig();
    if(config.isMember("enable_hash_cache")) {
        return config["enable_hash_cache"].asBool();
    }
    return true;
}

bool ConfigManager::WriteEnableHashCache(bool enable) {
    Json::Value config=ReadConfig();
    config["enable_hash_cache"]=enable;
    return WriteConfig(config);
}

std::string ConfigManager::ReadCacheDir() {
    Json::Value config=ReadConfig();
    if(config.isMember("cache_dir")) {
        return config["cache_dir"].asString();
    }
    return "./cache";
}

bool ConfigManager::WriteCacheDir(const std::string& dir) {
    Json::Value config=ReadConfig();
    config["cache_dir"]=dir;
    return WriteConfig(config);
}

int ConfigManager::ReadCacheExpiryHours() {
    Json::Value config=ReadConfig();
    if(config.isMember("cache_expiry_hours")) {
        return config["cache_expiry_hours"].asInt();
    }
    return 24;
}

bool ConfigManager::WriteCacheExpiryHours(int hours) {
    Json::Value config=ReadConfig();
    config["cache_expiry_hours"]=hours;
    return WriteConfig(config);
}