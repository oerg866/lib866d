/*  LIB866D
    Argument Parsing System

    (C) 2024 E. Voirin (oerg866)
*/

#include "args.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "types.h"
#include "vgacon.h"
#include "util.h"

#define __LIB866D_TAG__ "ARGS.C"
#include "debug.h"

typedef struct {
    arg_type    type;
    const char *name;
    const char *paramDesc;
} arg_type_name;

#if 0
static const arg_type_name argTypeNames[] = {
    { ARG_STR,  "Text  ", "Max. 255 characters" },
    { ARG_U8,   "Uint8 ", "Positive Integer" },
    { ARG_U16,  "Uint16", "Positive Integer" },
    { ARG_U32,  "Uint32", "Positive Integer" },
    { ARG_I8,   "Int8  ", "Positive or Negative Integer" },
    { ARG_I16,  "Int16 ", "Positive or Negative Integer" },
    { ARG_I32,  "Int32 ", "Positive or Negative Integer" },
    { ARG_BOOL, "Bool  ", "Boolean (0 or 1)" },
    { ARG_FLAG, "Flag  ", "Option Toggle" },
};

static const char * getArgTypeName(arg_type argType) {
    size_t idx;
    for (idx = 0; idx < ARG_TYPE_NAMES_COUNT; ++idx) {
        if (argTypeNames[idx].type == argType)
           return argTypeNames[idx].name;
    }
    return NULL;
}

static const char * getArgTypeDescription(arg_type argType) {
    size_t idx;
    for (idx = 0; idx < ARG_TYPE_NAMES_COUNT; ++idx) {
        if (argTypeNames[idx].type == argType)
           return argTypeNames[idx].paramDesc;
    }
    return NULL;
}

#endif

#ifdef DEBUG
typedef const char *args_ParseErrorLookup;

static const args_ParseErrorLookup args_parseErrorLookupTable[] = {
    "ARGS_SUCCESS",
    "ARGS_ERROR",
    "ARGS_INPUT_ERROR",
    "ARGS_OUT_OF_RANGE",
    "ARGS_TO_FEW_ARRAY_VALUE",
    "ARGS_STRING_TOO_LONG",
    "ARGS_CHECK_FAILED",
    "ARGS_ARG_NOT_FOUND",
    "ARGS_USAGE_PRINTED",
    "ARGS_INTERNAL_ERROR",
    "ARGS_NO_ARGUMENTS",
};
#define ARGS_ERROR_STR(x) (args_parseErrorLookupTable[x])
#else
#define ARGS_ERROR_STR(x) (0)
#endif

#define ARG_TYPE_NAMES_COUNT (sizeof(argTypeNames) / sizeof(arg_type_name))

#define GET_ARG_TYPE(type) ((arg_type) (type & 0xFF00))
#define GET_ARG_ARRAYSIZE(type) (type & 0x00FF)
#define ARG_HAS_PARAM(type) (type != ARG_FLAG && type != ARG_USAGE)

/* Microsoft C quirk ... */
#ifndef snprintf
#define snprintf _snprintf
#endif


void args_printAppInfo(const args_arg *argList, size_t argListSize) {
    size_t idx;
    for (idx = 0; idx < argListSize; idx++) {
        if (argList[idx].type == ARG_USAGE) {
            printf("%s\n", (const char *) argList[idx].dst);
            return;
        }
    }
}

static void args_incrementAndCheckPageBreak(void) {
    static size_t printedLines = 0;
    size_t consoleHeight = vgacon_getConsoleHeight();
    printedLines++;
    if (printedLines % consoleHeight == consoleHeight - 1) {
        vgacon_waitKeyWithMessage();
    }
}

static void args_printLineSeparator(void) {
    u16 i;
    u16 width = vgacon_getConsoleWidth();
    for (i = 0; i < width; ++i) {
        putchar(0xcd);
    }
    args_incrementAndCheckPageBreak();
}

void args_printUsage(const args_arg *argList, size_t argListSize) {
    size_t idx = 0;
    char   tmp[256] = { 0, };

    L866_NULLCHECK(argList);
    L866_ASSERT(argListSize > 0);

    /* First entry can be an ARGS_HEADER entry, so we print it at the start */
    if (GET_ARG_TYPE(argList[0].type) == ARG_HEADER) {
        args_printLineSeparator();
        printf("%s\n\n", argList[0].prefix);
        printf("%s\n", argList[0].description);
        vgacon_waitKeyWithMessage();
        args_printLineSeparator();
        idx = 1;
    }

    printf("\n Valid command line parameters are: \n\n");

    for (; idx < argListSize; ++idx) {
        switch (GET_ARG_TYPE(argList[idx].type)) {
            case ARG_HEADER:
                break;
            case ARG_BLANK:
                args_incrementAndCheckPageBreak();
                printf("\n");
                break;
            case ARG_EXPLAIN:
                args_incrementAndCheckPageBreak();
                printf("%*s %s\n",
                    25, "",
                    argList[idx].description);
                break;
            default:
                /* current entry is an actual parameter type we need to print */
                args_printLineSeparator();

                if (ARG_HAS_PARAM(GET_ARG_TYPE(argList[idx].type))) {
                    snprintf(tmp, sizeof(tmp), "/%s:<%s>",
                        argList[idx].prefix,
                        argList[idx].paramNames ? argList[idx].paramNames : "...");

                    printf("%*s %s\n",
                        25, tmp,
                        argList[idx].description);

                } else {
                    sprintf(tmp, "/%s", argList[idx].prefix);
                    printf("%*s %s\n",
                        25, tmp,
                        argList[idx].description);
                }

                args_incrementAndCheckPageBreak();

                break;
        }
    }

    printf("\n");
}

static args_ParseError parseAndSetNum(const args_arg *arg, const char *toParse, size_t arraySize, bool isSigned, size_t size) {
    union { u32 uVal; i32 iVal; } parsedValue;
    i32 signedMin = (i32) (0xFFFFFFFFL << (size * 8UL));
    i32 signedMax = 0x7FFFFFFFL >> ((4 - size) * 8UL);
    u32 unsignedMax = 0xFFFFFFFFUL >> ((4 - size) * 8UL);
    char *parseEnd = NULL;
    size_t arrayIndex;

    for (arrayIndex = 0; arrayIndex < arraySize; arrayIndex++) {
        if (isSigned) {
            parsedValue.iVal = strtoul(toParse, &parseEnd, 0);
        } else {
            parsedValue.uVal = strtol(toParse, &parseEnd, 0);
        }

        DBG("parseAndSetNum: /%s parsed uVal 0x%08lx\n", arg->prefix, parsedValue.uVal);

        if ((*parseEnd == '\0') && (parseEnd != toParse) && (arrayIndex == (arraySize - 1))) {
            DBG("parseAndSetNum: Last element found!\n");
            /* Do nothing else. We're happy! */
        } else if ((*parseEnd == '\0') && (arrayIndex < (arraySize - 1))) {
            printf ("Input '%s' has too few values for argument /%s!\n", toParse, arg->prefix);
            return ARGS_TO_FEW_ARRAY_VALUES;
        } else if ((parseEnd == toParse) || (*parseEnd != ',')) {
            printf ("Input '%s' could not be parsed as a numeric value.\n", toParse);
            DBG("parseEnd: '%s'\n", parseEnd);
            return ARGS_INPUT_ERROR;
        } else if (isSigned && (parsedValue.iVal < signedMin || parsedValue.iVal > signedMax)) {
            printf ("Input %ld is out of range (%ld < x < %ld)\n", parsedValue.iVal, signedMin, signedMax);
            return ARGS_OUT_OF_RANGE;
        } else if (parsedValue.uVal > unsignedMax) {
            printf ("Input %lu is out of range (limit: %lu)\n", parsedValue.uVal, unsignedMax);
            return ARGS_OUT_OF_RANGE;
        }

        if (arg->dst) {
            memcpy(((u8*) arg->dst) + size * arrayIndex, &parsedValue, size);
        }

        toParse = (const char *) parseEnd + 1;
    }

    if (arg->checker && !arg->checker(arg->dst)) {
        return ARGS_CHECK_FAILED;
    }

    return ARGS_SUCCESS;
}

static args_ParseError parseAndSetStr(const args_arg *arg, const char *toParse, size_t length) {
    if (length == 0 || length > ARG_MAX) {
        length = ARG_MAX;
    }

    if (strlen(toParse) > length) {
        return ARGS_STRING_TOO_LONG;
    }

    if (arg->dst) {
        strncpy((char *) arg->dst, toParse, ARG_MAX);
    }

    if (arg->checker && !arg->checker(toParse)) {
        return ARGS_CHECK_FAILED;
    }

    return ARGS_SUCCESS;
}

static args_ParseError setFlag(const args_arg *arg) {
    bool *dstFlag = (bool*) arg->dst;


    if (dstFlag) {
        *dstFlag = true;
    }

    if (arg->checker && arg->checker(dstFlag) == false) {
        return ARGS_CHECK_FAILED;
    }

    return ARGS_SUCCESS;
}

static args_ParseError doParse(const args_arg *arg, const char *toParse) {
    const char *val = &toParse[strlen(arg->prefix) + 1 + 1];
    size_t arraySize = arg->type & 0xFF;

    if (arraySize == 0) arraySize++; /* 0 = no array = 1 value */

    DBG("doParse: Type %u Array size %u\n", (u16) GET_ARG_TYPE(arg->type), (u16) GET_ARG_ARRAYSIZE(arg->type));

    switch (GET_ARG_TYPE(arg->type)) {
        case ARG_STR:   return parseAndSetStr(arg, val, arraySize);
        case ARG_U8:    return parseAndSetNum(arg, val, arraySize, false, sizeof(u8));
        case ARG_U16:   return parseAndSetNum(arg, val, arraySize, false, sizeof(u16));
        case ARG_U32:   return parseAndSetNum(arg, val, arraySize, false, sizeof(u32));
        case ARG_I8:    return parseAndSetNum(arg, val, arraySize, true,  sizeof(i8));
        case ARG_I16:   return parseAndSetNum(arg, val, arraySize, true,  sizeof(i16));
        case ARG_I32:   return parseAndSetNum(arg, val, arraySize, true,  sizeof(i32));
        case ARG_BOOL:  return parseAndSetNum(arg, val, arraySize, false, sizeof(u8));
        case ARG_FLAG:  return setFlag       (arg);
        case ARG_USAGE:
        default:
            return ARGS_INTERNAL_ERROR;
    }
}

static bool isThisArg(const args_arg *arg, const char *str) {

    size_t prefixLen = strlen(arg->prefix);

    if (  (strlen(str) < prefixLen)
       || (str[0] != '/')
       || (util_strncasecmp(&str[1], arg->prefix, prefixLen) != 0) )
    {
        return false;
    }


    /*  Flags and Usage are not expected to have a parameter so they must end with null terminator
        Everything else needs a colon. */
    switch (GET_ARG_TYPE(arg->type)) {
        case ARG_HEADER:
        case ARG_BLANK:
        case ARG_EXPLAIN:
            return false;
        case ARG_FLAG:
        case ARG_USAGE:
            if (str[prefixLen + 1] != '\0') return false;
            break;
        default:
            if (str[prefixLen + 1] != ':') return false;
            break;
    }

    return true;
}

static void printUsageHintIfPresent(const args_arg *argList, size_t argListSize) {
    size_t idx;
    for (idx = 0; idx < argListSize; idx++) {
        if (argList[idx].type == ARG_USAGE) {
            printf("Use /%s for parameter information.\n", argList[idx].prefix);
            return;
        }
    }
}

args_ParseError args_parseArg(const args_arg *argList, size_t argListSize, const char *toParse) {
    /* Das ja ganz schoen argListig hier :3 */
    size_t idx;
    args_ParseError ret = ARGS_ARG_NOT_FOUND;

    L866_NULLCHECK(argList);
    L866_NULLCHECK(toParse);

    for (idx = 0; idx < argListSize; ++idx) {
        if (argList[idx].prefix == NULL)
            continue;

        if (isThisArg(&argList[idx], toParse)) {
            if (argList[idx].type == ARG_USAGE) {
                args_printUsage(argList,argListSize);
                return ARGS_USAGE_PRINTED;
            }

            if (ARGS_SUCCESS == (ret = doParse(&argList[idx], toParse))) {
                DBG("args_parseArg: Successfully matched prefix /%s with input '%s'\n", argList[idx].prefix, toParse);
                return ARGS_SUCCESS;
            }

            DBG("args_parseArg: Prefix /%s matches, but parsing failed (%s)\n", argList[idx].prefix, ARGS_ERROR_STR(ret));
        }
    }

    if (ret == ARGS_ARG_NOT_FOUND) {
        printf("Input Parameter '%s' not recognized.\n", toParse);
        printUsageHintIfPresent(argList, argListSize);
    }

    return ret;
}

args_ParseError args_parseAllArgs(int argc, const char *argv[], const args_arg *argList, size_t argListSize) {
    args_ParseError ret = ARGS_SUCCESS;

    if (argc <= 1) {
        return ARGS_NO_ARGUMENTS;
    }

    argc--; /* Skip argv[0] */
    argv++;
    while (argc--) {
        if (ARGS_SUCCESS != (ret = args_parseArg(argList, argListSize, *argv++))) {
            return ret;
        }
    }

    return ARGS_SUCCESS;
}
