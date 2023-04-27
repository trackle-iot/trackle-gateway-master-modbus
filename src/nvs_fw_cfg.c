#include "nvs_fw_cfg.h"

#include <esp_log.h>
#include <nvs_flash.h>
#include <driver/uart.h>

#include "register_access_data.h"
#include "known_registers.h"

#define NVS_GATEWAY_FW_CFG_NAMESPACE "gateway-fw-cfg"
#define NVS_FIRMWARE_CONFIG_STRUCT_KEY "firmware-config"

#define DEFAULT_FIRMWARE_CONFIG              \
    {                                        \
        .fwVersion = FIRMWARE_VERSION,       \
        .modbusBaudrate = 9600,              \
        .modbusInterCmdsDelayMs = 50,        \
        .knownRegistersAtStartup = 0,        \
        .modbusReadPeriod = 1,               \
        .serialDataBits = UART_DATA_8_BITS,  \
        .serialParity = UART_PARITY_DISABLE, \
        .serialStopBits = UART_STOP_BITS_1   \
    }

static const char *TAG = "nvs_fw_cfg";

static FirmwareConfig_t actualFirmwareConfig = DEFAULT_FIRMWARE_CONFIG;
static FirmwareConfig_t nextFirmwareConfig = DEFAULT_FIRMWARE_CONFIG;

bool NvsFwCfg_loadFromNvs()
{
    FirmwareConfig_t loadedFirmwareConfig;

    // Open NVS
    nvs_handle_t nvsHandle = NULL;
    esp_err_t err = nvs_open(NVS_GATEWAY_FW_CFG_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK)
        return false;

    // Read main structure
    size_t structSize = sizeof(FirmwareConfig_t);
    err = nvs_get_blob(nvsHandle, NVS_FIRMWARE_CONFIG_STRUCT_KEY, &loadedFirmwareConfig, &structSize);
    if (err != ESP_OK)
    {
        nvs_close(nvsHandle);
        return false;
    }

    // Read registers access data
    int i;
    for (i = 0; i < loadedFirmwareConfig.knownRegistersAtStartup; i++)
    {
        char key[10] = {0};
        sprintf(key, "rad%d", i);
        structSize = sizeof(RegisterAccessData_t);
        RegisterAccessData_t rad = {0};
        err = nvs_get_blob(nvsHandle, key, &rad, &structSize);
        rad.regName[MAX_REG_NAME_SIZE - 1] = '\0';

        if (err != ESP_OK)
            break;
        if (!KnownRegisters_add(&rad))
            break;
    }

    // Close NVS and clean read RADs if errors occurred while reading
    nvs_close(nvsHandle);
    if (i != loadedFirmwareConfig.knownRegistersAtStartup)
    {
        KnownRegisters_clear();
        return false;
    }

    // Copy read configuration if success
    actualFirmwareConfig = loadedFirmwareConfig;
    nextFirmwareConfig = loadedFirmwareConfig;
    return true;
}

bool NvsFwCfg_setMbBaudrate(int32_t baudrate)
{
    if (baudrate <= 0)
        return false;
    nextFirmwareConfig.modbusBaudrate = baudrate;
    return true;
}

bool NvsFwCfg_setInterCmdsDelayMs(uint16_t delay)
{
    if (delay <= 0)
        return false;
    nextFirmwareConfig.modbusInterCmdsDelayMs = delay;
    return true;
}

void NvsFwCfg_setMbParity(uart_parity_t parity)
{
    nextFirmwareConfig.serialParity = parity;
}

void NvsFwCfg_setMbStopBits(uart_stop_bits_t stopBits)
{
    nextFirmwareConfig.serialStopBits = stopBits;
}

void NvsFwCfg_setMbDataBits(uart_word_length_t dataBits)
{
    nextFirmwareConfig.serialDataBits = dataBits;
}

void NvsFwCfg_setMbReadPeriod(uint8_t period)
{
    nextFirmwareConfig.modbusReadPeriod = period;
}

void NvsFwCfg_getActualFirmwareConfig(FirmwareConfig_t *fwConfig)
{
    *fwConfig = actualFirmwareConfig;
}

void NvsFwCfg_getNextFirmwareConfig(FirmwareConfig_t *fwConfig)
{
    *fwConfig = nextFirmwareConfig;
}

bool NvsFwCfg_saveToNvs()
{
    // Set number of registers saved to NVS
    const uint16_t knownRegisters = (uint16_t)KnownRegisters_count();
    nextFirmwareConfig.knownRegistersAtStartup = knownRegisters;

    nextFirmwareConfig.fwVersion = FIRMWARE_VERSION;

    // Open NVS
    nvs_handle_t nvsHandle = NULL;
    esp_err_t err = nvs_open(NVS_GATEWAY_FW_CFG_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK)
        return false;

    // Write config structure to NVS
    err = nvs_set_blob(nvsHandle, NVS_FIRMWARE_CONFIG_STRUCT_KEY, &nextFirmwareConfig, sizeof(FirmwareConfig_t));
    if (err != ESP_OK)
        return false;

    // Write registers access data
    int i;
    for (i = 0; i < knownRegisters; i++)
    {
        char key[10] = {0};
        sprintf(key, "rad%d", i);
        RegisterAccessData_t rad = {0};
        if (!KnownRegisters_at(i, &rad))
            break;
        err = nvs_set_blob(nvsHandle, key, &rad, sizeof(RegisterAccessData_t));
        if (err != ESP_OK)
            break;
    }

    // If there was an error writing RADs, invalidate all the saved data, in order not to have an inconsistent state.
    if (i != knownRegisters)
    {
        RegisterAccessData_t radZeroed = {0};
        err = nvs_set_blob(nvsHandle, NVS_FIRMWARE_CONFIG_STRUCT_KEY, &radZeroed, sizeof(FirmwareConfig_t));
        if (err != ESP_OK)
            ESP_LOGE(TAG, "Error zeroing data.");
        return false;
    }

    // Apply changes to NVS
    err = nvs_commit(nvsHandle);
    if (err != ESP_OK)
        return false;

    nvs_close(nvsHandle);
    return true;
}