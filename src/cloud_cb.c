#include <string.h>
#include <inttypes.h>
#include <math.h>

#include <esp_log.h>
#include <driver/uart.h>

#include <trackle_esp32.h>

#include "register_access_data.h"
#include "known_registers.h"
#include "nvs_fw_cfg.h"
#include "mb_rtu.h"
#include "str_utils.h"

#include "cloud_cb.h"

#define BOOL2STR(b) \
    (b ? "true" : "false")

#define JSON_ERROR(e) \
    ("{\"error\":" #e "}")

#define ARGS_BUFSIZE 128
#define JSON_BUFSIZE 1024
#define VALUE_STRING_BUFSIZE 128
#define KEYVALUE_STRING_BUFSIZE 144
#define MAX_TOKENS_NUM 5

#define INVALID_CONVERSION 127

static const char *TAG = "cloud_cb";

static int postAddRegister(const char *args)
{
    if (strlen(args) >= ARGS_BUFSIZE)
        return -1;

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return -2;
    case SplitRes_NULL_STRIN:
        return -3;
    default:;
    }

    if (tokensNum != 5)
        return -4;
    RegisterAccessData_t rad = {0};

    if (strlen(tokens[0]) < 1 || strlen(tokens[0]) > MAX_REG_NAME_SIZE - 1)
        return -5;
    strcpy(rad.regName, tokens[0]);

    if (!strContainsOnlyDigits(tokens[1]) || strValLessThan(tokens[1], "0") || strValLessThan("5", tokens[1]))
        return -6;
    sscanf(tokens[1], "%" PRIu8, &rad.readFunction);

    if (!strContainsOnlyDigits(tokens[2]) || !strValLessThan(tokens[2], MAX_U8_STR))
        return -7;
    sscanf(tokens[2], "%" PRIu8, &rad.slaveAddr);

    if (!strContainsOnlyDigits(tokens[3]) || !strValLessThan(tokens[3], MAX_U16_STR))
        return -8;
    sscanf(tokens[3], "%" PRIu16, &rad.regId);

    if (STREQ(tokens[4], TYPE_NUMBER_STR))
    {
        rad.type = RADType_NUMBER;
        rad.factor = 1;
    }
    else if (STREQ(tokens[4], TYPE_RAW_STR))
        rad.type = RADType_RAW;
    else
        return -9;

    if (!KnownRegisters_add(&rad))
        return -10;

    return 1;
}

static int postDeleteRegister(const char *args)
{
    if (!KnownRegisters_remove(args))
        return -1;
    return 1;
}

static int postMonitorRegister(const char *args)
{
    if (strlen(args) >= ARGS_BUFSIZE)
        return -1;

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return -2;
    case SplitRes_NULL_STRIN:
        return -3;
    default:
        if (tokensNum != 2)
            return -4;
    }

    const char *regName = tokens[0];

    bool monitored = false;
    if (STREQ(tokens[1], "false"))
        monitored = false;
    else if (STREQ(tokens[1], "true"))
        monitored = true;
    else
        return -5;

    if (!KnownRegisters_setMonitored(regName, monitored))
        return -6;

    return 1;
}

static int postEnableMonitorOnChange(const char *args)
{
    if (strlen(args) >= ARGS_BUFSIZE)
        return -1;

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return -2;
    case SplitRes_NULL_STRIN:
        return -3;
    default:
        if (tokensNum != 2)
            return -4;
    }

    const char *regName = tokens[0];

    bool onChange = false;
    if (STREQ(tokens[1], "false"))
        onChange = false;
    else if (STREQ(tokens[1], "true"))
        onChange = true;
    else
        return -5;

    if (!KnownRegisters_setOnChange(regName, onChange))
        return -6;

    return 1;
}

static int postSetRegisterChangeCheckInterval(const char *args)
{
    if (strlen(args) >= ARGS_BUFSIZE)
        return -1;

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return -2;
    case SplitRes_NULL_STRIN:
        return -3;
    default:
        if (tokensNum != 2)
            return -4;
    }

    if (!strContainsOnlyDigits(tokens[1]) || !strValLessThan(tokens[1], MAX_U32_STR))
        return -5;

    const char *regName = tokens[0];

    Seconds_t interval = 0;
    sscanf(tokens[1], "%" PRIu32, &interval);

    if (!KnownRegisters_setChangeCheckInterval(regName, interval))
        return -6;

    return 1;
}

static int postSetRegisterMaxPublishDelay(const char *args)
{
    if (strlen(args) >= ARGS_BUFSIZE)
        return -1;

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return -2;
    case SplitRes_NULL_STRIN:
        return -3;
    default:
        if (tokensNum != 2)
            return -4;
    }

    if (!strContainsOnlyDigits(tokens[1]) || !strValLessThan(tokens[1], MAX_U32_STR))
        return -5;

    const char *regName = tokens[0];

    Seconds_t delay = 0;
    sscanf(tokens[1], "%" PRIu32, &delay);

    if (!KnownRegisters_setMaxPublishDelay(regName, delay))
        return -6;

    return 1;
}

static int postMakeRegisterWritable(const char *args)
{
    if (strlen(args) >= ARGS_BUFSIZE)
        return -1;

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return -2;
    case SplitRes_NULL_STRIN:
        return -3;
    default:
        if (tokensNum != 2 && tokensNum != 3)
            return -4;
    }

    const char *regName = tokens[0];

    bool writable = false;
    if (STREQ(tokens[1], "false"))
        writable = false;
    else if (STREQ(tokens[1], "true"))
        writable = true;
    else
        return -5;

    uint8_t writeFunction = 0;
    if (writable)
    {
        if (!strContainsOnlyDigits(tokens[2]) || (atoi(tokens[2]) != 5 && atoi(tokens[2]) != 6 && atoi(tokens[2]) != 15 && atoi(tokens[2]) != 16))
            return -6;
        sscanf(tokens[2], "%" PRIu8, &writeFunction);
    }
    else
    { // no need writeFunction if not writable
        if (tokensNum == 3)
            return -7;
    }

    if (!KnownRegisters_setWritable(regName, writable, writeFunction))
        return -8;

    return 1;
}

static int postMakeRegisterSigned(const char *args)
{
    if (strlen(args) >= ARGS_BUFSIZE)
        return -1;

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return -2;
    case SplitRes_NULL_STRIN:
        return -3;
    default:
        if (tokensNum != 2)
            return -4;
    }

    const char *regName = tokens[0];

    bool interpretAsSigned = false;
    if (STREQ(tokens[1], "false"))
        interpretAsSigned = false;
    else if (STREQ(tokens[1], "true"))
        interpretAsSigned = true;
    else
        return -5;

    if (!KnownRegisters_setInterpretedAsSigned(regName, interpretAsSigned))
        return -6;

    return 1;
}

static int postSetRegisterCoefficients(const char *args)
{
    if (strlen(args) >= ARGS_BUFSIZE)
        return -1;

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return -2;
    case SplitRes_NULL_STRIN:
        return -3;
    default:
        if (tokensNum != 3)
            return -4;
    }

    if (!strContainsValidDouble(tokens[1]))
        return -5;

    if (!strContainsValidDouble(tokens[2]))
        return -6;

    const char *regName = tokens[0];

    double factor = 0;
    sscanf(tokens[1], "%lf", &factor);

    double offset = 0;
    sscanf(tokens[2], "%lf", &offset);

    if (!KnownRegisters_setFactor(regName, factor))
        return -7;

    if (!KnownRegisters_setOffset(regName, offset))
        return -8;

    return 1;
}

static int postSetRegisterOffset(const char *args)
{
    if (strlen(args) >= ARGS_BUFSIZE)
        return -1;

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return -2;
    case SplitRes_NULL_STRIN:
        return -3;
    default:
        if (tokensNum != 2)
            return -4;
    }

    if (!strContainsValidDouble(tokens[1]))
        return -5;

    const char *regName = tokens[0];

    double offset = 0;
    sscanf(tokens[1], "%lf", &offset);

    if (!KnownRegisters_setOffset(regName, offset))
        return -6;

    return 1;
}

static int postSetRegisterDecimals(const char *args)
{
    if (strlen(args) >= ARGS_BUFSIZE)
        return -1;

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return -2;
    case SplitRes_NULL_STRIN:
        return -3;
    default:
        if (tokensNum != 2)
            return -4;
    }

    if (!strContainsOnlyDigits(tokens[1]) || !strValLessThan(tokens[1], MAX_U8_STR))
        return -5;

    const char *regName = tokens[0];

    uint8_t decimals = 0;
    sscanf(tokens[1], "%" PRIu8, &decimals);

    if (!KnownRegisters_setDecimals(regName, decimals))
        return -6;

    return 1;
}

static uart_parity_t stringToParity(char *parity)
{
    // parity err is error
    if (STREQ(parity, "even"))
        return UART_PARITY_EVEN;
    if (STREQ(parity, "odd"))
        return UART_PARITY_ODD;
    if (STREQ(parity, "none"))
        return UART_PARITY_DISABLE;
    return INVALID_CONVERSION;
}

static uart_stop_bits_t doubleToStopBits(double stopBits)
{
    // stop bits max is error
    if (fabs(stopBits - 1) < 0.00001)
        return UART_STOP_BITS_1;
    if (fabs(stopBits - 1.5) < 0.00001)
        return UART_STOP_BITS_1_5;
    if (fabs(stopBits - 2) < 0.00001)
        return UART_STOP_BITS_2;
    return INVALID_CONVERSION;
}

static uart_word_length_t intToDataBits(int dataBits)
{
    // data max is error
    switch (dataBits)
    {
    case 5:
        return UART_DATA_5_BITS;
    case 6:
        return UART_DATA_6_BITS;
    case 7:
        return UART_DATA_7_BITS;
    case 8:
        return UART_DATA_8_BITS;
    default:
        return INVALID_CONVERSION;
    }
}

static int postSetMbConfig(const char *args)
{
    if (strlen(args) >= ARGS_BUFSIZE)
        return -1;

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return -2;
    case SplitRes_NULL_STRIN:
        return -3;
    default:
        if (tokensNum < 1)
            return -4;
    }

    if (!strContainsOnlyDigits(tokens[0]) || !strValLessThan(tokens[0], MAX_I32_STR))
        return -5;
    int32_t baudrate = 0;
    sscanf(tokens[0], "%" PRIi32, &baudrate);
    NvsFwCfg_setMbBaudrate(baudrate);

    if (tokensNum >= 2)
    {
        if (!strContainsOnlyDigits(tokens[1]))
            return -6;
        int32_t dataBitsInt = 0;
        sscanf(tokens[1], "%" PRIi32, &dataBitsInt);
        const uart_word_length_t dataBits = intToDataBits(dataBitsInt);
        if (dataBits == INVALID_CONVERSION)
            return -6;
        NvsFwCfg_setMbDataBits(dataBits);

        if (tokensNum >= 3)
        {
            uart_parity_t parity = stringToParity(tokens[2]);
            if (parity == INVALID_CONVERSION)
                return -7;
            NvsFwCfg_setMbParity(parity);

            if (tokensNum >= 4)
            {
                if (!strContainsValidDouble(tokens[3]))
                    return -8;
                double stopBitsDouble = 0;
                sscanf(tokens[3], "%lf", &stopBitsDouble);
                const uart_stop_bits_t stopBits = doubleToStopBits(stopBitsDouble);
                if (stopBits == INVALID_CONVERSION)
                    return -8;
                NvsFwCfg_setMbStopBits(stopBits);
            }
        }
    }

    return 1;
}

static int postSetMbInterCmdsDelayMs(const char *args)
{
    if (!strContainsOnlyDigits(args) || !strValLessThan(args, MAX_U16_STR))
        return -1;
    uint16_t delay = 0;
    sscanf(args, "%" PRIu16, &delay);

    if (!NvsFwCfg_setInterCmdsDelayMs(delay))
        return -2;

    return 1;
}

static int postSetMbReadPeriod(const char *args)
{
    if (!strContainsOnlyDigits(args) || !strValLessThan(args, MAX_U8_STR))
        return -1;
    uint8_t mbPeriod = 0;
    sscanf(args, "%" PRIu8, &mbPeriod);

    NvsFwCfg_setMbReadPeriod(mbPeriod);
    return 1;
}

static int postSaveConfigToFlash(const char *args)
{
    if (!NvsFwCfg_saveToNvs())
        return -1;
    return 1;
}

static int postWriteRegisterValue(const char *args)
{
    if (strlen(args) >= ARGS_BUFSIZE)
        return -1;

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return -2;
    case SplitRes_NULL_STRIN:
        return -3;
    default:
        if (tokensNum != 2)
            return -4;
    }

    const char *regName = tokens[0];
    const char *valueString = tokens[1];

    switch (MbRtu_writeTypedRegisterByName(regName, valueString))
    {
    case RegError_OK:
        return 1;
    case RegError_NOT_FOUND:
        return -5;
    case RegError_MB_NOT_INIT:
        return -6;
    case RegError_REG_NOT_WRITABLE:
        return -7;
    case RegError_MB_WRITE_ERR:
        return -8;
    case RegError_STRING_NOT_DOUBLE:
        return -9;
    case RegError_CANT_REPRESENT_WITH_INT16:
        return -10;
    case RegError_CANT_REPRESENT_WITH_UINT16:
        return -11;
    default:
        return -12;
    }
}

static int postWriteRawRegisterValue(const char *args)
{
    if (strlen(args) >= ARGS_BUFSIZE)
        return -1;

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return -2;
    case SplitRes_NULL_STRIN:
        return -3;
    default:
        if (tokensNum != 4)
            return -4;
    }

    if (!strContainsOnlyDigits(tokens[0]) || (atoi(tokens[0]) != 5 && atoi(tokens[0]) != 6 && atoi(tokens[0]) != 15 && atoi(tokens[0]) != 16))
        return -5;
    uint8_t writeFunction = 0;
    sscanf(tokens[0], "%" PRIu8, &writeFunction);

    if (!strContainsOnlyDigits(tokens[1]) || !strValLessThan(tokens[1], MAX_U8_STR))
        return -6;
    uint8_t slaveAddr = 0;
    sscanf(tokens[1], "%" PRIu8, &slaveAddr);

    if (!strContainsOnlyDigits(tokens[2]) || !strValLessThan(tokens[2], MAX_U16_STR))
        return -7;
    uint16_t regId = 0;
    sscanf(tokens[2], "%" PRIu16, &regId);

    if (!strContainsOnlyDigits(tokens[3]) || !strValLessThan(tokens[3], MAX_U16_STR))
        return -8;
    uint16_t rawValue = 0;
    sscanf(tokens[3], "%" PRIu16, &rawValue);

    switch (MbRtu_writeRawRegisterByAddr(writeFunction, slaveAddr, regId, rawValue))
    {
    case RegError_OK:
        return 1;
    case RegError_MB_NOT_INIT:
        return -9;
    case RegError_MB_WRITE_ERR:
        return -10;
    default:
        return -11;
    }
}

static void *getGetRegistersList(const char *args)
{
    static char jsonBuffer[JSON_BUFSIZE] = {0};
    char *json = jsonBuffer;
    json += sprintf(json, "[");

    const int knownRegistersNum = KnownRegisters_count();
    for (int i = 0; i < knownRegistersNum; i++)
    {
        RegisterAccessData_t rad = {0};
        KnownRegisters_at(i, &rad);
        json += sprintf(json, "\"%s\"", rad.regName);
        if (i != knownRegistersNum - 1)
            json += sprintf(json, ",");
    }

    json += sprintf(json, "]");

    return jsonBuffer;
}

static void *getGetRegisterDetails(const char *args)
{
    static char jsonBuffer[JSON_BUFSIZE] = {0};
    char *json = jsonBuffer;
    json += sprintf(json, "{");

    const char *regName = args;

    RegisterAccessData_t rad = {0};
    if (KnownRegisters_find(regName, &rad))
    {
        json += sprintf(json, "\"name\":\"%s\",", rad.regName);
        json += sprintf(json, "\"address\":%" PRIu8 ",", rad.slaveAddr);
        json += sprintf(json, "\"register\":%" PRIu16 ",", rad.regId);
        json += sprintf(json, "\"readFunction\":%" PRIu8 ",", rad.readFunction);
        switch (rad.type)
        {
        case RADType_NUMBER:
            json += sprintf(json, "\"type\":\"" TYPE_NUMBER_STR "\",");
            json += sprintf(json, "\"signed\":%s,", BOOL2STR(rad.interpretAsSigned));
            json += sprintf(json, "\"factor\":%f,", rad.factor);
            json += sprintf(json, "\"offset\":%f,", rad.offset);
            json += sprintf(json, "\"decimals\":%" PRIu8 ",", rad.decimals);
            break;
        case RADType_RAW:
            json += sprintf(json, "\"type\":\"" TYPE_RAW_STR "\",");
            break;
        }
        json += sprintf(json, "\"monitored\":%s,", BOOL2STR(rad.monitored));
        if (rad.monitored)
        {
            json += sprintf(json, "\"maxPublishDelay\":%" PRIu32 ",", rad.maxPublishDelay);
            json += sprintf(json, "\"publishOnChange\":%s,", BOOL2STR(rad.publishOnChange));
            if (rad.publishOnChange)
                json += sprintf(json, "\"changeCheckInterval\":%" PRIu32 ",", rad.changeCheckInterval);
        }
        json += sprintf(json, "\"writable\":%s", BOOL2STR(rad.writable));
        if (rad.writable)
            json += sprintf(json, ",\"writeFunction\":%" PRIu8, rad.writeFunction);
    }

    json += sprintf(json, "}");

    ESP_LOGE(TAG, "%s", jsonBuffer);

    return jsonBuffer;
}

static void *getGetRegisterNameByMbDetails(const char *args)
{
    if (strlen(args) >= ARGS_BUFSIZE)
        return JSON_ERROR("arg too long");

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return JSON_ERROR("too many parameters");
    case SplitRes_NULL_STRIN:
        return JSON_ERROR("null string");
    default:;
    }

    if (tokensNum != 3)
        return JSON_ERROR("wrong number of parameters");

    if (!strContainsOnlyDigits(tokens[0]) || strValLessThan(tokens[0], "0") || strValLessThan("5", tokens[0]))
        return JSON_ERROR("invalid read function");
    uint8_t readFunction = 0;
    sscanf(tokens[0], "%" PRIu8, &readFunction);

    if (!strContainsOnlyDigits(tokens[1]) || !strValLessThan(tokens[1], MAX_U8_STR))
        return JSON_ERROR("invalid slave address");
    uint8_t slaveAddr = 0;
    sscanf(tokens[1], "%" PRIu8, &slaveAddr);

    if (!strContainsOnlyDigits(tokens[2]) || !strValLessThan(tokens[2], MAX_U16_STR))
        return JSON_ERROR("invalid register index");
    uint16_t regId = 0;
    sscanf(tokens[2], "%" PRIu16, &regId);

    static char jsonBuffer[JSON_BUFSIZE] = {0};
    char *json = jsonBuffer;
    json += sprintf(json, "{");

    RegisterAccessData_t rad = {0};
    if (KnownRegisters_findByModbus(readFunction, slaveAddr, regId, &rad))
    {
        json += sprintf(json, "\"name\":\"%s\",", rad.regName);
        json += sprintf(json, "\"address\":%" PRIu8 ",", rad.slaveAddr);
        json += sprintf(json, "\"register\":%" PRIu16, rad.regId);
    }

    json += sprintf(json, "}");

    ESP_LOGE(TAG, "%s", jsonBuffer);

    return jsonBuffer;
}

static char *parityToString(uint8_t parity)
{
    switch (parity)
    {
    case UART_PARITY_DISABLE:
        return "none";
    case UART_PARITY_EVEN:
        return "even";
    case UART_PARITY_ODD:
        return "odd";
    default:
        return "invalid";
    }
}

static double stopBitsToDouble(uint8_t stopBits)
{
    switch (stopBits)
    {
    case UART_STOP_BITS_1:
        return 1.0;
    case UART_STOP_BITS_1_5:
        return 1.5;
    case UART_STOP_BITS_2:
        return 2.0;
    default:
        return -1;
    }
}

static int dataBitsToInt(uint8_t dataBits)
{
    if (dataBits >= UART_DATA_5_BITS && dataBits <= UART_DATA_8_BITS)
        return dataBits - UART_DATA_5_BITS + 5;
    return -1;
}

static void *getGetActualModbusConfig(const char *args)
{
    static char jsonBuffer[JSON_BUFSIZE] = {0};
    char *json = jsonBuffer;
    json += sprintf(json, "{");

    FirmwareConfig_t fwConfig = {0};
    NvsFwCfg_getActualFirmwareConfig(&fwConfig);

    json += sprintf(json, "\"running\":%s,", BOOL2STR(MbRtu_wasStartedSuccesfully()));
    json += sprintf(json, "\"interCmdsDelayMs\":%" PRIu16 ",", fwConfig.modbusInterCmdsDelayMs);
    json += sprintf(json, "\"baudrate\":%" PRIi32 ",", fwConfig.modbusBaudrate);
    json += sprintf(json, "\"readPeriod\":%" PRIu8 ",", fwConfig.modbusReadPeriod);
    json += sprintf(json, "\"dataBits\":%d,", dataBitsToInt(fwConfig.serialDataBits));
    json += sprintf(json, "\"stopBits\":%.2f,", stopBitsToDouble(fwConfig.serialStopBits));
    json += sprintf(json, "\"parity\":\"%s\"", parityToString(fwConfig.serialParity));

    json += sprintf(json, "}");

    ESP_LOGE(TAG, "%s", jsonBuffer);

    return jsonBuffer;
}

static void *getGetNextModbusConfig(const char *args)
{
    static char jsonBuffer[JSON_BUFSIZE] = {0};
    char *json = jsonBuffer;
    json += sprintf(json, "{");

    FirmwareConfig_t fwConfig = {0};
    NvsFwCfg_getNextFirmwareConfig(&fwConfig);

    json += sprintf(json, "\"interCmdsDelayMs\":%" PRIu16 ",", fwConfig.modbusInterCmdsDelayMs);
    json += sprintf(json, "\"baudrate\":%" PRIi32 ",", fwConfig.modbusBaudrate);
    json += sprintf(json, "\"readPeriod\":%" PRIu8 ",", fwConfig.modbusReadPeriod);
    json += sprintf(json, "\"dataBits\":%d,", dataBitsToInt(fwConfig.serialDataBits));
    json += sprintf(json, "\"stopBits\":%.2f,", stopBitsToDouble(fwConfig.serialStopBits));
    json += sprintf(json, "\"parity\":\"%s\"", parityToString(fwConfig.serialParity));

    json += sprintf(json, "}");

    ESP_LOGE(TAG, "%s", jsonBuffer);

    return jsonBuffer;
}

static void *getReadRegisterValue(const char *args)
{
    static char jsonBuffer[JSON_BUFSIZE] = {0};
    char *json = jsonBuffer;
    json += sprintf(json, "{");

    const char *regName = args;

    char valueString[VALUE_STRING_BUFSIZE] = {0};
    if (MbRtu_readTypedRegisterByName(regName, valueString, VALUE_STRING_BUFSIZE) == RegError_OK)
    {
        json += sprintf(json, "\"name\":\"%s\",", regName);
        json += sprintf(json, "\"value\":%s", valueString);
    }

    json += sprintf(json, "}");

    return jsonBuffer;
}

static void *getReadRawRegisterValue(const char *args)
{
    static char jsonBuffer[JSON_BUFSIZE] = {0};

    if (strlen(args) >= ARGS_BUFSIZE)
        return JSON_ERROR("arg too long");

    char argsCpy[ARGS_BUFSIZE];
    strcpy(argsCpy, args);

    char *tokens[MAX_TOKENS_NUM] = {0};
    int tokensNum = 0;
    switch (splitInPlace(argsCpy, ',', tokens, MAX_TOKENS_NUM, &tokensNum))
    {
    case SplitRes_TOO_MANY_PARAMS:
        return JSON_ERROR("too many parameters");
    case SplitRes_NULL_STRIN:
        return JSON_ERROR("null string");
    default:
        if (tokensNum != 3)
            return JSON_ERROR("wrong number of parameters");
    }

    if (!strContainsOnlyDigits(tokens[0]) || strValLessThan(tokens[0], "0") || strValLessThan("5", tokens[0]))
        return JSON_ERROR("invalid read function");
    uint8_t readFunction = 0;
    sscanf(tokens[0], "%" PRIu8, &readFunction);

    if (!strContainsOnlyDigits(tokens[1]) || !strValLessThan(tokens[1], MAX_U8_STR))
        return JSON_ERROR("invalid slave address");
    uint8_t slaveAddr = 0;
    sscanf(tokens[1], "%" PRIu8, &slaveAddr);

    if (!strContainsOnlyDigits(tokens[2]) || !strValLessThan(tokens[2], MAX_U16_STR))
        return JSON_ERROR("invalid register index");
    uint16_t regId = 0;
    sscanf(tokens[2], "%" PRIu16, &regId);

    char *json = jsonBuffer;
    json += sprintf(json, "{");

    uint16_t rawValue = 0;
    switch (MbRtu_readRawRegisterByAddr(readFunction, slaveAddr, regId, &rawValue))
    {
    case RegError_OK:
        json += sprintf(json, "\"readFunction\":%" PRIu8 ",", readFunction);
        json += sprintf(json, "\"address\":%" PRIu8 ",", slaveAddr);
        json += sprintf(json, "\"register\":%" PRIu16 ",", regId);
        json += sprintf(json, "\"value\":%" PRIu16, rawValue);
        break;
    case RegError_MB_NOT_INIT:
        return JSON_ERROR("modbus not running");
    case RegError_MB_READ_ERR:
        return JSON_ERROR("modbus read failed");
    default:
        return JSON_ERROR("internal error");
    }

    json += sprintf(json, "}");

    return jsonBuffer;
}

static void *getReadAllRegistersValues(const char *args)
{
    static char jsonBuffer[JSON_BUFSIZE] = {0};
    char *json = jsonBuffer;

    if (!MbRtu_readAllRegistersJson(jsonBuffer, JSON_BUFSIZE))
        return "{}";

    return jsonBuffer;
}

static void *getGetAllMonitoredRegistersLatestValues(const char *args)
{
    static char jsonBuffer[JSON_BUFSIZE] = {0};
    const int knownRegistersCount = KnownRegisters_count();
    jsonBuffer[0] = '\0';
    strcat(jsonBuffer, "{");
    int iAdded = 0;
    int i;
    for (i = 0; i < knownRegistersCount; i++)
    {
        RegisterAccessData_t rad = {0};

        configASSERT(KnownRegisters_at(i, &rad));

        if (!rad.monitored)
            continue;

        if (iAdded > 0)
        {
            if (strlen(jsonBuffer) + CT_STRLEN(",") + NULL_CHAR_LEN > JSON_BUFSIZE)
                break;
            strcat(jsonBuffer, ",");
        }

        char *valueString = KnownRegisters_getLatestPublishedValueAt(i);
        configASSERT(valueString != NULL);
        Seconds_t latestPublishedTime = 0;
        configASSERT(KnownRegisters_getLatestPublishedTimeAt(i, &latestPublishedTime));
        if (latestPublishedTime == 0 || STREQ(valueString, ""))
            valueString = "null";

        char keyValueString[KEYVALUE_STRING_BUFSIZE] = {0};
        const int kvLen = snprintf(keyValueString, KEYVALUE_STRING_BUFSIZE - NULL_CHAR_LEN, "\"%s\":%s", rad.regName, valueString);
        if (kvLen + NULL_CHAR_LEN > KEYVALUE_STRING_BUFSIZE)
            break;

        if (strlen(jsonBuffer) + strlen(keyValueString) + NULL_CHAR_LEN > JSON_BUFSIZE)
            break;
        strcat(jsonBuffer, keyValueString);

        iAdded++;
    }
    bool finalBracketFits = false;
    if (strlen(jsonBuffer) + CT_STRLEN("}") + NULL_CHAR_LEN <= JSON_BUFSIZE)
    {
        strcat(jsonBuffer, "}");
        finalBracketFits = true;
    }

    if (i < knownRegistersCount || !finalBracketFits)
        return "{}";

    return jsonBuffer;
}

void CloudCb_registerCallbacks()
{
    tracklePost(trackle_s, "AddRegister", postAddRegister, ALL_USERS);
    tracklePost(trackle_s, "DeleteRegister", postDeleteRegister, ALL_USERS);
    tracklePost(trackle_s, "MonitorRegister", postMonitorRegister, ALL_USERS);
    tracklePost(trackle_s, "EnableMonitorOnChange", postEnableMonitorOnChange, ALL_USERS);
    tracklePost(trackle_s, "SetRegisterChangeCheckInterval", postSetRegisterChangeCheckInterval, ALL_USERS);
    tracklePost(trackle_s, "SetRegisterMaxPublishDelay", postSetRegisterMaxPublishDelay, ALL_USERS);
    tracklePost(trackle_s, "MakeRegisterWritable", postMakeRegisterWritable, ALL_USERS);
    tracklePost(trackle_s, "MakeRegisterSigned", postMakeRegisterSigned, ALL_USERS);
    tracklePost(trackle_s, "WriteRegisterValue", postWriteRegisterValue, ALL_USERS);
    tracklePost(trackle_s, "WriteRawRegisterValue", postWriteRawRegisterValue, ALL_USERS);
    tracklePost(trackle_s, "SetRegisterCoefficients", postSetRegisterCoefficients, ALL_USERS);
    tracklePost(trackle_s, "SetRegisterDecimals", postSetRegisterDecimals, ALL_USERS);
    tracklePost(trackle_s, "SetMbConfig", postSetMbConfig, ALL_USERS);
    tracklePost(trackle_s, "SetMbInterCmdsDelayMs", postSetMbInterCmdsDelayMs, ALL_USERS);
    tracklePost(trackle_s, "SetMbReadPeriod", postSetMbReadPeriod, ALL_USERS);
    tracklePost(trackle_s, "SaveConfigToFlash", postSaveConfigToFlash, ALL_USERS);

    trackleGet(trackle_s, "GetRegistersList", getGetRegistersList, VAR_JSON);
    trackleGet(trackle_s, "GetRegisterDetails", getGetRegisterDetails, VAR_JSON);
    trackleGet(trackle_s, "ReadRegisterValue", getReadRegisterValue, VAR_JSON);
    trackleGet(trackle_s, "ReadRawRegisterValue", getReadRawRegisterValue, VAR_JSON);
    trackleGet(trackle_s, "ReadAllRegistersValues", getReadAllRegistersValues, VAR_JSON);
    trackleGet(trackle_s, "GetAllMonitoredRegistersLatestValues", getGetAllMonitoredRegistersLatestValues, VAR_JSON);
    trackleGet(trackle_s, "GetRegisterNameByMbDetails", getGetRegisterNameByMbDetails, VAR_JSON);
    trackleGet(trackle_s, "GetActualModbusConfig", getGetActualModbusConfig, VAR_JSON);
    trackleGet(trackle_s, "GetNextModbusConfig", getGetNextModbusConfig, VAR_JSON);
}