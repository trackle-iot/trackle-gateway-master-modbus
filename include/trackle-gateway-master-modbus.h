#ifndef GW_MASTER_MODBUS_H_
#define GW_MASTER_MODBUS_H_

#include <driver/uart.h>

#define TRACKLE_GATEWAY_MASTER_MODBUS_VERSION "0.2.0"

void GwMasterModbus_init(uart_port_t uartPort, int txPin, int rxPin, bool usesRs485, int dirPin);
void GwMasterModbus_stop();

#endif
