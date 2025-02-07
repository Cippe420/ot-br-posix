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

int Database::CreateTables()
{

    /*
    
    table -----
    
    |eui|pktnum|timestamp|undef|temperature|humidity|ir|vis|batt|temperature2|humidity2|pressure|gas_resistance|
    
    */

    char* errMsg = nullptr;
    const char *createSensorTableQuery = "CREATE TABLE IF NOT EXISTS sensors ("
    "id TEXT PRIMARY KEY, "
    "state TEXT"
    ");";

    int rc = sqlite3_exec(db, createSensorTableQuery, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errMsg);
        return 1;
    }

    const char *createTableQuery = 
    "CREATE TABLE IF NOT EXISTS data ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, "  // Chiave primaria automatica
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
    "avg_gas_resistance INTEGER,"                 // Media della resistenza gas
    "eui TEXT FOREIGN KEY REFERENCES sensors(id) "                     // Identificatore EUI in formato testo
    ");";

    int rc = sqlite3_exec(db, createTableQuery, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errMsg);
        return 1;
    }

    return 0;

}


char *Database::InsertData(Payload payload)
{
    char insertQuery[512];
    snprintf(insertQuery, 
    sizeof(insertQuery),
    "INSERT INTO data (pktnum,timestamp,undef,temperature,humidity,ir,vis,batt,avg_temperature,avg_humidity,avg_pressure,avg_gas_resistance,eui) VALUES ('%d','%d','%d','%d','%d','%d','%d','%d','%d','%d','%d','%d','%s');",
     payload.pktnum,payload.timestamp,
     payload.undef,payload.temperature,payload.humidity,
     payload.ir,payload.vis,payload.batt,payload.avg_temperature,
     payload.avg_humidity,payload.avg_pressure,payload.avg_gas_resistance,payload.eui);

    char* errorMessage = nullptr;
    if (sqlite3_exec(db, insertQuery, nullptr, nullptr, &errorMessage) != SQLITE_OK)
    {
        return errorMessage;
    } 
    return nullptr;
}

char *Database::InsertSensor(char *eui)
{
    char* errorMessage = nullptr;
    char insertQuery[512];
    snprintf(insertQuery, 
    sizeof(insertQuery),
    "INSERT INTO sensors (id,state) VALUES ('%s','active');",
     eui);

    if (sqlite3_exec(db, insertQuery, nullptr, nullptr, &errorMessage) != SQLITE_OK)
    {
        return errorMessage;
    } 
    return nullptr;
}

bool Database::CheckNewSensor(char *eui)
{
    char *errorMessage = nullptr;
    char query[512];
    snprintf(query, sizeof(query), "EXISTS(SELECT * FROM sensors WHERE id = '%s');", eui);
    if (sqlite3_exec(db, insertQuery, nullptr, nullptr, &errorMessage) != SQLITE_OK)
    {
        return false;
    } 
    return true;
}


void Database::printError() {
    if (db != nullptr) {
        printf("Errore database: %s\n", sqlite3_errmsg(db));
    } else {
        printf("Il database non è connesso.\n");
    }
}