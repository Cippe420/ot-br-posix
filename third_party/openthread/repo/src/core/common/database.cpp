#include "database.hpp"
#include <iostream>
#include <cstring>
#include "payloadreader.hpp"

DatabaseL::DatabaseL(char *name)
{
    memcpy(db_name,name,MAX_LENGTH-1);
    db_name[MAX_LENGTH-1] = '\0';
    db = nullptr;

}

DatabaseL::~DatabaseL() {
    disconnect();
}

bool DatabaseL::connect() {
    int rc = sqlite3_open(db_name, &db);
    if (rc) {
        return false;
    }
    return true;
}

void DatabaseL::disconnect() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}   


int DatabaseL::CreateTables()
{

    /*
    
    table -----
    
    |eui|pktnum|timestamp|undef|temperature|humidity|ir|vis|batt|temperature2|humidity2|pressure|gas_resistance|
    
    */
    const char *createTableQuery = 
    "CREATE TABLE IF NOT EXISTS data ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, "  // Chiave primaria automatica
    "eui TEXT, "                     // Identificatore EUI in formato testo
    "pktnum INTEGER, "               // Numero del pacchetto
    "timestamp INTEGER, "            // Timestamp in UNIX time
    "undef INTEGER, "                         // Campo indefinito (opzionale)
    "temperature INTEGER, "                      // Temperatura
    "humidity INTEGER, "                         // Umidità
    "ir INTEGER, "                               // Luce infrarossa
    "vis INTEGER, "                              // Luce visibile
    "batt INTEGER, "                             // Stato della batteria
    "avg_temperature INTEGER, "                 // Media delle temperature
    "avg_humidity INTEGER, "                    // Media delle umidità
    "avg_pressure INTEGER, "                     // Media delle pressioni
    "avg_gas_resistance INTEGER"                 // Media della resistenza gas
    ");";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, createTableQuery, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errMsg);
        return 1;
    }

    return 0;

}


char *DatabaseL::InsertData(Payload payload)
{

    char insertQuery[512];
    snprintf(insertQuery, 
    sizeof(insertQuery),
    "INSERT INTO data (eui,pktnum,timestamp,undef,temperature,humidity,ir,vis,batt,avg_temperature,avg_humidity,avg_pressure,avg_gas_resistance) VALUES ('%s','%d','%d','%d','%d','%d','%d','%d','%d','%d','%d','%d','%d');",
     payload.eui,payload.pktnum,payload.timestamp,
     payload.undef,payload.temperature,payload.humidity,
     payload.ir,payload.vis,payload.batt,payload.avg_temperature,
     payload.avg_humidity,payload.avg_pressure,payload.avg_gas_resistance);

    char* errorMessage = nullptr;
    if (sqlite3_exec(db, insertQuery, nullptr, nullptr, &errorMessage) != SQLITE_OK)
    {
        return errorMessage;
    } 
    return nullptr;
}


void DatabaseL::printError() {
    if (db != nullptr) {
        printf("Errore database: %s\n", sqlite3_errmsg(db));
    } else {
        printf("Il database non è connesso.\n");
    }
}
