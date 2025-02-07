#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <string>
#include <sqlite3.h>
#include "payloadreader.hpp"

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
    char *InsertData(Payload payload);
    bool CheckNewSensor(char *eui);
    char InsertSensor(char *eui);
    void printError(void);                    
};

#endif