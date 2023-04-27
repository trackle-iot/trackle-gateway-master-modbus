#ifndef SEM_UTILS_H_
#define SEM_UTILS_H_

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @brief Take/lock a FreeRTOS semaphore (binary/mutex) blocking on it forever. If it's not possible, abort.
 * @param sem Semaphore to take/lock.
 */
#define BLOCKING_LOCK_OR_ABORT(sem)                   \
    if (xSemaphoreTake(sem, portMAX_DELAY) != pdTRUE) \
        abort();

/**
 * @brief Take/lock a FreeRTOS semaphore (binary/mutex) or return immediately.
 * @param sem Semaphore to take/lock.
 */
#define TRY_LOCK_OR_RETURN(sem, retValue) \
    if (xSemaphoreTake(sem, 0) != pdTRUE) \
        return retValue;

/**
 * @brief Give/unblock a FreeRTOS semaphore (binary/mutex). If it's not possible, abort.
 * @param sem Semaphore to give/unlock.
 */
#define UNLOCK_OR_ABORT(sem)           \
    if (xSemaphoreGive(sem) != pdTRUE) \
        abort();

#endif