#ifndef NVS_FW_CFG_H_
#define NVS_FW_CFG_H_

#include <esp_types.h>
#include <driver/uart.h>

typedef struct FirmwareConfig_s
{
    uint8_t fwVersion;
    int32_t modbusBaudrate;
    uint16_t modbusInterCmdsDelayMs;
    uint16_t knownRegistersAtStartup;
    uint8_t modbusReadPeriod;
    uint8_t serialDataBits;
    uint8_t serialStopBits;
    uint8_t serialParity;
    uint8_t bitPosition;
} FirmwareConfig_t;

bool NvsFwCfg_loadFromNvs();
bool NvsFwCfg_setMbBaudrate(int32_t baudrate);
bool NvsFwCfg_setInterCmdsDelayMs(uint16_t delay);
void NvsFwCfg_setMbReadPeriod(uint8_t period);
void NvsFwCfg_getActualFirmwareConfig(FirmwareConfig_t *fwConfig);
void NvsFwCfg_getNextFirmwareConfig(FirmwareConfig_t *fwConfig);
bool NvsFwCfg_saveToNvs();
void NvsFwCfg_setMbParity(uart_parity_t parity);
void NvsFwCfg_setMbStopBits(uart_stop_bits_t stopBits);
void NvsFwCfg_setMbDataBits(uart_word_length_t stopBits);
void NvsFwCfg_setMbBitPosition(int8_t bitPosition);

#endif
