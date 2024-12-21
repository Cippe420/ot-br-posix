#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <sqlite3.h>
#include "payloadreader.hpp"
#define MAX_LENGTH 256

class DatabaseL {
private:
    sqlite3* db; // Puntatore al database SQLite
    char db_name[MAX_LENGTH];

public:
    DatabaseL(char *name); // Costruttore
    ~DatabaseL();                          // Distruttore

    bool connect(void);                       // Metodo per aprire la connessione
    void disconnect(void);                    // Metodo per chiudere la connessione
    int CreateTables(void);
    char *InsertData(Payload payload);
    void printError(void);                    // Metodo per stampare errori
};

#endif