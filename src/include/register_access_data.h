#ifndef REGISTER_ACCESS_DATA_H_
#define REGISTER_ACCESS_DATA_H_

#include <inttypes.h>
#include <stdbool.h>

#define TYPE_NUMBER_STR "number"
#define TYPE_RAW_STR "raw"
#define TYPE_FLOAT_STR "float"
#define TYPE_STRING_STR "string"

#define MAX_REG_NAME_SIZE 20
#define MAX_REG_LENGTH 10

typedef enum
{
    RADType_NUMBER,
    RADType_RAW,
    RADType_FLOAT,
    RADType_STRING,
} RADType_t;

typedef uint32_t Seconds_t;

typedef struct RegisterAccessData_s
{
    // Generic
    char regName[MAX_REG_NAME_SIZE];
    uint16_t regId;
    uint8_t slaveAddr;
    RADType_t type;
    bool writable;
    uint8_t readFunction;
    uint8_t writeFunction;
    uint8_t regNumber; // default 1, max 4 for number, 2 or 4 for float, 10 for string

    // Monitoring fields
    bool monitored;
    bool publishOnChange;
    Seconds_t changeCheckInterval;
    Seconds_t maxPublishDelay;

    // Number related fields
    bool interpretAsSigned;
    double factor;
    double offset;
    uint8_t decimals;
} RegisterAccessData_t;

#endif
