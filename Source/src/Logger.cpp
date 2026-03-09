#include "Logger.h"
#include <filesystem>

Logger g_logger;

Logger::Logger(): enabled(false),isNewLine(true){}

Logger::~Logger() {
    if(logFile.is_open()) {
        logFile.close();
    }
}
//FIXME: Initialize 中错误处理不够全面,可在logFile.close() 释放资源
bool Logger::Initialize(const std::string& filename) {
    logFileName=filename;

    std::filesystem::path path(filename);
    std::filesystem::create_directories(path.parent_path());
    logFile.open(filename,std::ios::app);
    if(!logFile.is_open()) {
        std::cerr<<"[ERROR]无法打开日志文件: "<<filename<<std::endl;
        enabled=false;
        return false;
    }

    enabled=true;
    isNewLine=true;
    *this<<"[INFO]=== McUpdaterServer 日志开始 ==="<<std::endl;
    return true;
}

void Logger::Enable(bool enable) {
    enabled=enable;
}
//FIXME: 多线程同时操作 logFile 和 isNewLine 状态存在数据竞争。加入互斥锁
std::string Logger::GetTimestamp() {
    auto now=std::chrono::system_clock::now();
    auto time_t=std::chrono::system_clock::to_time_t(now);
    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch())%1000;

    std::tm local_tm;
#ifdef _WIN32
    localtime_s(&local_tm,&time_t);  // Windows版本
#else
    localtime_r(&time_t,&local_tm);  // Linux版本
#endif

    std::stringstream ss;
    ss<<"["<<std::put_time(&local_tm,"%Y-%m-%d %H:%M:%S");
    ss<<"."<<std::setfill('0')<<std::setw(3)<<ms.count()<<"]";
    return ss.str();
}