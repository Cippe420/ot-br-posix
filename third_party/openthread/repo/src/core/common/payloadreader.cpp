#include "payloadreader.hpp"
#include "endian.h"
#include <iostream>
#include <fstream>
#include <string>

uint16_t bytesToDecimal(unsigned char* data, uint16_t length) {
    uint16_t result=0;
    for (size_t i = 0; i < length; ++i) {
            result = (result << 8) | data[i]; // Shift a sinistra di 8 bit e aggiungi il byte corrente
        }
        return result;
}

// Funzione per estrarre e convertire una porzione del payload in decimale
uint16_t extractNumber(unsigned char* number,uint16_t *start,uint16_t length) {
    // convert an array of byte into the appropriate number
    uint16_t result;

    unsigned char ltlEndian[length];

    for (int i = 0; i < length ; i++)
    {
        ltlEndian[i] = number[*start + length - i - 1];
    }

    result = bytesToDecimal(ltlEndian,length);

    *start += length;

    return result;

}

// Funzione per estrarre una stringa leggendo direttamente dall'array di byte
void extractString(const unsigned char* payload, uint16_t *start_byte, uint16_t length, unsigned char* output) {
    
    std::ofstream file;

    file.open("/home/pi/log.txt");
    for (size_t i = 0; i < length; ++i) {
        output[i] = payload[*start_byte + i];
    }
    output[length] = '\0';
    
    // Terminatore null
    *start_byte += length;  // Aggiorna l'indice di partenza

    file.close();
}

uint64_t extractEui64(unsigned char * number, uint16_t *start,uint16_t length)
{
    uint64_t result;
    unsigned char ltlEndian[length];
    for (int i = 0; i < length ; i++)
    {
        ltlEndian[i] = number[*start + length - i - 1];
    }
    result = bytesToDecimal64(ltlEndian,length);
    *start += length;

    return result;
}

uint64_t bytesToDecimal64(unsigned char* data, uint16_t length) {
    uint64_t result=0;
    for (size_t i = 0; i < length; ++i) {
            result = (result << 8) | data[i]; // Shift a sinistra di 8 bit e aggiungi il byte corrente
        }
        return result;
}

