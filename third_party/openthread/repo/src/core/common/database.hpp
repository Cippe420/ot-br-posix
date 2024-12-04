#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <sqlite3.h>
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
    void CreateTables(void);
    void InsertData(char *data);
    void printError(void);                    // Metodo per stampare errori
};

#endif