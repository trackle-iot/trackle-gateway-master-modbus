#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>

#include <sys/queue.h>

#include <esp_log.h>

#include "known_registers.h"
#include "str_utils.h"

// BEGIN ----------------------------------------------- QUEUE TYPES DEFINITIONS -----------------------------------------------------------

#define MAX_LATEST_PUBLISHED_SIZE 24

typedef struct Slot_s
{
    // Register details (saved to flash)
    RegisterAccessData_t rad;

    // Current execution details (NOT saved to flash)
    char latestPublishedValue[MAX_LATEST_PUBLISHED_SIZE];
    Seconds_t latestPublishSec;
    bool mustPublish;

    // Structure holding pointers to other elements of the queue
    STAILQ_ENTRY(Slot_s)
    ptrs;
} Slot_t;

typedef STAILQ_HEAD(SlotsQueue_s, Slot_s) SlotsQueue_t;

// BEGIN --------------------------------------------------- STATIC DECLARATIONS -----------------------------------------------------------

// Queues
static SlotsQueue_t availableSlots;
static SlotsQueue_t inUseSlots;

// Raw static memory allocation
static Slot_t slotsMemoryPool[MAX_REGISTERS_NUM];

// BEGIN ---------------------------------------------------- QUEUES FUNCTIONS --------------------------------------------------------------

static void queuePush(SlotsQueue_t *queue, Slot_t *element)
{
    STAILQ_INSERT_TAIL(queue, element, ptrs);
}

static Slot_t *queueRemove(SlotsQueue_t *queue, char *regName)
{
    Slot_t *slot = NULL;
    STAILQ_FOREACH(slot, queue, ptrs)
    {
        if (STREQ(slot->rad.regName, regName))
            break;
    }

    if (slot != NULL)
        STAILQ_REMOVE(queue, slot, Slot_s, ptrs);
    return slot;
}

static bool queueIsEmpty(SlotsQueue_t *queue)
{
    return STAILQ_EMPTY(queue);
}

static Slot_t *queuePop(SlotsQueue_t *queue)
{
    Slot_t *slot = STAILQ_FIRST(queue);
    if (slot != NULL)
        STAILQ_REMOVE_HEAD(queue, ptrs);
    return slot;
}

static int queueSize(SlotsQueue_t *queue)
{
    int size = 0;
    Slot_t *slot = NULL;
    STAILQ_FOREACH(slot, queue, ptrs)
    {
        size++;
    }
    return size;
}

static RegisterAccessData_t *queueFind(SlotsQueue_t *queue, char *regName)
{
    Slot_t *slot = NULL;
    STAILQ_FOREACH(slot, queue, ptrs)
    {
        if (STREQ(slot->rad.regName, regName))
            return &slot->rad;
    }
    return NULL;
}

static RegisterAccessData_t *queueFindByModbus(SlotsQueue_t *queue, uint8_t readFunction, uint8_t slaveAddr, uint16_t regId)
{
    Slot_t *slot = NULL;
    STAILQ_FOREACH(slot, queue, ptrs)
    {
        if (slot->rad.readFunction == readFunction && slot->rad.slaveAddr == slaveAddr && slot->rad.regId == regId)
            return &slot->rad;
    }
    return NULL;
}

static RegisterAccessData_t *queueAt(SlotsQueue_t *queue, int idx)
{
    Slot_t *slot = STAILQ_FIRST(queue);
    for (int i = 0; slot != NULL; i++)
    {
        if (i == idx)
            return &slot->rad;
        slot = STAILQ_NEXT(slot, ptrs);
    }
    return NULL;
}

static Slot_t *queueSlotAt(SlotsQueue_t *queue, int idx)
{
    Slot_t *slot = STAILQ_FIRST(queue);
    for (int i = 0; slot != NULL; i++)
    {
        if (i == idx)
            return slot;
        slot = STAILQ_NEXT(slot, ptrs);
    }
    return NULL;
}

// BEGIN ---------------------------------------------------- PUBLIC FUNCTIONS --------------------------------------------------------------

void KnownRegisters_init()
{
    STAILQ_INIT(&availableSlots);
    STAILQ_INIT(&inUseSlots);

    for (int i = 0; i < MAX_REGISTERS_NUM; i++)
        queuePush(&availableSlots, &slotsMemoryPool[i]);
}

void KnownRegisters_clear()
{
    Slot_t *slot = NULL;
    while (!queueIsEmpty(&inUseSlots))
    {
        slot = queuePop(&inUseSlots);
        queuePush(&availableSlots, slot);
    }
}

bool KnownRegisters_remove(char *regName)
{
    Slot_t *slot = queueRemove(&inUseSlots, regName);
    if (slot == NULL)
        return false;
    queuePush(&availableSlots, slot);
    return true;
}

bool KnownRegisters_add(const RegisterAccessData_t *rad)
{
    if (queueFind(&inUseSlots, rad->regName) != NULL || queueFindByModbus(&inUseSlots, rad->readFunction, rad->slaveAddr, rad->regId) != NULL)
        return false;

    Slot_t *slot = queuePop(&availableSlots);
    if (slot == NULL)
        return false;
    slot->rad = *rad;
    queuePush(&inUseSlots, slot);
    return true;
}

int KnownRegisters_count()
{
    return queueSize(&inUseSlots);
}

bool KnownRegisters_find(char *regName, RegisterAccessData_t *radOut)
{
    RegisterAccessData_t *rad = queueFind(&inUseSlots, regName);
    if (rad != NULL)
    {
        *radOut = *rad;
        return true;
    }
    return false;
}

bool KnownRegisters_findByModbus(uint8_t readFunction, uint8_t slaveAddr, uint16_t regId, RegisterAccessData_t *radOut)
{
    RegisterAccessData_t *rad = queueFindByModbus(&inUseSlots, readFunction, slaveAddr, regId);
    if (rad != NULL)
    {
        *radOut = *rad;
        return true;
    }
    return false;
}

bool KnownRegisters_setMonitored(char *regName, bool monitored)
{
    RegisterAccessData_t *rad = queueFind(&inUseSlots, regName);
    if (rad != NULL)
    {
        rad->monitored = monitored;
        return true;
    }
    return false;
}

bool KnownRegisters_setOnChange(char *regName, bool onChange)
{
    RegisterAccessData_t *rad = queueFind(&inUseSlots, regName);
    if (rad != NULL && rad->monitored)
    {
        rad->publishOnChange = onChange;
        return true;
    }
    return false;
}

bool KnownRegisters_setChangeCheckInterval(char *regName, Seconds_t changeCheckInterval)
{
    RegisterAccessData_t *rad = queueFind(&inUseSlots, regName);
    if (rad != NULL && rad->monitored && rad->publishOnChange)
    {
        rad->changeCheckInterval = changeCheckInterval;
        return true;
    }
    return false;
}

bool KnownRegisters_setMaxPublishDelay(char *regName, Seconds_t maxPublishDelay)
{
    RegisterAccessData_t *rad = queueFind(&inUseSlots, regName);
    if (rad != NULL && rad->monitored)
    {
        rad->maxPublishDelay = maxPublishDelay;
        return true;
    }
    return false;
}

bool KnownRegisters_setWritable(char *regName, bool writable, uint8_t writeFunction)
{
    RegisterAccessData_t *rad = queueFind(&inUseSlots, regName);
    if (rad != NULL)
    {
        rad->writable = writable;
        rad->writeFunction = writeFunction;
        return true;
    }
    return false;
}

bool KnownRegisters_setInterpretedAsSigned(char *regName, bool asSigned)
{
    RegisterAccessData_t *rad = queueFind(&inUseSlots, regName);
    if (rad != NULL && rad->type == RADType_NUMBER)
    {
        rad->interpretAsSigned = asSigned;
        return true;
    }
    return false;
}

bool KnownRegisters_setFactor(char *regName, double factor)
{
    RegisterAccessData_t *rad = queueFind(&inUseSlots, regName);
    if (rad != NULL && factor != 0 && (rad->type == RADType_NUMBER || rad->type == RADType_FLOAT))
    {
        rad->factor = factor;
        return true;
    }
    return false;
}

bool KnownRegisters_setOffset(char *regName, double offset)
{
    RegisterAccessData_t *rad = queueFind(&inUseSlots, regName);
    if (rad != NULL && (rad->type == RADType_NUMBER || rad->type == RADType_FLOAT))
    {
        rad->offset = offset;
        return true;
    }
    return false;
}

bool KnownRegisters_setDecimals(char *regName, uint8_t decimals)
{
    RegisterAccessData_t *rad = queueFind(&inUseSlots, regName);
    if (rad != NULL && (rad->type == RADType_NUMBER || rad->type == RADType_FLOAT))
    {
        rad->decimals = decimals;
        return true;
    }
    return false;
}

bool KnownRegisters_setLength(char *regName, uint8_t length)
{
    RegisterAccessData_t *rad = queueFind(&inUseSlots, regName);
    if (rad != NULL)
    {

        // TODO CHECK read function if is single register exit!!!
        if (rad->type == RADType_NUMBER || rad->type == RADType_FLOAT)
        {
            // for number between 1 and 4
            if (length >= 1 && length <= 4)
            {
                rad->regNumber = length;
            }
            else
            {
                return false;
            }
        }
        else if (rad->type == RADType_STRING)
        {
            // for number between 1 and 10
            if (length >= 1 && length <= 10)
            {
                rad->regNumber = length;
            }
            else
            {
                return false;
            }
        }
        else
        {
            // RAW or other
            return false;
        }

        return true;
    }

    return false;
}

bool KnownRegisters_at(int idx, RegisterAccessData_t *radOut)
{
    RegisterAccessData_t *rad = queueAt(&inUseSlots, idx);
    if (rad != NULL)
    {
        *radOut = *rad;
        return true;
    }
    return false;
}

const char *KnownRegisters_getLatestPublishedValueAt(int idx)
{
    Slot_t *slot = queueSlotAt(&inUseSlots, idx);
    if (slot != NULL)
    {
        return slot->latestPublishedValue;
    }
    return NULL;
}

bool KnownRegisters_setLatestPublishedValueAt(int idx, const char *value)
{
    Slot_t *slot = queueSlotAt(&inUseSlots, idx);
    if (slot != NULL && strlen(value) < MAX_LATEST_PUBLISHED_SIZE - NULL_CHAR_LEN)
    {
        strcpy(slot->latestPublishedValue, value);
        return true;
    }
    return false;
}

bool KnownRegisters_getLatestPublishedTimeAt(int idx, Seconds_t *latestPublish)
{
    Slot_t *slot = queueSlotAt(&inUseSlots, idx);
    if (slot != NULL)
    {
        *latestPublish = slot->latestPublishSec;
        return true;
    }
    return false;
}

bool KnownRegisters_getMustPublish(int idx, bool *mustPublish)
{
    Slot_t *slot = queueSlotAt(&inUseSlots, idx);
    if (slot != NULL)
    {
        *mustPublish = slot->mustPublish;
        return true;
    }
    return false;
}

bool KnownRegisters_setLatestPublishedTimeAt(int idx, Seconds_t latestPublishedTime)
{
    Slot_t *slot = queueSlotAt(&inUseSlots, idx);
    if (slot != NULL)
    {
        slot->latestPublishSec = latestPublishedTime;
        return true;
    }
    return false;
}

bool KnownRegisters_setMustPublish(int idx, bool mustPublish)
{
    Slot_t *slot = queueSlotAt(&inUseSlots, idx);
    if (slot != NULL)
    {
        slot->mustPublish = mustPublish;
        return true;
    }
    return false;
}
