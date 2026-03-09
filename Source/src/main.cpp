#include "Logger.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include "Config.h"
#include "UpdateGenerator.h"
#include "WebServer.h"
#include "Language.h"
#include "VersionManager.h"
#include <windows.h>
#include <locale>
#include <codecvt>
#include <regex>
bool ValidateVersionFormat(const std::string& version) {
    std::regex versionPattern(R"(^\d+\.\d+\.\d+$)");
    return std::regex_match(version,versionPattern);
}

// 比较两个版本号，返回true如果v1 < v2
bool CompareVersions(const std::string& v1,const std::string& v2) {
    // 使用正则表达式分割版本号
    std::regex pattern(R"(\.)");
    std::sregex_token_iterator it1(v1.begin(),v1.end(),pattern,-1);
    std::sregex_token_iterator it2(v2.begin(),v2.end(),pattern,-1);
    std::sregex_token_iterator end;

    std::vector<int> parts1,parts2;

    while(it1!=end) {
        parts1.push_back(std::stoi(*it1));
        ++it1;
    }

    while(it2!=end) {
        parts2.push_back(std::stoi(*it2));
        ++it2;
    }

    // 比较版本号的每个部分
    size_t max_parts=std::max(parts1.size(),parts2.size());
    for(size_t i=0; i<max_parts; ++i) {
        int part1=(i<parts1.size())?parts1[i]:0;
        int part2=(i<parts2.size())?parts2[i]:0;

        if(part1<part2) return true;
        if(part1>part2) return false;
    }

    return false; // 所有部分都相等
}

void PrintHelp() {
    g_logger<<"Mc UpdaterServer v1.0.0"<<std::endl;
    g_logger<<"用法: mcupdaterserver [命令] [选项]"<<std::endl;
    g_logger<<std::endl;
    g_logger<<"命令:"<<std::endl;
    g_logger<<"  serve             启动Web服务器"<<std::endl;
    g_logger<<"  scan              扫描工作空间"<<std::endl;
    g_logger<<"  version <ver>     创建新版本"<<std::endl;
    g_logger<<"  incremental <from> <to>  创建增量更新包"<<std::endl;
    g_logger<<"  full <ver>        创建全量更新包"<<std::endl;
    g_logger<<"  init              初始化配置文件"<<std::endl;
    g_logger<<"  help              显示帮助"<<std::endl;
    g_logger<<std::endl;
    g_logger<<"选项:"<<std::endl;
    g_logger<<"  --config <path>   指定配置文件路径 (默认: config/server.json)"<<std::endl;
    g_logger<<"  --output <path>   指定输出目录"<<std::endl;
    g_logger<<"  --host <host>     指定服务器主机 (默认: 127.0.0.1)"<<std::endl;
    g_logger<<"  --port <port>     指定服务器端口 (默认: 8080)"<<std::endl;
    g_logger<<std::endl;
    g_logger<<LANG("info_put_game_files")<<std::endl;
}

void PrintMenu() {
    g_logger<<"\n===== Minecraft更新服务器菜单 ====="<<std::endl;
    g_logger<<"1. 启动Web服务器"<<std::endl;
    g_logger<<"2. 扫描工作空间"<<std::endl;
    g_logger<<"3. 创建新版本"<<std::endl;
    g_logger<<"4. 初始化配置"<<std::endl;
    g_logger<<"5. 显示帮助"<<std::endl;
    g_logger<<"6. 重置版本系统"<<std::endl;
    g_logger<<"7. 回退到指定版本"<<std::endl;
    g_logger<<"8. 退出"<<std::endl;
    g_logger<<"=================================="<<std::endl;
    g_logger<<"工作空间: ./public"<<std::endl;
    g_logger<<LANG("info_menu_choice");
}

int InteractiveMenu(Config& config) {
    while(true) {
        PrintMenu();

        int choice;
        std::cin>>choice;

        // 清除输入缓冲区
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');

        switch(choice) {
        case 1: { // 启动Web服务器
            // 检查工作空间目录是否存在
            if(!std::filesystem::exists(config.GetWorkspace())) {
                g_logger<<"[INFO] "<<LANG("info_workspace")<<config.GetWorkspace()<<std::endl;
                g_logger<<"[INFO] "<<LANG("info_put_game_files")<<std::endl;
                g_logger<<LANG("info_enter_continue")<<std::endl;
                std::cin.get();
                break;
            }

            // 启动Web服务器
            g_logger<<"[INFO] "<<LANG("info_server_starting")<<std::endl;
            g_logger<<"[INFO] "<<LANG("info_workspace")<<config.GetWorkspace()<<std::endl;
            g_logger<<"[INFO] "<<LANG("info_output_dir")<<config.GetOutputDir()<<std::endl;
            g_logger<<"[INFO] "<<LANG("info_server_address")<<config.GetBaseUrl()<<std::endl;
            g_logger<<"[INFO] "<<LANG("info_ctrl_c_stop")<<std::endl;
            g_logger<<std::endl;

            VersionManager versionManager(config.GetOutputDir()+"/data");
            if(!versionManager.Initialize()) {
                g_logger<<"[ERROR] "<<LANG("error_init_version_manager")<<std::endl;
                g_logger<<LANG("info_enter_continue")<<std::endl;
                std::cin.get();
                break;
            }

            WebServer server(config,versionManager,config.GetWorkspace());
            return server.Start()?0:1;
        }

        case 2: { // 扫描工作空间
            UpdateGenerator generator(config);
            if(!generator.Initialize()) {
                g_logger<<LANG("info_enter_continue")<<std::endl;
                std::cin.get();
                break;
            }

            if(!generator.ScanAndBuild()) {
                g_logger<<LANG("info_scan_failed")<<std::endl;
                g_logger<<LANG("info_enter_continue")<<std::endl;
                std::cin.get();
                break;
            }

            g_logger<<"[INFO] "<<LANG("scan_complete")<<std::endl;
            g_logger<<LANG("info_enter_continue")<<std::endl;
            std::cin.get();
            break;
        }

        case 3: { // 创建新版本
            std::string version;
            bool validVersion=false;

            while(!validVersion) {
                g_logger<<LANG("info_enter_version");
                std::getline(std::cin,version);

                // 验证版本号格式
                if(ValidateVersionFormat(version)) {
                    validVersion=true;
                }
                else {
                    g_logger<<"[ERROR] 版本号格式错误！必须使用 x.x.x 格式，如: 1.0.0, 2.1.3"<<std::endl;
                    g_logger<<"请重新输入版本号: ";
                }
            }

            g_logger<<LANG("info_enter_description");
            std::string description;
            std::getline(std::cin,description);

            UpdateGenerator generator(config);
            if(!generator.Initialize()) {
                g_logger<<LANG("info_enter_continue")<<std::endl;
                std::cin.get();
                break;
            }

            if(!generator.GenerateVersion(version,description)) {
                g_logger<<LANG("info_enter_continue")<<std::endl;
                std::cin.get();
                break;
            }

            g_logger<<"[INFO] "<<LANG("info_version")<<version<<LANG("info_created_complete")<<std::endl;
            g_logger<<LANG("info_enter_continue")<<std::endl;
            std::cin.get();
            break;
        }

        case 4: { // 初始化配置
            if(config.Save()) {
                g_logger<<"[INFO] "<<LANG("info_config_created")<<std::endl;

                // 创建必要目录（包括public目录）
                if(config.CreateDirectories()) {
                    g_logger<<"[INFO] "<<LANG("info_directories_created")<<std::endl;
                    g_logger<<"[INFO] "<<LANG("info_put_game_files")<<std::endl;
                }

                g_logger<<LANG("info_enter_continue")<<std::endl;
                std::cin.get();
            }
            else {
                g_logger<<"[ERROR] "<<LANG("error_create_directory")<<std::endl;
                g_logger<<LANG("info_enter_continue")<<std::endl;
                std::cin.get();
            }
            break;
        }

        case 5: { // 显示帮助
            PrintHelp();
            g_logger<<LANG("info_enter_continue")<<std::endl;
            std::cin.get();
            break;
        }

        case 6: { // 重置版本系统
            g_logger<<"[WARNING] 您确定要重置版本系统吗？所有版本数据将被删除！(y/N): ";
            std::string confirm;
            std::getline(std::cin,confirm);
            if(confirm=="y"||confirm=="Y") {
                std::string dataDir=config.GetOutputDir()+"/data";
                std::string snapshotsDir=config.GetOutputDir()+"/snapshots";
                std::string fullDir=config.GetOutputDir()+"/full";
                std::string incrementalDir=config.GetOutputDir()+"/incremental";
                std::string packagesDir=config.GetOutputDir()+"/packages"; // 新增

                std::filesystem::remove_all(dataDir);
                std::filesystem::remove_all(snapshotsDir);
                std::filesystem::remove_all(fullDir);
                std::filesystem::remove_all(incrementalDir);
                std::filesystem::remove_all(packagesDir); // 删除目录包

                std::filesystem::create_directories(dataDir);
                std::filesystem::create_directories(snapshotsDir);
                std::filesystem::create_directories(fullDir);
                std::filesystem::create_directories(incrementalDir);
                std::filesystem::create_directories(packagesDir); // 重新创建

                g_logger<<"[INFO] 版本系统已重置"<<std::endl;
            }
            else {
                g_logger<<"[INFO] 操作已取消"<<std::endl;
            }

            g_logger<<LANG("info_enter_continue")<<std::endl;
            std::cin.get();
            break;
        }

        case 7: { // 回退到指定版本（删除后续版本）
            VersionManager versionManager(config.GetOutputDir()+"/data");
            if(!versionManager.Initialize()) {
                g_logger<<"[ERROR] 无法初始化版本管理器"<<std::endl;
                break;
            }
            auto versions=versionManager.GetVersionList();
            if(versions.empty()) {
                g_logger<<"[INFO] 没有可回退的版本"<<std::endl;
                break;
            }
            g_logger<<"现有版本："<<std::endl;
            for(size_t i=0; i<versions.size(); ++i) {
                g_logger<<"  "<<i+1<<". "<<versions[i]<<std::endl;
            }
            g_logger<<"请输入要回退到的目标版本号（将删除此版本之后的所有版本）: ";
            std::string targetVersion;
            std::getline(std::cin,targetVersion);
            if(versionManager.GetVersion(targetVersion)==nullptr) {
                g_logger<<"[ERROR] 版本不存在"<<std::endl;
                break;
            }
            // 获取当前最新版本
            std::string latestVersion=versions.back();
            if(targetVersion==latestVersion) {
                g_logger<<"[INFO] 目标版本就是最新版本，无需回退"<<std::endl;
                break;
            }

            // 收集要删除的版本
            std::vector<std::string> toDelete;
            for(const auto& v:versions) {
                if(v>targetVersion) toDelete.push_back(v);
            }
            g_logger<<"[WARNING] 此操作将永久删除版本: ";
            for(const auto& v:toDelete) g_logger<<v<<" ";
            g_logger<<"\n确定要继续吗？(y/N): ";
            std::string confirm;
            std::getline(std::cin,confirm);
            if(confirm!="y"&&confirm!="Y") {
                g_logger<<"[INFO] 操作已取消"<<std::endl;
                break;
            }

            // 从大到小删除版本（保证依赖关系正确）
            bool success=true;
            for(auto it=toDelete.rbegin(); it!=toDelete.rend(); ++it) {
                if(!versionManager.DeleteVersion(*it)) {
                    g_logger<<"[ERROR] 删除版本 "<<*it<<" 失败，回退中断"<<std::endl;
                    success=false;
                    break;
                }
            }

            if(!success) {
                g_logger<<"[ERROR] 回退过程中发生错误，请检查版本一致性"<<std::endl;
                break;
            }

            // 重新生成目标版本的目录包
            UpdateGenerator generator(config);
            if(!generator.Initialize()) {
                g_logger<<"[ERROR] 无法初始化生成器"<<std::endl;
                break;
            }
            std::vector<FileInfo> targetFiles;
            std::vector<DirectoryInfo> targetDirs;
            if(!generator.GetPreviousVersionFiles(targetVersion,targetFiles,targetDirs)) {
                g_logger<<"[ERROR] 无法加载目标版本的快照"<<std::endl;
                break;
            }
            if(!generator.CreateDirectoryPackages(targetVersion,targetDirs)) {
                g_logger<<"[ERROR] 重新生成目录包失败"<<std::endl;
                break;
            }

            g_logger<<"[INFO] 已成功回退到版本 "<<targetVersion<<"，后续版本已删除"<<std::endl;
            break;
        }

        case 8: { // 退出
            g_logger<<LANG("info_goodbye")<<std::endl;
            return 0;
        }

        default:
            g_logger<<LANG("info_invalid_choice")<<std::endl;
            g_logger<<LANG("info_enter_continue")<<std::endl;
            std::cin.get();
            break;
        }
    }

    return 0;
}

int ExecuteCommand(const std::string& command,const std::vector<std::string>& commandArgs,Config& config) {
    if(command=="serve") {
        // 检查工作空间目录是否存在
        if(!std::filesystem::exists(config.GetWorkspace())) {
            g_logger<<"[INFO] "<<LANG("info_workspace")<<config.GetWorkspace()<<std::endl;
            g_logger<<"[INFO] "<<LANG("info_put_game_files")<<std::endl;
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        // 启动Web服务器
        g_logger<<"[INFO] "<<LANG("info_server_starting")<<std::endl;
        g_logger<<"[INFO] "<<LANG("info_workspace")<<config.GetWorkspace()<<std::endl;
        g_logger<<"[INFO] "<<LANG("info_output_dir")<<config.GetOutputDir()<<std::endl;
        g_logger<<"[INFO] "<<LANG("info_server_address")<<config.GetBaseUrl()<<std::endl;
        g_logger<<"[INFO] "<<LANG("info_ctrl_c_stop")<<std::endl;
        g_logger<<std::endl;

        VersionManager versionManager(config.GetOutputDir()+"/data");
        if(!versionManager.Initialize()) {
            g_logger<<"[ERROR] "<<LANG("error_init_version_manager")<<std::endl;
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        WebServer server(config,versionManager,config.GetWorkspace());
        if(!server.Start()) {
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }
        return 0;
    }
    else if(command=="scan") {
        // 扫描工作空间
        UpdateGenerator generator(config);
        if(!generator.Initialize()) {
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        if(!generator.ScanAndBuild()) {
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        g_logger<<"[INFO] "<<LANG("scan_complete")<<std::endl;
        g_logger<<LANG("info_enter_exit")<<std::endl;
        std::cin.get();
        return 0;
    }
    else if(command=="version") {
        // 创建新版本
        if(commandArgs.empty()) {
            g_logger<<"[ERROR] "<<LANG("info_enter_version")<<std::endl;
            PrintHelp();
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        std::string version=commandArgs[0];
        std::string description=commandArgs.size()>1?commandArgs[1]:"";

        UpdateGenerator generator(config);
        if(!generator.Initialize()) {
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        if(!generator.GenerateVersion(version,description)) {
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        g_logger<<"[INFO] "<<LANG("info_version")<<version<<LANG("info_created_complete")<<std::endl;
        g_logger<<LANG("info_enter_exit")<<std::endl;
        std::cin.get();
        return 0;
    }
    else if(command=="incremental") {
        // 创建增量包
        if(commandArgs.size()<2) {
            g_logger<<"[ERROR] "<<LANG("error_config")<<LANG("info_enter_from_version")<<std::endl;
            PrintHelp();
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        std::string fromVersion=commandArgs[0];
        std::string toVersion=commandArgs[1];

        UpdateGenerator generator(config);
        if(!generator.Initialize()) {
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        // 需要先扫描当前文件
        if(!generator.ScanAndBuild()) {
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        if(!generator.GenerateIncrementalPackage(fromVersion,toVersion)) {
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        g_logger<<"[INFO] "<<LANG("info_incremental")<<fromVersion<<" -> "<<toVersion<<LANG("info_created_complete")<<std::endl;
        g_logger<<LANG("info_enter_exit")<<std::endl;
        std::cin.get();
        return 0;
    }
    else if(command=="full") {
        // 创建全量包
        if(commandArgs.empty()) {
            g_logger<<"[ERROR] "<<LANG("info_enter_version")<<std::endl;
            PrintHelp();
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        std::string version=commandArgs[0];

        UpdateGenerator generator(config);
        if(!generator.Initialize()) {
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        // 需要先扫描当前文件
        if(!generator.ScanAndBuild()) {
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        if(!generator.GenerateFullPackage(version)) {
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        g_logger<<"[INFO] "<<LANG("info_full")<<version<<LANG("info_created_complete")<<std::endl;
        g_logger<<LANG("info_enter_exit")<<std::endl;
        std::cin.get();
        return 0;
    }
    else if(command=="init") {
        // 初始化配置
        if(config.Save()) {
            g_logger<<"[INFO] "<<LANG("info_config_created")<<std::endl;

            // 创建必要目录
            if(config.CreateDirectories()) {
                g_logger<<"[INFO] "<<LANG("info_directories_created")<<std::endl;
                g_logger<<"[INFO] "<<LANG("info_put_game_files")<<std::endl;
            }

            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 0;
        }
        else {
            g_logger<<"[ERROR] "<<LANG("error_create_directory")<<std::endl;
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }
    }
    else if(command=="help"||command=="--help"||command=="-h") {
        PrintHelp();
        g_logger<<LANG("info_enter_exit")<<std::endl;
        std::cin.get();
        return 0;
    }

    return -1; // 未处理的命令
}

int main(int argc,char* argv[]) {
	SetConsoleOutputCP(CP_UTF8);
    std::setlocale(LC_ALL,"zh_CN.UTF-8");

    // 解析命令行参数
    std::string configPath="config/server.json";
    std::string command="";
    std::string outputOverride;
    std::string hostOverride;
    std::string portStr;
    int portOverride=0;

    std::vector<std::string> commandArgs;

    for(int i=1; i<argc; ++i) {
        std::string arg=argv[i];

        if(arg=="--config") {
            if(i+1<argc) {
                configPath=argv[i+1];
                i++;
            }
        }
        else if(arg=="--output") {
            if(i+1<argc) {
                outputOverride=argv[i+1];
                i++;
            }
        }
        else if(arg=="--host") {
            if(i+1<argc) {
                hostOverride=argv[i+1];
                i++;
            }
        }
        else if(arg=="--port") {
            if(i+1<argc) {
                portStr=argv[i+1];
                try {
                    portOverride=std::stoi(portStr);
                }
                catch(const std::exception& e) {
                    g_logger<<LANG("error_config")<<portStr<<std::endl;
                    return 1;
                }
                i++;
            }
        }
        else if(arg=="help"||arg=="--help"||arg=="-h") {
            PrintHelp();
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 0;
        }
        else if(arg[0]!='-') {
            if(command.empty()) {
                command=arg;
            }
            else {
                commandArgs.push_back(arg);
            }
        }
    }

    // 创建配置对象
    Config config(configPath);

    // 检查配置文件是否存在
    if(!std::filesystem::exists(configPath)) {
        g_logger<<"[INFO] "<<LANG("info_no_config")<<std::endl;

        // 确保配置目录存在
        std::filesystem::path configFilePath(configPath);
        std::filesystem::create_directories(configFilePath.parent_path());

        // 保存默认配置
        if(!config.Save()) {
            g_logger<<"[ERROR] "<<LANG("error_create_directory")<<std::endl;
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        // 创建必要目录（包括public目录）
        if(!config.CreateDirectories()) {
            g_logger<<"[ERROR] "<<LANG("error_create_directory")<<std::endl;
            g_logger<<LANG("info_enter_exit")<<std::endl;
            std::cin.get();
            return 1;
        }

        g_logger<<"[INFO] "<<LANG("info_default_config_created")<<std::endl;
        g_logger<<"[INFO] "<<LANG("info_put_game_files")<<std::endl;
        g_logger<<"[INFO] "<<LANG("info_run_server")<<std::endl;
        g_logger<<"[INFO] "<<LANG("info_enter_exit")<<std::endl;
        std::cin.get();
        return 0;
    }

    // 配置文件存在，加载配置
    if(!config.Load()) {
        g_logger<<"[ERROR] "<<LANG("error_config")<<LANG("error_open_file")<<std::endl;
        g_logger<<LANG("info_enter_exit")<<std::endl;
        std::cin.get();
        return 1;
    }

    // 覆盖配置
    if(!outputOverride.empty()) {
        config.SetOutputDir(outputOverride);
    }
    if(!hostOverride.empty()) {
        config.SetServerHost(hostOverride);
    }
    if(portOverride>0) {
        config.SetServerPort(portOverride);
    }

    // 确保必要目录存在（包括public目录）
    if(!config.CreateDirectories()) {
        g_logger<<"[ERROR] "<<LANG("error_create_directory")<<std::endl;
        g_logger<<LANG("info_enter_exit")<<std::endl;
        std::cin.get();
        return 1;
    }
    g_logger.Initialize(config.GetLogFile());


    // 如果有命令行参数，执行相应命令
    if(!command.empty()) {
        int result=ExecuteCommand(command,commandArgs,config);
        if(result>=0) {
            return result;
        }
    }

    // 如果没有命令行参数或命令未识别，显示交互式菜单
    return InteractiveMenu(config);
}