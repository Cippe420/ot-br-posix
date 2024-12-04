#include "database.hpp"
#include <iostream>
#include <cstring>

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


void DatabaseL::CreateTables()
{

    /*
    
    table -----
    
    |eui|pktnum|timestamp|undef|temperature|humidity|ir|vis|batt|temperature2|humidity2|pressure|gas_resistance|
    
    */
    const char *createTableQuery = "CREATE TABLE IF NOT EXISTS dati ("
                                       "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                       "valore TEXT NOT NULL);";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, createTableQuery, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errMsg);
        return;
    }

}


void DatabaseL::InsertData(char *data)
{

    char insertQuery[512];
    snprintf(insertQuery, sizeof(insertQuery), "INSERT INTO dati (valore) VALUES ('%s');", data);

    char* errorMessage = nullptr;
    if (sqlite3_exec(db, insertQuery, nullptr, nullptr, &errorMessage) != SQLITE_OK) {
        printf("Errore nell'inserimento dei dati: %s\n", errorMessage);
        sqlite3_free(errorMessage);
    } else {
        printf("Dati inseriti correttamente: %s\n", data);
    }

}


void DatabaseL::printError() {
    if (db != nullptr) {
        printf("Errore database: %s\n", sqlite3_errmsg(db));
    } else {
        printf("Il database non è connesso.\n");
    }
}
