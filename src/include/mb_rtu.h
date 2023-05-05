#ifndef MB_RTU_H_
#define MB_RTU_H_

#include <esp_types.h>
#include <float.h>
#include <driver/uart.h>

#include <trackle_modbus.h>

typedef enum
{
    RegError_OK = 0,
    RegError_NOT_FOUND,
    RegError_MB_NOT_INIT,
    RegError_MB_READ_ERR,
    RegError_MB_WRITE_ERR,
    RegError_STRING_TOO_LONG,
    RegError_REG_NOT_WRITABLE,
    RegError_CANT_REPRESENT_WITH_UINT16,
    RegError_CANT_REPRESENT_WITH_INT16,
    RegError_CANT_REPRESENT_WITH_UINT32,
    RegError_CANT_REPRESENT_WITH_INT32,
    RegError_CANT_REPRESENT_WITH_UINT64,
    RegError_CANT_REPRESENT_WITH_INT64,
    RegError_CANT_REPRESENT_WITH_FLOAT,
    RegError_CANT_REPRESENT_WITH_DOUBLE,
    RegError_STRING_NOT_DOUBLE,
} RegError_t;

bool MbRtu_init(uart_port_t uartPort, int baudrate, uint8_t txPin, uint8_t rxPin, bool onRS485, uint8_t dirPin, uint16_t mbInterCmdsDelayMs,
                uint8_t mbReadPeriod, uint8_t serialDataBits, uint8_t serialParity, uint8_t serialStopBits, uint8_t bitPosition);
bool MbRtu_wasStartedSuccesfully();
RegError_t MbRtu_readTypedRegisterByName(char *regName, char *valueString, int valueStringLen);
RegError_t MbRtu_readRawRegisterByAddr(uint8_t readFunction, uint8_t slaveAddr, uint16_t regId, uint16_t *value);
RegError_t MbRtu_writeTypedRegisterByName(char *regName, char *valueString);
RegError_t MbRtu_writeRawRegisterByAddr(uint8_t writeFunction, uint8_t slaveAddr, uint16_t regId, uint16_t value);
bool MbRtu_readAllRegistersJson(char *publishString, int publishStringMaxLen);
void MbRtu_stop();
ModbusError MbRtu_forwardRequestToSlaves(TrackleModbusFunction function, uint8_t slaveAddr, uint16_t regId, uint16_t size, void *value);

#endif
