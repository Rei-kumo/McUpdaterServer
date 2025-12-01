#ifndef ZIPMANAGER_H
#define ZIPMANAGER_H

#include <string>
#include <zip.h>

class ZipManager {
public:
    static bool CreateZipFromDirectory(const std::string& dirPath,const std::string& zipPath);

private:
    static bool AddFileToZip(zip_t* zip,const std::string& filePath,const std::string& zipPath);
    static bool AddLargeFileToZip(zip_t* zip,const std::string& filePath,const std::string& zipPath);
};

#endif