#include "DiffEngine.h"
#include "Language.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "Logger.h"

//主函数
//FIXME: 现有设计有缺陷，譬如若内容被改变的文件移动，DetectFileMovements 因为hash不同应该不会记录移动。
std::vector<ChangeRecord> DiffEngine::CalculateDiff(
    const std::vector<FileInfo>& oldFiles,
    const std::vector<FileInfo>& newFiles,
    const std::vector<DirectoryInfo>& oldDirs,
    const std::vector<DirectoryInfo>& newDirs) {

    g_logger<<LANG("diff_processing")<<std::endl;

    std::vector<ChangeRecord> changes;

    // 构建新旧文件映射
    std::unordered_map<std::string,const FileInfo*> oldFileMap;
    for(const auto& file:oldFiles) {
        oldFileMap[file.path]=&file;
    }

    std::unordered_map<std::string,const FileInfo*> newFileMap;
    for(const auto& file:newFiles) {
        newFileMap[file.path]=&file;
    }
	//optimize: 日志输出冗余，当文件数量较多时会产生大量日志，考虑增加日志级别控制
    // 新增和修改文件的检测
    for(const auto& newFile:newFiles) {
        auto it=oldFileMap.find(newFile.path);
        if(it==oldFileMap.end()) {
            // 新增文件检测
            ChangeRecord record;
            record.type=ChangeType::ADDED;
            record.path=newFile.path;
            record.hash=newFile.hash;
            record.size=newFile.size;
            changes.push_back(record);
            g_logger<<LANG("diff_added")<<newFile.path<<std::endl;
        }
        else {
            // 修改的检测
			// FIXME: 文件移动的检测依赖于哈希匹配，但当前实现存在潜在缺陷：若多个文件内容相同（哈希碰撞或重复内容），可能会误判移动关系。
            const FileInfo* oldFile=it->second;
            if(oldFile->hash!=newFile.hash) {
                ChangeRecord record;
                record.type=ChangeType::MODIFIED;
                record.path=newFile.path;
                record.hash=newFile.hash;
                record.size=newFile.size;
                changes.push_back(record);
                g_logger<<LANG("diff_modified")<<newFile.path<<std::endl;
            }
        }
    }

    // 删除的文件检测
    for(const auto& oldFile:oldFiles) {
        if(newFileMap.find(oldFile.path)==newFileMap.end()) {
            ChangeRecord record;
            record.type=ChangeType::DELETED;
            record.path=oldFile.path;
            record.hash=oldFile.hash;
            record.size=oldFile.size;
            changes.push_back(record);
            g_logger<<LANG("diff_deleted")<<oldFile.path<<std::endl;
        }
    }
    // 移动的文件检测
    DetectFileMovements(oldFiles,newFiles,changes);

    // 构建新旧目录映射
    std::unordered_map<std::string,const DirectoryInfo*> oldDirMap;
    for(const auto& dir:oldDirs) {
        oldDirMap[dir.path]=&dir;
    }

    std::unordered_map<std::string,const DirectoryInfo*> newDirMap;
    for(const auto& dir:newDirs) {
        newDirMap[dir.path]=&dir;
    }

    // 检查新增和删除的空目录
    for(const auto& newDir:newDirs) {
        auto it=oldDirMap.find(newDir.path);
        if(it==oldDirMap.end()&&newDir.files.empty()&&newDir.subdirectories.empty()) {
            ChangeRecord record;
            record.type=ChangeType::DIRECTORY_ADDED;
            record.path=newDir.path;
            changes.push_back(record);
            g_logger<<LANG("info_directory_added")<<newDir.path<<std::endl;
        }
    }

    for(const auto& oldDir:oldDirs) {
        if(newDirMap.find(oldDir.path)==newDirMap.end()&&
            oldDir.files.empty()&&oldDir.subdirectories.empty()) {
            ChangeRecord record;
            record.type=ChangeType::DIRECTORY_DELETED;
            record.path=oldDir.path;
            changes.push_back(record);
            g_logger<<LANG("info_directory_deleted")<<oldDir.path<<std::endl;
        }
    }
	//FIXEME : 非空目录的创建/删除不会产生变更记录，完全依赖文件条目。
    // 客户端虽然能通过文件路径隐式创建目录，但某些情况，若版本间目录由非空变为空（例如删光所有文件），现有逻辑不会生成 DIRECTORY_ADDED，导致客户端可能遗漏创建空目录。
    return changes;
}
// 检测文件移动
//OPTIMIZE: 每次匹配哈希后，都要遍历 changes 查找对应的 DELETED 和 ADDED 记录，导致最坏 O(n*m) 复杂度（n 为新文件数，m 为变更记录数）。
// 当变更量大时（如数万文件），性能急剧下降,现在只是对小文件没问题，以后务必修复。
void DiffEngine::DetectFileMovements(
    const std::vector<FileInfo>& oldFiles,
    const std::vector<FileInfo>& newFiles,
    std::vector<ChangeRecord>& changes) {

    // 哈希
    auto hashMap=BuildHashMap(oldFiles);

    // 遍历新文件，检查是否有相同哈希的旧文件
    for(const auto& newFile:newFiles) {
        auto it=hashMap.find(newFile.hash);
        if(it!=hashMap.end()&&!it->second.empty()) {
            // 找到相同哈希的旧文件
            const FileInfo* oldFile=it->second[0];

            // 确保路径不同，新文件被标记为新增，旧文件被标记为删除
            if(oldFile->path!=newFile.path) {
				// 新增和删除标记的检测
                // FIXME: 当多个旧文件哈希相同时（如重复空白文件），只取第一个匹配可能容易出问题。
                auto deleteIt=std::find_if(changes.begin(),changes.end(),
                    [&](const ChangeRecord& r) {
                        return r.type==ChangeType::DELETED&&r.path==oldFile->path;
                    });

                auto addIt=std::find_if(changes.begin(),changes.end(),
                    [&](const ChangeRecord& r) {
                        return r.type==ChangeType::ADDED&&r.path==newFile.path;
                    });
                // HACK: 当前 deleteIt 在 erase 后未被使用，因此迭代器失效无影响。
                //       但若将来代码修改后误用，可能会未定义，头大。
                //       以后务必修改，可能算一个严重bug。
                if(deleteIt!=changes.end()&&addIt!=changes.end()) {
					// 修改成移动
                    deleteIt->type=ChangeType::MOVED;
                    deleteIt->oldPath=oldFile->path;
                    changes.erase(addIt); 

                    g_logger<<LANG("info_moved")<<oldFile->path<<LANG("info_to")<<newFile.path<<std::endl;
                }
            }
        }
    }
}
// 哈希映射到列表
//FIXME: 实现太简单了，只匹配了哈希，如果有人用md5这样的哈希应该会出问题吧
std::unordered_map<std::string,std::vector<const FileInfo*>>
DiffEngine::BuildHashMap(const std::vector<FileInfo>& files) {
    std::unordered_map<std::string,std::vector<const FileInfo*>> hashMap;

    for(const auto& file:files) {
        hashMap[file.hash].push_back(&file);
    }

    return hashMap;
}
//manifest记录
//NOTE: 这里未考虑跨平台，譬如路径分隔符等问题，未来做的话需要改进。
std::string DiffEngine::GenerateManifest(const std::vector<ChangeRecord>& changes) {
    std::ostringstream oss;
    oss<<"# Update Manifest\n";
    oss<<"# Generated by MinecraftUpdaterServer\n";
    oss<<"# Format: TYPE:PATH:OLD_PATH:HASH:SIZE\n";
    oss<<"# TYPE: A=Added, M=Modified, D=Deleted, R=Moved, AD=Directory Added, DD=Directory Deleted\n\n";

    for(const auto& change:changes) {
        std::string typeStr;
        switch(change.type) {
        case ChangeType::ADDED: typeStr="A"; break;
        case ChangeType::MODIFIED: typeStr="M"; break;
        case ChangeType::DELETED: typeStr="D"; break;
        case ChangeType::MOVED: typeStr="R"; break;
        case ChangeType::DIRECTORY_ADDED: typeStr="AD"; break;
        case ChangeType::DIRECTORY_DELETED: typeStr="DD"; break;
        }

        oss<<typeStr<<":"
            <<change.path<<":"
            <<change.oldPath<<":"
            <<change.hash<<":"
            <<change.size<<"\n";
    }

    return oss.str();
}
//解析manifest记录
std::vector<ChangeRecord> DiffEngine::ParseManifest(const std::string& manifest) {
    std::vector<ChangeRecord> changes;
    std::istringstream iss(manifest);
    std::string line;

    while(std::getline(iss,line)) {
        // 跳过注释行和空行
        if(line.empty()||line[0]=='#') {
            continue;
        }

        std::istringstream lineStream(line);
        std::string token;
        std::vector<std::string> tokens;

        while(std::getline(lineStream,token,':')) {
            tokens.push_back(token);
        }

        if(tokens.size()>=2) {
            ChangeRecord record;

            // 解析类型
            if(tokens[0]=="A") record.type=ChangeType::ADDED;
            else if(tokens[0]=="M") record.type=ChangeType::MODIFIED;
            else if(tokens[0]=="D") record.type=ChangeType::DELETED;
            else if(tokens[0]=="R") record.type=ChangeType::MOVED;
            else if(tokens[0]=="AD") record.type=ChangeType::DIRECTORY_ADDED;
            else if(tokens[0]=="DD") record.type=ChangeType::DIRECTORY_DELETED;

            record.path=tokens[1];

            if(tokens.size()>2&&!tokens[2].empty()) {
                record.oldPath=tokens[2];
            }

            if(tokens.size()>3) {
                record.hash=tokens[3];
            }

            if(tokens.size()>4) {
                record.size=std::stoull(tokens[4]);
            }

            changes.push_back(record);
        }
    }

    return changes;
}