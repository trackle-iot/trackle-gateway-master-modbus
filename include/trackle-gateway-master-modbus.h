#ifndef GW_MASTER_MODBUS_H_
#define GW_MASTER_MODBUS_H_

#include <driver/uart.h>

#include <trackle_modbus.h>

#define TRACKLE_GATEWAY_MASTER_MODBUS_VERSION "1.1.2"

void GwMasterModbus_init(uart_port_t uartPort, int txPin, int rxPin, bool usesRs485, int dirPin);
void GwMasterModbus_stop();
bool GwMasterModbus_saveConfigToFlash();
ModbusError GwMasterModbus_forwardMbReqToSlaves(TrackleModbusFunction function, uint8_t slaveAddr, uint16_t regId, uint16_t size, void *value);

#endif
