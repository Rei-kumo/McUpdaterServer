#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <string>
#include <unordered_map>

class Language {
public:
    static Language& Instance() {
        static Language instance;
        return instance;
    }
	//FIXME: Language 单例的 SetLanguage 和 Get 没有同步机制，多线程未来修改
    void SetLanguage(const std::string& language) {
        currentLanguage=language;
    }
	//FIXME: 这里代码膨胀浪费内存
    std::string Get(const std::string& key) {
        auto it=strings.find(currentLanguage);
        if(it!=strings.end()) {
            auto keyIt=it->second.find(key);
            if(keyIt!=it->second.end()) {
                return keyIt->second;
            }
        }

        // 如果没找到，返回默认值
		//FIXEM: 以后把默认设置移动到语言中
        static std::unordered_map<std::string,std::string> defaultStrings={
            {"app_start","Minecraft更新服务器启动"},
            {"app_stop","Minecraft更新服务器停止"},
            {"config_load","加载配置文件"},
            {"config_missing","配置文件不存在，使用默认配置"},
            {"error_config","配置错误"},
            {"error_scan","扫描错误"},
            {"error_package","打包错误"},
            {"error_io","I/O错误"},
            {"error_time_conversion","时间转换错误"},
            {"scan_start","开始扫描工作目录"},
            {"scan_complete","扫描完成"},
            {"package_building","构建更新包"},
            {"package_complete","更新包构建完成"},
            {"diff_processing","处理文件差异"},
            {"diff_added","新增"},
            {"diff_modified","修改"},
            {"diff_deleted","删除"},
            {"diff_moved","移动"},
            {"server_start","Web服务器启动"},
            {"server_stop","Web服务器停止"},
            {"request_update","请求获取更新信息"},
            {"request_download","请求下载文件"},
            {"request_package","请求下载更新包"},
            {"status_idle","空闲"},
            {"status_scanning","扫描中"},
            {"status_building","构建中"},
            {"status_serving","服务中"},
            {"error_create_file","无法创建文件"},
            {"info_created"," 创建完成"},
            {"info_directory","目录"},
            {"error_version_not_exist","版本不存在"},
            {"error_version_snapshot_not_exist","版本快照不存在"},
            {"error_save_snapshot","无法保存快照"},
            {"error_open_file","无法打开文件"},
            {"error_parse_json","解析JSON失败"},
            {"error_zip_source","无法创建ZIP源"},
            {"error_add_file_zip","无法添加文件到ZIP"},
            {"error_create_marker","无法创建空目录标记"},
            {"info_empty_directory","# 空目录标记"},
            {"error_no_versions","没有可用的版本"},
            {"error_open_package","无法打开包"},
            {"info_file_not_found_simple","文件不存在"},
            {"error_file_not_found","文件不存在"},
            {"error_invalid_zip","无效的ZIP文件"},
            {"error_diff","差异计算错误"},
            {"error_server","服务器错误"},
            {"scan_complete_files","扫描完成，找到 {} 个文件，{} 个目录"},
            {"package_building_incremental","构建增量更新包: {} -> {}"},
            {"package_building_full","构建全量更新包: {}"},
            {"server_start_at","Web服务器启动于 {}:{}"}
        };

        auto defaultIt=defaultStrings.find(key);
        if(defaultIt!=defaultStrings.end()) {
            return defaultIt->second;
        }

        return key; // 如果默认也没有，返回键值
    }

private:
    Language() {
        LoadChinese();
        LoadEnglish();
    }

    void LoadChinese();
    void LoadEnglish();
	//FIXME: SetLanguage 未做有效性检查
    std::unordered_map<std::string,
        std::unordered_map<std::string,std::string>> strings;
    std::string currentLanguage="zh_CN";
};

#define LANG(key) Language::Instance().Get(key)

#endif