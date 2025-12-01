#ifndef UPDATEAPIGENERATOR_H
#define UPDATEAPIGENERATOR_H

#include <string>
#include <json/json.h>
#include "ConfigManager.h"
#include "FileScanner.h"

class UpdateApiGenerator {
public:
    UpdateApiGenerator(ConfigManager& configManager,FileScanner& fileScanner);

    Json::Value GenerateUpdateInfo();

private:
    ConfigManager& configManager;
    FileScanner& fileScanner;
};

#endif