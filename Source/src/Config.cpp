#include "Config.h"
#include "Language.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include "Logger.h"

Config::Config(const std::string& configPath)
    : configPath(configPath) {
}

bool Config::Load() {
    std::ifstream file(configPath);
    if(!file.is_open()) {
        return false;
    }

    Json::CharReaderBuilder reader;
    std::string errors;

    if(!Json::parseFromStream(reader,file,&jsonConfig,&errors)) {
        g_logger<<LANG("error_config")<<errors<<std::endl;
        return false;
    }
	//Fixme: JSON 值类型未检查,如果配置文件中 server_port 是字符串（如 "8080"），asInt() 会返回默认值
    // 导致配置错误且难以排查。
    if(jsonConfig.isMember("output_dir"))
        outputDir=jsonConfig["output_dir"].asString();

    if(jsonConfig.isMember("server_host"))
        serverHost=jsonConfig["server_host"].asString();

    if(jsonConfig.isMember("server_port"))
        serverPort=jsonConfig["server_port"].asInt();

    if(jsonConfig.isMember("base_url"))
        baseUrl=jsonConfig["base_url"].asString();

    if(jsonConfig.isMember("hash_algorithm"))
        hashAlgorithm=jsonConfig["hash_algorithm"].asString();

    if(jsonConfig.isMember("enable_web_server"))
        enableWebServer=jsonConfig["enable_web_server"].asBool();

    if(jsonConfig.isMember("enable_incremental"))
        enableIncremental=jsonConfig["enable_incremental"].asBool();

    if(jsonConfig.isMember("max_package_versions"))
        maxPackageVersions=jsonConfig["max_package_versions"].asInt();

    if(jsonConfig.isMember("log_file"))
        logFile=jsonConfig["log_file"].asString();

    if(jsonConfig.isMember("language"))
        language=jsonConfig["language"].asString();

    Language::Instance().SetLanguage(language);

    return true;
}

bool Config::Save() {
    //Fixme:：配置文件中额外字段加载后 jsonConfig 会保留它们。
    // 但调用 Save() 后，jsonConfig 被完全替换为默认配置，再更新已知字段，导致所有未知字段永久丢失。
    jsonConfig=GetDefaultConfig();

    jsonConfig["output_dir"]=outputDir;
    jsonConfig["server_host"]=serverHost;
    jsonConfig["server_port"]=serverPort;
    jsonConfig["base_url"]=baseUrl;
    jsonConfig["hash_algorithm"]=hashAlgorithm;
    jsonConfig["enable_web_server"]=enableWebServer;
    jsonConfig["enable_incremental"]=enableIncremental;
    jsonConfig["max_package_versions"]=maxPackageVersions;
    jsonConfig["log_file"]=logFile;
    jsonConfig["language"]=language;

    // 初始化或者判断有没有人把配置给删除了，防止出奇奇怪怪的问题
    std::filesystem::path configFilePath(configPath);
    std::filesystem::create_directories(configFilePath.parent_path());

    std::ofstream file(configPath);
    if(!file.is_open()) {
        g_logger<<LANG("error_write_config")<<configPath<<std::endl;
        return false;
    }

    Json::StreamWriterBuilder writer;
    writer["indentation"]="  ";
    std::string jsonString=Json::writeString(writer,jsonConfig);
	//FIXME: 没有文件状态检测，无论如何都返回了true
    file<<jsonString;

    return true;
}

bool Config::CreateDirectories() {
    try {
		// 工作目录，我用作public，其实我想用workspace这个名字的，但我觉得public更形象一些，毕竟它就是用来放公开资源的
        if(!std::filesystem::exists(workspace)) {
            std::filesystem::create_directories(workspace);
            g_logger<<LANG("directory_created")<<workspace<<std::endl;
        }

		// 创建工作目录下各种各样的子目录
		//FIXME: 跨平台目录问题，未创建 full 和 incremental 目录，在其它模块实现的，代码混乱
        std::filesystem::create_directories(outputDir);
        std::filesystem::create_directories(outputDir+"/data");
        std::filesystem::create_directories(outputDir+"/snapshots");
        std::filesystem::create_directories(outputDir+"/packages");
        std::filesystem::path logPath(logFile);
        if(logPath.has_parent_path()) {
            std::filesystem::create_directories(logPath.parent_path());
        }

        return true;
    }
    catch(const std::exception& e) {
        g_logger<<LANG("error_create_directory")<<e.what()<<std::endl;
        return false;
    }
}
// 默认配置
Json::Value Config::GetDefaultConfig() {
    Json::Value config;
    config["output_dir"]="./updates";
    config["server_host"]="127.0.0.1";
    config["server_port"]=8080;
    config["base_url"]="http://127.0.0.1:8080";
    config["hash_algorithm"]="sha256";
    config["enable_web_server"]=true;
    config["enable_incremental"]=true;
    config["max_package_versions"]=10;
    config["log_file"]="./logs/server.log";
    config["language"]="zh_CN";
    return config;
}