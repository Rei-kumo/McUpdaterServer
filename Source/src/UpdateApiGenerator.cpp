#include "UpdateApiGenerator.h"
#include "Logger.h"
#include <json/json.h>

UpdateApiGenerator::UpdateApiGenerator(ConfigManager& configManager,FileScanner& fileScanner)
    : configManager(configManager),fileScanner(fileScanner) {
}

Json::Value UpdateApiGenerator::GenerateUpdateInfo() {
    Json::Value updateInfo;

    updateInfo["version"]=configManager.ReadVersion();
    updateInfo["update_mode"]=configManager.ReadUpdateMode();
    updateInfo["timestamp"]=static_cast<Json::UInt64>(time(nullptr));

    Json::Value changelog=configManager.ReadChangelog();
    if(changelog.empty()) {
        changelog=Json::Value(Json::arrayValue);
        changelog.append("修复了游戏崩溃问题");
        changelog.append("新增了更多生物群系");
        changelog.append("优化了性能表现");
    }
    updateInfo["changelog"]=changelog;

    std::string baseUrl=configManager.ReadFileBaseUrl();
    std::string hashAlgorithm=configManager.ReadHashAlgorithm();

    Json::Value scanResult=fileScanner.ScanFiles(baseUrl,hashAlgorithm);
    updateInfo["files"]=scanResult["files"];
    updateInfo["directories"]=scanResult["directories"];
    updateInfo["delete_list"]=fileScanner.GenerateDeleteList();

    std::cout<<"[INFO] 生成更新信息，版本: "<<updateInfo["version"].asString()
        <<"，文件数: "<<updateInfo["files"].size()
        <<"，目录数: "<<updateInfo["directories"].size()<<std::endl;

    g_logger<<"[INFO]生成更新信息，版本: "<<updateInfo["version"].asString()<<std::endl;

    return updateInfo;
}