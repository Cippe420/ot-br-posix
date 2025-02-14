#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <string>
#include <sqlite3.h>
#include "payloadreader.hpp"
#include <iomanip>
#include <sstream>
#include <cstring>

// Database Manager instance
class Database {
private:
    sqlite3* db; 
    std::string db_name;

public:
    Database(const std::string& db_name); 
    ~Database();                         

    bool connect(void);                      
    void disconnect(void);                    
    int CreateTables(void);
    const char *InsertData(Payload payload);
    bool CheckNewSensor(uint64_t eui);
    void InsertSensor(uint64_t eui);
    void printError(void);                    
};

#endif