#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <string>
#include <sqlite3.h>
#include "payloadreader.hpp"

class Database {
private:
    sqlite3* db; // Puntatore al database SQLite
    std::string db_name;

public:
    Database(const std::string& db_name); // Costruttore
    ~Database();                          // Distruttore

    bool connect(void);                       // Metodo per aprire la connessione
    void disconnect(void);                    // Metodo per chiudere la connessione
    int CreateTables(void);
    char *InsertData(Payload payload);
    void printError(void);                    // Metodo per stampare errori
};

#endif