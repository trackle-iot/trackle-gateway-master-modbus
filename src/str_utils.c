#include "str_utils.h"

SplitRes_t splitInPlace(char *strIn, char separatorIn, char *tokensOut[], int maxTokensNumIn, int *tokensNumOut)
{
    if (strIn == NULL)
        return SplitRes_NULL_STRIN;

    int tokIdx = 0;
    char *nextTok = strIn;
    do
    {
        if (tokIdx == maxTokensNumIn)
        {
            *tokensNumOut = maxTokensNumIn;
            return SplitRes_TOO_MANY_PARAMS;
        }
        tokensOut[tokIdx] = nextTok;
        nextTok = strchr(nextTok, separatorIn);
        if (nextTok != NULL)
        {
            *nextTok = '\0';
            nextTok++;
        }
        tokIdx++;
    } while (nextTok != NULL);
    *tokensNumOut = tokIdx;

    return SplitRes_OK;
}

bool strContainsOnlyDigits(char *string)
{
    if (string == NULL || strlen(string) == 0)
        return false;
    const int len = strlen(string);
    for (int i = 0; i < len; i++)
    {
        if (string[i] < '0' || string[i] > '9')
        {
            return false;
        }
    }
    return true;
}

bool strValLessThan(char *string1, char *string2)
{
    if (string1 == NULL || string2 == NULL)
        return false;
    const size_t len1 = strlen(string1);
    const size_t len2 = strlen(string2);
    if (len1 < len2)
        return true;
    if (len1 > len2)
        return false;
    for (int i = 0; i < len2; i++)
    {
        if (string1[i] > string2[i])
            return false;
        if (string1[i] < string2[i])
            return true;
    }
    return true;
}

bool strContainsValidDouble(char *string)
{
    if (string == NULL)
        return false;

    const int len = strlen(string);
    int intDigits = 0;
    int decDigits = 0;
    bool foundDot = false;
    for (int i = 0; i < len; i++)
    {
        if (i == 0 && string[i] == '-')
            continue;
        if (!foundDot && string[i] == '.')
        {
            foundDot = true;
            continue;
        }
        if (string[i] >= '0' && string[i] <= '9')
        {
            if (!foundDot)
                intDigits++;
            else
                decDigits++;
        }
        else
            return false;
    }
    if (foundDot && (intDigits < 1 || decDigits < 1))
        return false;
    return true;
}
