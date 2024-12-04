#include "database.hpp"
#include <iostream>

Database::Database(const std::string& db_name) : db(nullptr), db_name(db_name) {}

Database::~Database() {
    disconnect();
}

bool Database::connect() {
    int rc = sqlite3_open(db_name.c_str(), &db);
    if (rc) {
        std::cerr << "Errore nell'apertura del database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    std::cout << "Connessione al database avvenuta con successo." << std::endl;
    return true;
}

void Database::disconnect() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
        std::cout << "Connessione al database chiusa." << std::endl;
    }
}   


void Database::CreateTables()
{
    std::string createTableQuery = "CREATE TABLE IF NOT EXISTS dati ("
                                       "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                       "valore TEXT NOT NULL);";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, createTableQuery.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Errore nell'esecuzione della query: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return;
    }
    std::cout << "Query eseguita con successo." << std::endl;

}


void Database::InsertData(std::string data)
{
    std::string insertDataQuery = "INSERT INTO dati (valore) VALUES ('" + data + "');";
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, insertDataQuery.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Errore nell'esecuzione della query: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return;
    }
    std::cout << "Query eseguita con successo." << std::endl;

}


void Database::printError() {
    if (db) {
        std::cerr << "Errore: " << sqlite3_errmsg(db) << std::endl;
    }
}
