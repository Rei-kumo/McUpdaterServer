#include <iostream>
#include <fstream>
#include <filesystem>
#include <csignal>
#include <json/json.h>
#include "crow.h"
#include "ConfigManager.h"
#include "FileScanner.h"
#include "UpdateApiGenerator.h"
#include "Logger.h"

namespace fs=std::filesystem;

Json::Value g_updateInfo;

void signalHandler(int signum) {
    std::cout<<"[INFO] 接收到信号 "<<signum<<"，程序将退出"<<std::endl;
    exit(signum);
}

bool CreateDirectoryStructure() {
    try {
        std::vector<std::string> directories={
            "public",
            "public/mods",
            "public/config",
            "public/config/ui",
            "delete_list",
            "logs",
            "config",
            "cache"
        };

        std::cout<<"[INFO] 创建目录结构..."<<std::endl;
        for(const auto& dir:directories) {
            if(fs::create_directories(dir)) {
                std::cout<<"[INFO] 创建目录: "<<dir<<std::endl;
            }
        }

        std::cout<<"[INFO] 创建示例文件..."<<std::endl;

        std::ofstream modFile("public/mods/core.jar");
        modFile<<"This is a mock jar file for testing";
        modFile.close();
        std::cout<<"[INFO] 创建文件: public/mods/core.jar"<<std::endl;

        std::ofstream configFile("public/config/settings.json");
        configFile<<"{\n  \"game_settings\": \"default\"\n}";
        configFile.close();
        std::cout<<"[INFO] 创建文件: public/config/settings.json"<<std::endl;

        std::ofstream uiFile("public/config/ui/Dev-C++.lnk");
        uiFile<<"Mock shortcut file";
        uiFile.close();
        std::cout<<"[INFO] 创建文件: public/config/ui/Dev-C++.lnk"<<std::endl;

        std::ofstream deleteFile("delete_list/delete.txt");
        deleteFile<<"old_mods/obsolete.mod\ntemp/cache.data";
        deleteFile.close();
        std::cout<<"[INFO] 创建文件: delete_list/delete.txt"<<std::endl;

        return true;
    }
    catch(const std::exception& e) {
        std::cerr<<"[ERROR] 创建目录结构失败: "<<e.what()<<std::endl;
        return false;
    }
}

int main() {
    signal(SIGINT,signalHandler);
    signal(SIGTERM,signalHandler);

    std::string configPath="config/updater.json";
    std::string publicDir="public";
    std::string deleteListDir="delete_list";

    std::cout<<"[INFO] === McUpdaterServer 启动 ==="<<std::endl;

    bool firstRun=!fs::exists(configPath);

    if(firstRun) {
        std::cout<<"[INFO] 检测到首次运行，进行初始化..."<<std::endl;

        if(!CreateDirectoryStructure()) {
            std::cerr<<"[ERROR] 初始化失败!"<<std::endl;
            return 1;
        }

        ConfigManager configManager(configPath);

        std::cout<<"[INFO] 创建默认配置文件..."<<std::endl;
        if(!configManager.InitializeDefaultConfig()) {
            std::cerr<<"[ERROR] 创建默认配置失败!"<<std::endl;
            return 1;
        }
        std::cout<<"[INFO] 创建配置文件: "<<configPath<<std::endl;

        std::cout<<"[SUCCESS] 初始化完成!"<<std::endl;
        std::cout<<"[INFO] 目录结构已创建，请检查以下内容:"<<std::endl;
        std::cout<<"[INFO] 1. 编辑 "<<configPath<<" 文件配置服务器参数"<<std::endl;
        std::cout<<"[INFO] 2. 将您的游戏文件放入 public/ 目录"<<std::endl;
        std::cout<<"[INFO] 3. 配置删除列表文件 delete_list/delete.txt"<<std::endl;
        std::cout<<"[INFO] 4. 重新启动程序以运行服务器"<<std::endl;

        std::cout<<"[INFO] 按回车键退出..."<<std::endl;
        std::cin.get();
        return 0;
    }

    std::cout<<"[INFO] 检测到现有配置，启动服务器..."<<std::endl;

    if(!g_logger.Initialize("logs/api_server.log")) {
        std::cerr<<"[ERROR] 无法初始化日志文件"<<std::endl;
    }

    try {
        ConfigManager configManager(configPath);
        FileScanner fileScanner(publicDir,deleteListDir);
        UpdateApiGenerator apiGenerator(configManager,fileScanner);

        std::cout<<"[INFO] 开始扫描文件..."<<std::endl;
        g_updateInfo=apiGenerator.GenerateUpdateInfo();
        std::cout<<"[INFO] 文件扫描完成"<<std::endl;
        std::cout<<"[INFO] 发现 "<<g_updateInfo["files"].size()<<" 个文件"<<std::endl;
        std::cout<<"[INFO] 发现 "<<g_updateInfo["directories"].size()<<" 个目录"<<std::endl;
        std::cout<<"[INFO] 发现 "<<g_updateInfo["delete_list"].size()<<" 个待删除文件"<<std::endl;

        crow::SimpleApp app;

        CROW_ROUTE(app,"/api/update")([]() {
            Json::StyledWriter writer;
            std::string jsonString=writer.write(g_updateInfo);

            crow::response res;
            res.set_header("Content-Type","application/json");
            res.write(jsonString);

            g_logger<<"[INFO] 处理API更新请求"<<std::endl;
            return res;
            });

        CROW_ROUTE(app,"/<string>")([](const std::string& filename) {
            std::string filepath="public/"+filename;
            std::ifstream file(filepath,std::ios::binary);

            if(!file) {
                g_logger<<"[WARN] 文件未找到: "<<filename<<std::endl;
                return crow::response(404);
            }

            std::string content((std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());

            crow::response res;
            res.set_header("Content-Type","application/octet-stream");
            res.write(content);

            g_logger<<"[INFO] 提供文件下载: "<<filename<<std::endl;
            return res;
            });

        CROW_ROUTE(app,"/")([]() {
            return "McUpdaterServer 正在运行！访问 /api/update 获取更新信息。";
            });

        std::cout<<"[INFO] McUpdaterServer 启动在 http://localhost:8080"<<std::endl;
        std::cout<<"[INFO] API端点: http://localhost:8080/api/update"<<std::endl;
        std::cout<<"[INFO] 服务器已启动，按 Ctrl+C 停止"<<std::endl;

        app.port(8080).multithreaded().run();
    }
    catch(const std::exception& e) {
        std::cerr<<"[ERROR] 程序发生未捕获的异常: "<<e.what()<<std::endl;
        g_logger<<"[ERROR] 程序发生未捕获的异常: "<<e.what()<<std::endl;
        return 1;
    }
    catch(...) {
        std::cerr<<"[ERROR] 程序发生未知异常"<<std::endl;
        g_logger<<"[ERROR] 程序发生未知异常"<<std::endl;
        return 1;
    }

    return 0;
}