#ifndef STR_UTILS_H_
#define STR_UTILS_H_

#include <string.h>
#include <stdbool.h>

#define MAX_U16_STR "65535"
#define MAX_U8_STR "255"
#define MAX_I32_STR "2147483647"
#define MAX_U32_STR "4294967295"
#define MAX_I8_STR "127"

#define NULL_CHAR_LEN 1

// Compile time len of a string
#define CT_STRLEN(s) \
    (sizeof(s) - 1)

#define STREQ(s1, s2) \
    (strcmp(s1, s2) == 0)

typedef enum
{
    SplitRes_OK,              //< Parsing performed successfully
    SplitRes_NULL_STRIN,      //< Parsing string is NULL
    SplitRes_TOO_MANY_PARAMS, //< Parsing found more params than output array can hold
} SplitRes_t;

SplitRes_t splitInPlace(char *strIn, char separatorIn, char *tokensOut[], int maxTokensNumIn, int *tokensNumOut);
bool strContainsOnlyDigits(char *string);
bool strValLessThan(char *string1, char *string2);
bool strContainsValidDouble(char *string);

#endif