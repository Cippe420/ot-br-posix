#ifndef PAYLOADREADER_HPP
#define PAYLOADREADER_HPP

#include <cstdint>
#include <cstddef>
struct Payload{
                    // maybe not uint8
                    char eui[17];
                    uint16_t pktnum;
                    uint16_t timestamp;
                    uint16_t undef;
                    uint16_t temperature;
                    uint16_t humidity;
                    uint16_t ir;
                    uint16_t vis;
                    uint16_t batt;
                    // need the average of those,really
                    uint16_t avg_temperature;
                    uint16_t avg_humidity;
                    uint16_t avg_pressure;
                    uint16_t avg_gas_resistance;

};


uint16_t bytesToDecimal(unsigned char* data, uint16_t length);

uint16_t extractNumber(unsigned char* number,uint16_t &start, uint16_t length);

void extractString(const unsigned char* payload, uint16_t &start_byte, uint16_t length, char* output);

#endif