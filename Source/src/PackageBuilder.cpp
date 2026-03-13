#include "PackageBuilder.h"
#include "Language.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include "Logger.h"
bool PackageBuilder::CreateIncrementalPackage(
    const std::string& oldVersion,
    const std::string& newVersion,
    const std::vector<ChangeRecord>& changes,
    const std::string& workspace,
    const std::string& outputPath) {

    std::cout<<LANG("package_building_incremental")<<oldVersion<<LANG("info_to")<<newVersion<<std::endl;

    int error=0;
    zip_t* zip=zip_open(outputPath.c_str(),ZIP_CREATE|ZIP_TRUNCATE,&error);
    if(!zip) {
        g_logger<<LANG("error_create_package")<<outputPath<<std::endl;
        return false;
    }

    std::string manifest=DiffEngine::GenerateManifest(changes);
    if(!AddManifestToZip(zip,manifest)) {
        zip_close(zip);
        return false;
    }

    for(const auto& change:changes) {
        if(change.type==ChangeType::ADDED||
            change.type==ChangeType::MODIFIED||
            change.type==ChangeType::MOVED) {

            std::string filePath=workspace+"/"+change.path;
            if(std::filesystem::exists(filePath)) {
                if(!AddFileToZip(zip,filePath,change.path)) {
                    zip_close(zip);
                    return false;
                }
            }
        }
        else if(change.type==ChangeType::DIRECTORY_ADDED) {
            if(!AddEmptyDirectoryMarker(zip,change.path)) {
                zip_close(zip);
                return false;
            }
        }
    }

    if(zip_close(zip)<0) {
        zip_error_t* error=zip_get_error(zip);
        g_logger<<LANG("error_close_package")<<": "<<zip_error_strerror(error)<<std::endl;
        return false;
    }
    std::cout<<LANG("package_complete")<<outputPath<<std::endl;
    return true;
}
bool PackageBuilder::CreateFullPackage(
    const std::string& version,
    const std::vector<FileInfo>& files,
    const std::vector<DirectoryInfo>& dirs,
    const std::string& workspace,
    const std::string& outputPath) {

    std::cout<<LANG("package_building_full")<<version<<std::endl;

    int error=0;
    zip_t* zip=zip_open(outputPath.c_str(),ZIP_CREATE|ZIP_TRUNCATE,&error);
    if(!zip) {
        g_logger<<LANG("error_create_package")<<outputPath<<std::endl;
        return false;
    }

    std::vector<ChangeRecord> changes;
    for(const auto& file:files) {
        ChangeRecord record;
        record.type=ChangeType::ADDED;
        record.path=file.path;
        record.hash=file.hash;
        record.size=file.size;
        changes.push_back(record);
    }
    for(const auto& dir:dirs) {
        if(dir.files.empty()&&dir.subdirectories.empty()) {
            ChangeRecord record;
            record.type=ChangeType::DIRECTORY_ADDED;
            record.path=dir.path;
            changes.push_back(record);
        }
    }

    std::string manifest=DiffEngine::GenerateManifest(changes);
    if(!AddManifestToZip(zip,manifest)) {
        zip_close(zip);
        return false;
    }

    for(const auto& file:files) {
        std::string filePath=workspace+"/"+file.path;
        if(std::filesystem::exists(filePath)) {
            if(!AddFileToZip(zip,filePath,file.path)) {
                zip_close(zip);
                return false;
            }
        }
    }

    for(const auto& dir:dirs) {
        if(dir.files.empty()&&dir.subdirectories.empty()) {
            if(!AddEmptyDirectoryMarker(zip,dir.path)) {
                zip_close(zip);
                return false;
            }
        }
    }

    if(zip_close(zip)<0) {
        zip_error_t* error=zip_get_error(zip);
        g_logger<<LANG("error_close_package")<<": "<<zip_error_strerror(error)<<std::endl;
        return false;
    }
    std::cout<<LANG("package_complete")<<outputPath<<std::endl;
    return true;
}

bool PackageBuilder::CreateDirectoryPackage(
    const std::string& rootDir,
    const std::vector<DirectoryInfo>& allDirs,
    const std::vector<FileInfo>& allFiles,
    const std::string& workspace,
    const std::string& outputPath) {

    int error=0;
    zip_t* zip=zip_open(outputPath.c_str(),ZIP_CREATE|ZIP_TRUNCATE,&error);
    if(!zip) {
        g_logger<<LANG("error_create_package")<<outputPath<<std::endl;
        return false;
    }

    if(rootDir.empty()) {
        // 根目录包：只打包根目录下的直接文件（路径中不含 '/'）
        for(const auto& file:allFiles) {
            if(file.path.find('/')==std::string::npos) {
                std::string fullPath=workspace+"/"+file.path;
                if(!AddFileToZip(zip,fullPath,file.path)) {
                    zip_close(zip);
                    return false;
                }
            }
        }
    }
    else {
        // 子目录包：递归打包整个目录树
        std::string physicalRoot=workspace+"/"+rootDir;
        if(std::filesystem::exists(physicalRoot)) {
            if(!AddDirectoryRecursively(zip,physicalRoot,rootDir)) {
                zip_close(zip);
                return false;
            }
        }
        else {
            g_logger<<"[WARNING] 目录不存在: "<<physicalRoot<<std::endl;
        }
    }

    if(zip_close(zip)<0) {
        zip_error_t* error=zip_get_error(zip);
        g_logger<<LANG("error_close_package")<<": "<<zip_error_strerror(error)<<std::endl;
        return false;
    }
    return true;
}

bool PackageBuilder::AddDirectoryRecursively(zip_t* zip,const std::string& physicalPath,const std::string& zipPath) {
    if(!zipPath.empty()) {
        zip_dir_add(zip,zipPath.c_str(),ZIP_FL_ENC_UTF_8);
    }

    std::error_code ec;
    for(const auto& entry:std::filesystem::directory_iterator(physicalPath,ec)) {
        if(ec) {
            g_logger<<LANG("error_scan")<<": "<<ec.message()<<std::endl;
            return false;
        }

        std::string entryName=entry.path().filename().string();
        std::string entryPhysical=entry.path().string();
        std::string entryZip;
        if(zipPath.empty()) {
            entryZip=entryName;
        }
        else {
            entryZip=zipPath+"/"+entryName;
        }

        if(entry.is_directory()) {
            // 递归添加子目录
            if(!AddDirectoryRecursively(zip,entryPhysical,entryZip)) {
                return false;
            }
        }
        else if(entry.is_regular_file()) {
            // 添加文件
            if(!AddFileToZip(zip,entryPhysical,entryZip)) {
                return false;
            }
        }
    }
    return true;
}
/*
//目前无用，之后修复，以后可能会用
bool PackageBuilder::AddDirectoryToZip(
    zip_t* zip,
    const std::string& dirPath,
    const std::unordered_map<std::string,std::vector<FileInfo>>& dirFilesMap,
    const std::unordered_map<std::string,std::vector<Json::String>>& subdirsMap,
    const std::string& workspace,
    bool includeSubdirMarkers) {   // 新增参数

    // 如果当前目录不是根目录，先添加目录条目（便于工具识别目录结构）
    if(!dirPath.empty()) {
        zip_dir_add(zip,dirPath.c_str(),ZIP_FL_ENC_UTF_8);
    }

    // 添加当前目录下的直接文件
    auto fit=dirFilesMap.find(dirPath);
    if(fit!=dirFilesMap.end()) {
        for(const auto& file:fit->second) {
            std::string fullPath=workspace+"/"+file.path;
            if(!AddFileToZip(zip,fullPath,file.path)) {
                return false;
            }
        }
    }

    // 如果需要包含子目录的空标记
    if(includeSubdirMarkers) {
        auto sit=subdirsMap.find(dirPath);
        if(sit!=subdirsMap.end()) {
            for(const auto& subdirName:sit->second) {
                // 构建子目录在 ZIP 中的完整路径
                std::string subdirZipPath;
                if(dirPath.empty()) {
                    subdirZipPath=std::string(subdirName);
                }
                else {
                    subdirZipPath=dirPath+"/"+std::string(subdirName);
                }

                if(!AddEmptyDirectoryMarker(zip,subdirZipPath)) {
                    return false;
                }
            }
        }
    }

    return true;
}
*/

bool PackageBuilder::AddEmptyDirectoryMarker(zip_t* zip,const std::string& dirPath) {
    std::string markerContent=LANG("info_empty_directory")+std::string(" ")+dirPath+"\n";
    std::string zipPath;
    if(dirPath.empty()||dirPath==".") {
        zipPath=".empty_dir_marker";
    }
    else {
        zipPath=dirPath+"/.empty_dir_marker";
    }

    zip_source_t* source=zip_source_buffer(zip,markerContent.c_str(),markerContent.length(),0);
    if(!source) {
        g_logger<<LANG("error_zip_source")<<" for empty directory marker"<<std::endl;
        return false;
    }

    if(zip_file_add(zip,zipPath.c_str(),source,ZIP_FL_OVERWRITE|ZIP_FL_ENC_UTF_8)<0) {
        zip_source_free(source);
        zip_error_t* error=zip_get_error(zip);
        g_logger<<LANG("error_add_file_zip")<<": "<<zipPath
            <<" - Error: "<<zip_error_strerror(error)<<std::endl;
        return false;
    }

    return true;
}

bool PackageBuilder::AddManifestToZip(zip_t* zip,const std::string& manifest) {
    zip_source_t* source=zip_source_buffer(zip,manifest.c_str(),manifest.length(),0);
    if(!source) {
        g_logger<<LANG("error_zip_source")<<std::endl;
        return false;
    }

    if(zip_file_add(zip,"update_manifest.txt",source,ZIP_FL_OVERWRITE)<0) {
        zip_source_free(source);
        g_logger<<LANG("error_add_file_zip")<<"update_manifest.txt"<<std::endl;
        return false;
    }

    return true;
}

bool PackageBuilder::AddFileToZip(zip_t* zip,const std::string& filePath,const std::string& zipPath) {
    std::string normalizedZipPath=zipPath;
    std::replace(normalizedZipPath.begin(),normalizedZipPath.end(),'\\','/');
    if(normalizedZipPath.find("./")==0) {
        normalizedZipPath=normalizedZipPath.substr(2);
    }
    else if(normalizedZipPath.find("/")==0) {
        normalizedZipPath=normalizedZipPath.substr(1);
    }

    zip_source_t* source=nullptr;

	source=zip_source_file_create(filePath.c_str(),0,0,nullptr);

    if(!source) {
		zip_error_t* error=zip_get_error(zip);
        g_logger<<LANG("error_zip_source")<<": "<<filePath<<"-"<<zip_error_strerror(error)<<std::endl;
        return false;
    }

    if(zip_file_add(zip,normalizedZipPath.c_str(),source,ZIP_FL_OVERWRITE|ZIP_FL_ENC_UTF_8)<0) {
        zip_source_free(source);
        zip_error_t* error=zip_get_error(zip);
        g_logger<<LANG("error_add_file_zip")<<": "<<normalizedZipPath
            <<" - Error: "<<zip_error_strerror(error)<<std::endl;
        return false;
    }

    return true;
}