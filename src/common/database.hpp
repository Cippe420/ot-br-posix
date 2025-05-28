#ifndef DATABASE_HPP
#define DATABASE_HPP

#include "payloadreader.hpp"
#include <cstring>
#include <sqlite3.h>
#include <string>
#include <vector>


// Database Manager instance
class Database
{
private:
    sqlite3    *db;
    std::string db_name;

public:
    Database(const std::string &db_name);
    ~Database();

    bool        connect(void);
    void        disconnect(void);
    int         CreateTables(void);
    const char *InsertData(Payload payload);
    bool        CheckNewSensor(uint16_t eui);
    void        InsertSensor(uint16_t eui);
    void        SetSensorsState(std::vector<uint16_t> mRloc16);
    std::vector<uint64_t> GetEuiSensors(void);
    
    void        printError(void);
};

#endif
