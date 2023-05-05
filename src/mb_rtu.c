#include "mb_rtu.h"

#include <math.h>

#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <trackle_esp32.h>

#include "sem_utils.h"
#include "str_utils.h"
#include "known_registers.h"

#define PUBLISH_STRING_LEN 2048
#define VALUE_STRING_LEN 128
#define KEYVALUE_STRING_LEN 144

#define MON_REGS_TASK_NAME "mon-regs-task"
#define MON_REGS_TASK_STACKSIZE 8192
#define MON_REGS_TASK_PRIORITY (tskIDLE_PRIORITY + 3)
#define MON_REGS_TASK_CORE_ID 0
#define MON_REGS_TASK_LOOP_MS 1000

#define ROUND_TO_NTH_DECIMAL(v, n) \
    (round(v * pow(10, n)) / pow(10, n))

static const char *TAG = MON_REGS_TASK_NAME;

static SemaphoreHandle_t mbSem;
static StaticSemaphore_t mbSemBuffer;

static TaskHandle_t monRegTaskHandle = NULL;
static StackType_t monRegsTaskStackBuffer[MON_REGS_TASK_STACKSIZE];
static StaticTask_t monRegsTaskBuffer;

static bool startedSuccessfully = false;
static uint16_t mbInterCmdsDelayMs = 10;
static uint8_t mbReadPeriod = 1;

static bool numberToString(uint16_t num, const RegisterAccessData_t *rad, char *valueString, int valueStringBuffLen)
{
    double value;
    if (rad->interpretAsSigned)
        value = *((int16_t *)&num);
    else
        value = num;
    value *= rad->factor;
    value += rad->offset;
    value = ROUND_TO_NTH_DECIMAL(value, rad->decimals);

    char valueStringFmt[10];
    sprintf(valueStringFmt, "%%.%" PRIu8 "f", rad->decimals);
    if (snprintf(valueString, valueStringBuffLen, valueStringFmt, value) > valueStringBuffLen - NULL_CHAR_LEN)
        return false;
    return true;
}

static bool rawToString(uint16_t num, char *valueString, int valueStringBuffLen)
{
    if (snprintf(valueString, valueStringBuffLen, "%" PRIu16, num) > valueStringBuffLen - NULL_CHAR_LEN)
        return false;
    return true;
}

static RegError_t readTypedRegister(RegisterAccessData_t *rad, char *valueString, int valueStringBuffLen)
{
    if (!startedSuccessfully)
        return RegError_MB_NOT_INIT;

    uint16_t rawRegValue = 0;
    if (Trackle_Modbus_execute_command(rad->readFunction, rad->slaveAddr, rad->regId, 1, &rawRegValue) != MODBUS_OK)
    {
        vTaskDelay(mbInterCmdsDelayMs / portTICK_PERIOD_MS);
        return RegError_MB_READ_ERR;
    }

    bool valueFitsString = false;
    switch (rad->type)
    {
    case RADType_NUMBER:
        valueFitsString = numberToString(rawRegValue, rad, valueString, valueStringBuffLen);
        break;
    case RADType_RAW:
        valueFitsString = rawToString(rawRegValue, valueString, valueStringBuffLen);
        break;
    }
    if (!valueFitsString)
        return RegError_STRING_TOO_LONG;

    vTaskDelay(mbInterCmdsDelayMs / portTICK_PERIOD_MS);
    return RegError_OK;
}

static void monitoredRegistersTask(void *args)
{
    static char publishString[PUBLISH_STRING_LEN] = {0};
    TickType_t prevWakeTicks = xTaskGetTickCount();
    BaseType_t xWasDelayed;
    Seconds_t seconds = 0;

    for (;;)
    {
        const int knownRegistersCount = KnownRegisters_count();
        publishString[0] = '\0';
        strcat(publishString, "{");
        int iAdded = 0;
        int i;
        for (i = 0; i < knownRegistersCount; i++)
        {
            // To make error handling lighter inside the "for" body, we simply "break" when an error occurs.
            // Then we detect outside if an error occurred here by checking if all known registers were considered.
            // Place where this is done is marked by a comment in next lines.
            RegisterAccessData_t rad = {0};
            bool mustPublish = false;

            configASSERT(KnownRegisters_at(i, &rad));
            configASSERT(KnownRegisters_getMustPublish(i, &mustPublish));

            if (!rad.monitored)
                continue;

            char valueString[VALUE_STRING_LEN] = {0};

            BLOCKING_LOCK_OR_ABORT(mbSem);
            RegError_t res = readTypedRegister(&rad, valueString, VALUE_STRING_LEN);
            UNLOCK_OR_ABORT(mbSem);

            if (res != RegError_OK)
                continue;

            Seconds_t latestPublishTime = 0;
            configASSERT(KnownRegisters_getLatestPublishedTimeAt(i, &latestPublishTime));
            const char *latestPublishedValue = KnownRegisters_getLatestPublishedValueAt(i);
            if (!(rad.publishOnChange && seconds - latestPublishTime >= rad.changeCheckInterval && !STREQ(latestPublishedValue, valueString)) &&
                !(rad.maxPublishDelay > 0 && seconds - latestPublishTime >= rad.maxPublishDelay) &&
                !(latestPublishTime == 0) &&
                !mustPublish)
                continue;

            if (iAdded > 0)
            {
                if (strlen(publishString) + CT_STRLEN(",") + NULL_CHAR_LEN > PUBLISH_STRING_LEN)
                    break;
                strcat(publishString, ",");
            }

            char keyValueString[KEYVALUE_STRING_LEN] = {0};
            const int kvLen = snprintf(keyValueString, KEYVALUE_STRING_LEN - NULL_CHAR_LEN, "\"%s\":%s", rad.regName, valueString);
            if (kvLen + NULL_CHAR_LEN > KEYVALUE_STRING_LEN)
                break;

            if (strlen(publishString) + strlen(keyValueString) + NULL_CHAR_LEN > PUBLISH_STRING_LEN)
                break;
            strcat(publishString, keyValueString);

            KnownRegisters_setLatestPublishedTimeAt(i, seconds);
            KnownRegisters_setLatestPublishedValueAt(i, valueString);
            KnownRegisters_setMustPublish(i, true);
            iAdded++;
        }
        bool finalBracketFits = false;
        if (strlen(publishString) + CT_STRLEN("}") + NULL_CHAR_LEN <= PUBLISH_STRING_LEN)
        {
            strcat(publishString, "}");
            finalBracketFits = true;
        }

        // We check if an error occurred in the previous for loop with the second term of the condition in next line.
        if (iAdded > 0 && i == knownRegistersCount && finalBracketFits)
        {
            if (tracklePublishSecure("trackle/p", publishString))
            {
                for (int i = 0; i < knownRegistersCount; i++)
                    KnownRegisters_setMustPublish(i, false);
            }
        }

        xWasDelayed = xTaskDelayUntil(&prevWakeTicks, (mbReadPeriod * 1000) / portTICK_PERIOD_MS);
        if (!xWasDelayed)
        {
            tracklePublishSecure("mbTask", "period too short");
        }

        seconds += mbReadPeriod;
    }
}

bool MbRtu_init(uart_port_t uartPort, int baudrate, uint8_t txPin, uint8_t rxPin, bool onRS485, uint8_t dirPin, uint16_t interCmdsDelayMs,
                uint8_t readPeriod, uint8_t serialDataBits, uint8_t serialParity, uint8_t serialStopBits)
{
    // Init semaphore dedicated to modbus
    mbSem = xSemaphoreCreateBinaryStatic(&mbSemBuffer);
    configASSERT(mbSem != NULL);
    configASSERT(xSemaphoreGive(mbSem) == pdTRUE);

    // Set delay between commands
    mbInterCmdsDelayMs = interCmdsDelayMs;

    // Set modbus loop period
    mbReadPeriod = readPeriod;

    // Init modbus library and task
    modbus_config_t mbCfg = {
        .uart_num = uartPort,
        .baud_rate = baudrate,
        .data_bits = serialDataBits,
        .parity = serialParity,
        .stop_bits = serialStopBits,
        .tx_io_num = txPin,
        .rx_io_num = rxPin,
        .rts_io_num = onRS485 ? dirPin : UART_PIN_NO_CHANGE,
        .cts_io_num = UART_PIN_NO_CHANGE,
        .mode = onRS485 ? UART_MODE_RS485_HALF_DUPLEX : UART_MODE_UART,
    };

    if (Trackle_Modbus_init(&mbCfg) == ESP_OK)
    {
        monRegTaskHandle = xTaskCreateStaticPinnedToCore(monitoredRegistersTask,
                                                         MON_REGS_TASK_NAME,
                                                         MON_REGS_TASK_STACKSIZE,
                                                         NULL,
                                                         MON_REGS_TASK_PRIORITY,
                                                         monRegsTaskStackBuffer,
                                                         &monRegsTaskBuffer,
                                                         MON_REGS_TASK_CORE_ID);
        if (monRegTaskHandle != NULL)
        {
            startedSuccessfully = true;
            return true;
        }
    }
    return false;
}

bool MbRtu_wasStartedSuccesfully()
{
    return startedSuccessfully;
}

RegError_t MbRtu_readTypedRegisterByName(char *regName, char *valueString, int valueStringBuffLen)
{
    BLOCKING_LOCK_OR_ABORT(mbSem);

    RegisterAccessData_t rad = {0};
    if (!KnownRegisters_find(regName, &rad))
    {
        UNLOCK_OR_ABORT(mbSem);
        return RegError_NOT_FOUND;
    }

    const RegError_t regError = readTypedRegister(&rad, valueString, valueStringBuffLen);

    UNLOCK_OR_ABORT(mbSem);
    return regError;
}

bool MbRtu_readAllRegistersJson(char *publishString, int publishStringMaxLen)
{

    const int knownRegistersCount = KnownRegisters_count();
    publishString[0] = '\0';
    strcat(publishString, "{");
    int iAdded = 0;
    for (int i = 0; i < knownRegistersCount; i++)
    {
        RegisterAccessData_t rad = {0};

        configASSERT(KnownRegisters_at(i, &rad));

        char valueString[VALUE_STRING_LEN] = {0};

        BLOCKING_LOCK_OR_ABORT(mbSem);
        readTypedRegister(&rad, valueString, VALUE_STRING_LEN);
        UNLOCK_OR_ABORT(mbSem);

        if (iAdded > 0)
        {
            if (strlen(publishString) + CT_STRLEN(",") + NULL_CHAR_LEN > publishStringMaxLen)
            {
                return false;
            }
            strcat(publishString, ",");
        }

        char keyValueString[KEYVALUE_STRING_LEN] = {0};
        const int kvLen = snprintf(keyValueString, KEYVALUE_STRING_LEN - NULL_CHAR_LEN, "\"%s\":%s", rad.regName, valueString);
        if (kvLen + NULL_CHAR_LEN > KEYVALUE_STRING_LEN)
        {
            return false;
        }

        if (strlen(publishString) + strlen(keyValueString) + NULL_CHAR_LEN > publishStringMaxLen)
        {
            return false;
        }
        strcat(publishString, keyValueString);

        iAdded++;
    }
    if (strlen(publishString) + CT_STRLEN("}") + NULL_CHAR_LEN > publishStringMaxLen)
    {
        return false;
    }
    strcat(publishString, "}");

    return true;
}

static RegError_t numberStringToRaw(const RegisterAccessData_t *rad, const char *valueString, uint16_t *rawRegValue)
{
    if (!strContainsValidDouble(valueString))
        return RegError_STRING_NOT_DOUBLE;
    double value = 0;
    sscanf(valueString, "%lf", &value);
    value -= rad->offset;
    value /= rad->factor;
    const long long roundValue = llround(value);
    if (rad->interpretAsSigned)
    {
        if (roundValue > INT16_MAX || roundValue < INT16_MIN)
            return RegError_CANT_REPRESENT_WITH_INT16;
        const int16_t signedRawRegValue = (int16_t)roundValue;
        *rawRegValue = *((uint16_t *)&signedRawRegValue);
    }
    else
    {
        if (roundValue > UINT16_MAX || roundValue < 0)
            return RegError_CANT_REPRESENT_WITH_UINT16;
        *rawRegValue = (uint16_t)roundValue;
    }

    return RegError_OK;
}

static RegError_t rawStringToRaw(char *valueString, uint16_t *rawRegValue)
{
    if (!strContainsOnlyDigits(valueString) || !strValLessThan(valueString, MAX_U16_STR))
        return RegError_CANT_REPRESENT_WITH_UINT16;
    sscanf(valueString, "%" PRIu16, rawRegValue);
    return RegError_OK;
}

RegError_t MbRtu_writeTypedRegisterByName(char *regName, char *valueString)
{
    BLOCKING_LOCK_OR_ABORT(mbSem);

    RegisterAccessData_t rad = {0};
    if (!KnownRegisters_find(regName, &rad))
    {
        UNLOCK_OR_ABORT(mbSem);
        return RegError_NOT_FOUND;
    }

    if (!startedSuccessfully)
    {
        UNLOCK_OR_ABORT(mbSem);
        return RegError_MB_NOT_INIT;
    }

    uint16_t rawRegValue = 0;
    RegError_t regError = RegError_OK;
    switch (rad.type)
    {
    case RADType_NUMBER:
        regError = numberStringToRaw(&rad, valueString, &rawRegValue);
        break;
    case RADType_RAW:
        regError = rawStringToRaw(valueString, &rawRegValue);
        break;
    }
    if (regError != RegError_OK)
    {
        UNLOCK_OR_ABORT(mbSem);
        return regError;
    }

    if (!rad.writable)
    {
        UNLOCK_OR_ABORT(mbSem);
        return RegError_REG_NOT_WRITABLE;
    }

    if (Trackle_Modbus_execute_command(rad.writeFunction, rad.slaveAddr, rad.regId, 1, &rawRegValue) != MODBUS_OK)
    {
        vTaskDelay(mbInterCmdsDelayMs / portTICK_PERIOD_MS);
        UNLOCK_OR_ABORT(mbSem);
        return RegError_MB_WRITE_ERR;
    }

    vTaskDelay(mbInterCmdsDelayMs / portTICK_PERIOD_MS);
    UNLOCK_OR_ABORT(mbSem);
    return RegError_OK;
}

RegError_t MbRtu_readRawRegisterByAddr(uint8_t readFunction, uint8_t slaveAddr, uint16_t regId, uint16_t *value)
{
    BLOCKING_LOCK_OR_ABORT(mbSem);

    if (!startedSuccessfully)
    {
        UNLOCK_OR_ABORT(mbSem);
        return RegError_MB_NOT_INIT;
    }

    if (Trackle_Modbus_execute_command(readFunction, slaveAddr, regId, 1, value) != MODBUS_OK)
    {
        vTaskDelay(mbInterCmdsDelayMs / portTICK_PERIOD_MS);
        UNLOCK_OR_ABORT(mbSem);
        return RegError_MB_READ_ERR;
    }

    vTaskDelay(mbInterCmdsDelayMs / portTICK_PERIOD_MS);
    UNLOCK_OR_ABORT(mbSem);
    return RegError_OK;
}

RegError_t MbRtu_writeRawRegisterByAddr(uint8_t writeFunction, uint8_t slaveAddr, uint16_t regId, uint16_t value)
{
    BLOCKING_LOCK_OR_ABORT(mbSem);

    if (!startedSuccessfully)
    {
        UNLOCK_OR_ABORT(mbSem);
        return RegError_MB_NOT_INIT;
    }

    if (Trackle_Modbus_execute_command(writeFunction, slaveAddr, regId, 1, &value) != MODBUS_OK)
    {
        vTaskDelay(mbInterCmdsDelayMs / portTICK_PERIOD_MS);
        UNLOCK_OR_ABORT(mbSem);
        return RegError_MB_WRITE_ERR;
    }

    vTaskDelay(mbInterCmdsDelayMs / portTICK_PERIOD_MS);
    UNLOCK_OR_ABORT(mbSem);
    return RegError_OK;
}

void MbRtu_stop()
{
    BLOCKING_LOCK_OR_ABORT(mbSem);
}

ModbusError MbRtu_forwardRequestToSlaves(TrackleModbusFunction function, uint8_t slaveAddr, uint16_t regId, uint16_t size, void *value)
{
    BLOCKING_LOCK_OR_ABORT(mbSem);

    ModbusError err = Trackle_Modbus_execute_command(function, slaveAddr, regId, size, value);
    vTaskDelay(mbInterCmdsDelayMs / portTICK_PERIOD_MS);

    UNLOCK_OR_ABORT(mbSem);
    return err;
}
