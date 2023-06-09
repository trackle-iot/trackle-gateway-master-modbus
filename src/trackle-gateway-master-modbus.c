#include "trackle-gateway-master-modbus.h"

#include <esp_log.h>

#include "known_registers.h"
#include "nvs_fw_cfg.h"
#include "mb_rtu.h"
#include "cloud_cb.h"

static const char *TAG = "gw-master-mb";

void GwMasterModbus_init(uart_port_t uartPort, int txPin, int rxPin, bool usesRs485, int dirPin, void (*mbReqFailedCallback)())
{
    FirmwareConfig_t fwConfig = {0};

    KnownRegisters_init();

    if (NvsFwCfg_loadFromNvs())
        ESP_LOGE(TAG, "Config loaded from NVS");
    else
        ESP_LOGE(TAG, "Config not found in NVS, using default");

    NvsFwCfg_getActualFirmwareConfig(&fwConfig);

    if (!MbRtu_init(uartPort,
                    fwConfig.modbusBaudrate,
                    txPin,
                    rxPin,
                    usesRs485,
                    dirPin,
                    fwConfig.modbusInterCmdsDelayMs,
                    fwConfig.modbusReadPeriod,
                    fwConfig.serialDataBits,
                    fwConfig.serialParity,
                    fwConfig.serialStopBits,
                    fwConfig.bitPosition,
                    mbReqFailedCallback))
        ESP_LOGE(TAG, "Invalid modbus parameters. Modbus not started");

    CloudCb_registerCallbacks();
}

void GwMasterModbus_stop()
{
    MbRtu_stop();
}

bool GwMasterModbus_saveConfigToFlash()
{
    return NvsFwCfg_saveToNvs();
}

ModbusError GwMasterModbus_forwardMbReqToSlaves(TrackleModbusFunction function, uint8_t slaveAddr, uint16_t regId, uint16_t size, void *value)
{
    return MbRtu_forwardRequestToSlaves(function, slaveAddr, regId, size, value);
}
