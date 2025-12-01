#include "ZipManager.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include "Logger.h"
#include <cstdlib>
#include <algorithm>

bool ZipManager::CreateZipFromDirectory(const std::string& dirPath,const std::string& zipPath) {
    int err=0;

    std::cout<<"[INFO] 开始创建ZIP: "<<zipPath<<" 从目录: "<<dirPath<<std::endl;

    if(!std::filesystem::exists(dirPath)) {
        std::cerr<<"[ERROR] 源目录不存在: "<<dirPath<<std::endl;
        g_logger<<"[ERROR] 源目录不存在: "<<dirPath<<std::endl;
        return false;
    }

    std::filesystem::path zipFilePath(zipPath);
    std::filesystem::create_directories(zipFilePath.parent_path());

    zip_t* zip=zip_open(zipPath.c_str(),ZIP_CREATE|ZIP_TRUNCATE,&err);
    if(!zip) {
        std::cerr<<"[ERROR] 创建ZIP文件失败: "<<zipPath<<" 错误码: "<<err<<std::endl;
        g_logger<<"[ERROR] 创建ZIP文件失败: "<<zipPath<<" 错误码: "<<err<<std::endl;
        return false;
    }

    bool success=true;
    try {
        int fileCount=0;
        for(const auto& entry:std::filesystem::recursive_directory_iterator(dirPath)) {
            if(entry.is_regular_file()) {
                std::string relativePath=std::filesystem::relative(entry.path(),dirPath).string();
                std::replace(relativePath.begin(),relativePath.end(),'\\','/');

                std::cout<<"[INFO] 添加文件到ZIP: "<<relativePath<<std::endl;

                if(!AddFileToZip(zip,entry.path().string(),relativePath)) {
                    std::cerr<<"[ERROR] 添加文件失败: "<<relativePath<<std::endl;
                    success=false;
                    break;
                }
                fileCount++;
            }
        }

        if(success) {
            std::cout<<"[INFO] 成功添加 "<<fileCount<<" 个文件到ZIP"<<std::endl;
        }
        else {
            std::cerr<<"[ERROR] 创建ZIP过程中发生错误"<<std::endl;
        }
    }
    catch(const std::exception& e) {
        std::cerr<<"[ERROR] 遍历目录异常: "<<e.what()<<std::endl;
        g_logger<<"[ERROR] 遍历目录异常: "<<e.what()<<std::endl;
        success=false;
    }

    if(zip_close(zip)!=0) {
        std::cerr<<"[ERROR] 关闭ZIP文件失败: "<<zipPath<<std::endl;
        g_logger<<"[ERROR] 关闭ZIP文件失败: "<<zipPath<<std::endl;
        success=false;
    }

    if(!success&&std::filesystem::exists(zipPath)) {
        std::filesystem::remove(zipPath);
    }
    else if(success) {
        std::cout<<"[INFO] 成功创建ZIP文件: "<<zipPath<<std::endl;
        g_logger<<"[INFO] 成功创建ZIP文件: "<<zipPath<<std::endl;
    }

    return success;
}

bool ZipManager::AddFileToZip(zip_t* zip,const std::string& filePath,const std::string& zipPath) {
    if(!std::filesystem::exists(filePath)) {
        std::cerr<<"[ERROR] 文件不存在: "<<filePath<<std::endl;
        return false;
    }

    try {
        uintmax_t fileSize=std::filesystem::file_size(filePath);

        std::ifstream file(filePath,std::ios::binary);
        if(!file) {
            std::cerr<<"[ERROR] 无法打开文件: "<<filePath<<std::endl;
            g_logger<<"[ERROR] 无法打开文件: "<<filePath<<std::endl;
            return false;
        }

        size_t size = static_cast<size_t>(fileSize);
        char* data = nullptr;
        if(size > 0) {
            data = static_cast<char*>(std::malloc(size));
            if(!data) {
                std::cerr<<"[ERROR] 无法分配内存: "<<filePath<<std::endl;
                g_logger<<"[ERROR] 无法分配内存: "<<filePath<<std::endl;
                return false;
            }
            if(!file.read(data, size)) {
                std::free(data);
                std::cerr<<"[ERROR] 读取文件失败: "<<filePath<<std::endl;
                g_logger<<"[ERROR] 读取文件失败: "<<filePath<<std::endl;
                return false;
            }
        }
        file.close();

        zip_source_t* source = zip_source_buffer(zip, data, size, ZIP_SOURCE_FREE);
        if(!source) {
            if(data) std::free(data);
            std::cerr<<"[ERROR] 创建ZIP源失败: "<<zipPath<<" 错误: "<<zip_error_strerror(zip_get_error(zip))<<std::endl;
            g_logger<<"[ERROR] 创建ZIP源失败: "<<zipPath<<std::endl;
            return false;
        }

        zip_int64_t index = zip_file_add(zip, zipPath.c_str(), source, ZIP_FL_ENC_UTF_8);
        if(index < 0) {
            zip_source_free(source);
            std::cerr<<"[ERROR] 添加文件到ZIP失败: "<<zipPath<<" 错误: "<<zip_error_strerror(zip_get_error(zip))<<std::endl;
            g_logger<<"[ERROR] 添加文件到ZIP失败: "<<zipPath<<std::endl;
            return false;
        }

        std::cout<<"[INFO] 成功添加文件: "<<zipPath<<" 大小: "<<fileSize<<" 字节"<<std::endl;
        return true;
    }
    catch(const std::exception& e) {
        std::cerr<<"[ERROR] 处理文件异常: "<<filePath<<" - "<<e.what()<<std::endl;
        g_logger<<"[ERROR] 处理文件异常: "<<filePath<<" - "<<e.what()<<std::endl;
        return false;
    }
}

bool ZipManager::AddLargeFileToZip(zip_t* zip,const std::string& filePath,const std::string& zipPath) {
    zip_source_t* source=zip_source_file(zip,filePath.c_str(),0,0);
    if(!source) {
        std::cerr<<"[ERROR] 创建ZIP文件源失败: "<<zipPath<<std::endl;
        g_logger<<"[ERROR] 创建ZIP文件源失败: "<<zipPath<<std::endl;
        return false;
    }

    zip_int64_t index=zip_file_add(zip,zipPath.c_str(),source,ZIP_FL_ENC_UTF_8);
    if(index<0) {
        zip_source_free(source);
        std::cerr<<"[ERROR] 添加大文件到ZIP失败: "<<zipPath<<std::endl;
        g_logger<<"[ERROR] 添加大文件到ZIP失败: "<<zipPath<<std::endl;
        return false;
    }

    uintmax_t fileSize=std::filesystem::file_size(filePath);
    std::cout<<"[INFO] 成功添加大文件: "<<zipPath<<" 大小: "<<fileSize<<" 字节"<<std::endl;
    return true;
}