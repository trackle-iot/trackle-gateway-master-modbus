#ifndef KNOWN_REGISTERS_H_
#define KNOWN_REGISTERS_H_

#include "register_access_data.h"

#define MAX_REGISTERS_NUM 60

void KnownRegisters_init();
bool KnownRegisters_remove(char *regName);
bool KnownRegisters_add(const RegisterAccessData_t *rad);
int KnownRegisters_count();
void KnownRegisters_clear();
bool KnownRegisters_find(char *regName, RegisterAccessData_t *radOut);
bool KnownRegisters_findByModbus(uint8_t readFunction, uint8_t slaveAddr, uint16_t regId, RegisterAccessData_t *radOut);
bool KnownRegisters_at(int idx, RegisterAccessData_t *radOut);
bool KnownRegisters_setMonitored(char *regName, bool monitored);
bool KnownRegisters_setWritable(char *regName, bool writable, uint8_t writeFunction);
bool KnownRegisters_setInterpretedAsSigned(char *regName, bool asSigned);
bool KnownRegisters_setFactor(char *regName, double factor);
bool KnownRegisters_setOffset(char *regName, double offset);
bool KnownRegisters_setDecimals(char *regName, uint8_t decimals);
bool KnownRegisters_setLength(char *regName, uint8_t length);
bool KnownRegisters_setOnChange(char *regName, bool onChange);
bool KnownRegisters_setChangeCheckInterval(char *regName, Seconds_t changeCheckInterval);
bool KnownRegisters_setMaxPublishDelay(char *regName, Seconds_t maxPublishDelay);
const char *KnownRegisters_getLatestPublishedValueAt(int idx);
bool KnownRegisters_setLatestPublishedValueAt(int idx, const char *value);
bool KnownRegisters_getLatestPublishedTimeAt(int idx, Seconds_t *latestPublish);
bool KnownRegisters_setLatestPublishedTimeAt(int idx, Seconds_t latestPublishedTime);
bool KnownRegisters_getMustPublish(int idx, bool *mustPublish);
bool KnownRegisters_setMustPublish(int idx, bool mustPublish);

#endif
