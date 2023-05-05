#ifndef NUM_UTILS_H_
#define NUM_UTILS_H_

#include <stdbool.h>
#include <stdint.h>

double registersToNumber(uint16_t uints[], uint8_t len, uint8_t bitPosition, bool asSigned)
{
    uint64_t intValue = 0;
    double doubleValue = 0;

    ESP_LOG_BUFFER_HEX_LEVEL("registersToNumber", uints, 2 * len, ESP_LOG_WARN);

    if (bitPosition == 0) // msb
    {
        for (uint8_t index = 0; index < len; index++)
        {
            intValue <<= 16;
            intValue |= uints[index];
        }
    }
    else if (bitPosition == 1) // lsb
    {
        for (int8_t index = len; index >= 0; index--)
        {
            {
                intValue <<= 16;
                intValue |= uints[index];
            }
        }
    }

    ESP_LOGE("registersToNumber", "%d", (int)intValue);
    ESP_LOGE("registersToNumber", "asSigned %d", asSigned);

    // cast if signed or unsigned
    if (asSigned)
    {
        switch (len)
        {
        case 1: // int16
            doubleValue = *((int16_t *)&intValue);
            break;
        case 2: // int32
            doubleValue = *((int32_t *)&intValue);
            break;
        case 4: // int64
            doubleValue = *((int64_t *)&intValue);
            break;
        }
    }
    else
    {
        doubleValue = intValue;
    }

    ESP_LOGE("registersToNumber", "%f", doubleValue);

    return doubleValue;
}

double registersToFloat(uint16_t uints[], uint8_t len, uint8_t bitPosition, bool asSigned)
{
    uint64_t intValue = 0;
    double doubleValue = 0;

    if (bitPosition == 0) // msb
    {
        for (uint8_t index = 0; index < len; index++)
        {
            intValue <<= 16;
            intValue |= uints[index];
        }
    }
    else if (bitPosition == 1) // lsb
    {
        for (int8_t index = len; index >= 0; index--)
        {
            {
                intValue <<= 16;
                intValue |= uints[index];
            }
        }
    }

    // cast float or double
    switch (len)
    {
    case 2: // float
        doubleValue = *((float *)&intValue);
        break;
    case 4: // double
        doubleValue = *((double *)&intValue);
        break;
    }

    return doubleValue;
}

void numberToRegister(const long long value, uint8_t len, uint8_t bitPosition, uint16_t *result)
{

    if (bitPosition == 0) // msb
    {
        for (uint8_t index = 0; index < len; index++)
        {
            result[(len - 1) - index] = (value >> (16 * index)) & 0x0000FFFF;
        }
    }
    else if (bitPosition == 1) // lsb
    {
        for (uint8_t index = 0; index < len; index++)
        {
            result[index] = (value >> (16 * index)) & 0x0000FFFF;
        }
    }

    ESP_LOG_BUFFER_HEX_LEVEL("numberToRegister", result, 2 * len, ESP_LOG_WARN);
}

void floatToRegister(double value, uint8_t len, uint8_t bitPosition, uint16_t *result)
{

    uint16_t temp[4];

    if (len == 2) // float
    {
        float f_value = (float)value;
        memcpy(temp, &f_value, 4);
    }
    else // double
    {
        memcpy(temp, &value, 8);
    }

    if (bitPosition == 0) // msb
    {
        for (uint8_t index = 0; index < len; index++)
        {
            result[(len - 1) - index] = temp[index];
        }
    }
    else if (bitPosition == 1) // lsb
    {
        for (uint8_t index = 0; index < len; index++)
        {
            result[index] = temp[index];
        }
    }
}

#endif