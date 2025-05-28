#include "database.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>


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
    "eui TEXT, FOREIGN KEY(eui) REFERENCES sensors(id) "                     // Identificatore EUI in formato testo
    ");";

    rc = sqlite3_exec(db, createTableQuery, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errMsg);
        return 1;
    }

    return 0;

}


const char *Database::InsertData(Payload payload)
{
    sqlite3_stmt *stmt;
    std::string insertData("INSERT INTO data (pktnum,timestamp,undef,temperature,humidity,ir,vis,batt,avg_temperature,avg_humidity,avg_pressure,avg_gas_resistance,eui) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)");
    if (sqlite3_open("/home/pi/coap.db", &db) != SQLITE_OK) 
    {
            std::cerr << "Errore apertura database: " << sqlite3_errmsg(db) << std::endl;
            return sqlite3_errmsg(db);
    }


    // prepara lo statement
    if (sqlite3_prepare_v2(db, insertData.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Errore preparazione query insertData: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return sqlite3_errmsg(db);
    }
    // binda ogni campo del payload step by step
    std::string tmpEui = std::to_string(payload.eui);
    sqlite3_bind_int(stmt,1,static_cast<int>(payload.pktnum));
    sqlite3_bind_int(stmt,2,static_cast<int>(payload.timestamp));
    sqlite3_bind_int(stmt,3,static_cast<int>(payload.undef));
    sqlite3_bind_int(stmt,4,static_cast<int>(payload.temperature));
    sqlite3_bind_int(stmt,5,static_cast<int>(payload.humidity));
    sqlite3_bind_int(stmt,6,static_cast<int>(payload.ir));
    sqlite3_bind_int(stmt,7,static_cast<int>(payload.vis));
    sqlite3_bind_int(stmt,8,static_cast<int>(payload.batt));
    sqlite3_bind_int(stmt,9,static_cast<int>(payload.avg_temperature));
    sqlite3_bind_int(stmt,10,static_cast<int>(payload.avg_humidity));
    sqlite3_bind_int(stmt,11,static_cast<int>(payload.avg_pressure));
    sqlite3_bind_int(stmt,12,static_cast<int>(payload.avg_gas_resistance));
    sqlite3_bind_text(stmt,13, tmpEui.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        std::cerr << "Errore esecuzione query: " << sqlite3_errmsg(db) << std::endl;
        return sqlite3_errmsg(db);
    } 
    else 
    {
        std::cout << "Data inserita con successo!" << std::endl;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return nullptr;
}

void Database::InsertSensor(uint16_t eui)
{
    sqlite3_stmt *stmt;

    std::string sql("INSERT INTO sensors(id,state) values(?,'active')");
    if (sqlite3_open("/home/pi/coap.db", &db) != SQLITE_OK) 
    {
            std::cerr << "Errore apertura database: " << sqlite3_errmsg(db) << std::endl;
            return;
    }

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Errore preparazione query insertSensor: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return;
    }

    // binda ogni campo del payload step by step
    std::string tmpEui = std::to_string(eui);
    sqlite3_bind_text(stmt, 1, tmpEui.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Errore esecuzione query: " << sqlite3_errmsg(db) << std::endl;
    } else {
        std::cout << "Sensore inserito con successo!" << std::endl;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return;
}

bool Database::CheckNewSensor(uint16_t eui)
{
    sqlite3_stmt *stmt;
    std::string statement("SELECT EXISTS(SELECT 1 FROM sensors WHERE id = ?);");

    //std::cerr << "entrato in checknewsensor\n "  << std::endl;

    if (sqlite3_open("/home/pi/coap.db", &db) != SQLITE_OK) {
        std::cerr << "Errore apertura database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    if (sqlite3_prepare_v2(db, statement.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Errore preparazione query exists: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    //std::cerr << "convertendo tmpEUI\n "  << std::endl;
    // binda ogni campo del payload step by step
    std::string tmpEui = std::to_string(eui);

    std::cerr << "Eui dentro la query checknewsensor  " << tmpEui.c_str() << std::endl;
    sqlite3_bind_text(stmt, 1, tmpEui.c_str(), -1, SQLITE_STATIC);

    //std::cerr << "bindato tmpEUI\n "  << std::endl;

    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = sqlite3_column_int(stmt, 0);  
        std::cerr << "eseguita statement, con risultato \n " <<std::boolalpha << exists << std::endl;    
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return exists;
}
// TODO:  check correctness
std::vector<uint64_t> Database::GetEuiSensors()
{
    sqlite3_stmt *stmt;
    std::string statement("SELECT id FROM sensors;");
    std::vector<uint64_t> euis;

    if (sqlite3_open("/home/pi/coap.db", &db) != SQLITE_OK) {
        std::cerr << "Errore apertura database: " << sqlite3_errmsg(db) << std::endl;
        return euis;
    }

    if (sqlite3_prepare_v2(db, statement.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Errore preparazione query getEuiSensors: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return euis;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        uint64_t eui = sqlite3_column_int64(stmt, 0);
        euis.push_back(eui);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return euis;
}

//TODO: automatically sets the table,remove the packet routine
void Database::SetSensorsState(std::vector<uint16_t> devicesMrloc16)
{
    sqlite3_stmt *stmt;
    std::string statement("UPDATE sensors SET state = 'dead' WHERE id NOT IN (");

    if (sqlite3_open("/home/pi/coap.db", &db) != SQLITE_OK) {
        std::cerr << "Errore apertura database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    for(size_t i = 0; i < devicesMrloc16.size(); i++)
    {
        statement += std::to_string(devicesMrloc16[i]);
        if (CheckNewSensor(devicesMrloc16[i]) == false)
        {
            InsertSensor(devicesMrloc16[i]);
        }else
        {
            // Set sensor state to active
        }

        if (i != devicesMrloc16.size() - 1) {
            statement += ",";
        }

    }
    statement += ");";
    std::cerr << "query da eseguire: " << statement << std::endl;

    if (sqlite3_prepare_v2(db, statement.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "Errore preparazione query setSensorsState: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return;
    }

    if (sqlite3_exec(db, statement.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::cerr << "Errore esecuzione query setSensorsState: " << sqlite3_errmsg(db) << std::endl;
    } else {
        std::cout << "Stato dei sensori aggiornato con successo!" << std::endl;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);


}

void Database::printError() {
    if (db != nullptr) {
        printf("Errore database: %s\n", sqlite3_errmsg(db));
    } else {
        printf("Il database non è connesso.\n");
    }
}
