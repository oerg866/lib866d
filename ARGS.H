/*  LIB866D
    Argument Parsing System

    (C) 2024 E. Voirin (oerg866)
*/

#ifndef _ARGS_H_
#define _ARGS_H_

#include "types.h"

#include <stddef.h>

#define ARG_STR         0x0000
#define ARG_U8          0x0100
#define ARG_U16         0x0200
#define ARG_U32         0x0300
#define ARG_I8          0x0400
#define ARG_I16         0x0500
#define ARG_I32         0x0600
#define ARG_BOOL        0x0700
#define ARG_FLAG        0x0800
#define ARG_BLANK       0xFB00
#define ARG_HEADER      0xFC00
#define ARG_EXPLAIN     0xFD00
#define ARG_USAGE       0xFE00
#define ARG_NONE        0xFF00

/* Defines a string of given length (MAXIMUM 255) */
#define ARG_STRING(size) ( ARG_STR | (size & 0xFF) )

/* Defines an Array of <type> values. This is ONLY supported for numeric types and booleans!
   <count> must be 255 or lower!  */
#define ARG_ARRAY(type, count) ( type | (count & 0xFF) )

/* ARG_MAX is the minimum length a target string buffer for a string argument must have */
#define ARG_MAX  255

typedef u16 arg_type;

typedef bool (*arg_check_fn) (const void *); /* The void* argument is a pointer to the value to check */

typedef struct { bool found; u8     value; } args_U8;
typedef struct { bool found; u16    value; } args_U16;
typedef struct { bool found; u32    value; } args_U32;
typedef struct { bool found; i8     value; } args_I8;
typedef struct { bool found; i16    value; } args_I16;
typedef struct { bool found; i32    value; } args_I32;
typedef struct { bool found; bool   value; } args_Bool;
typedef struct { bool found; }               args_Flag;

typedef struct {
    const char     *prefix;         /* Prefix of the argument's value */
    const char     *paramNames;     /* Names of parameters, printed inside <...> brackets*/
    const char     *description;    /* Description of the argument */
    arg_type        type;           /* Argument type (see defines above) */
    void           *dst;            /* Pointer to where the parsed value will go */
    arg_check_fn    checker;        /* Pointer to function that checks validity */
} args_arg;

/*  ARGS_HEADER indicates a header that is displayed at the start of the program.
    This must ALWAYS be the first entry of an args_arg array!

    Example:
      ARGS_HEADER("Cool Program", "This is a program which is cool and very awesome."), 
     ...
      { "asd", "A cool argument", ARG_STR, &sArgument, NULL },
      ARGS_EXPLAIN("This argument is so cool that you can't even imagine it!"),
     ...
*/
#define ARGS_HEADER(title, appInfo)     { title,  NULL, appInfo,  ARG_HEADER, NULL, NULL }
/*  ARGS_USAGE defines an argument which prints the entire argument list.
    param is the parameter (e.g. '?' or 'help')
    desc is a descroption for it (e.g. 'Shows supported parameters' - kind of redundant..) */
#define ARGS_USAGE(param, desc)         { param,  NULL, desc,     ARG_USAGE,  NULL, NULL }
/*  ARGS_BLANK indicates a blank entry. The usage will display a blank line here.
    This can be used to logically group parameters together. */
#define ARGS_BLANK                      { NULL,   NULL, NULL,     ARG_BLANK,  NULL, NULL }
/*  ARGS_EXPLAIN indicates that the previous argument requires further
    explanation. It will be printed separately. */
#define ARGS_EXPLAIN(x)                 { NULL,   NULL, x,        ARG_EXPLAIN,NULL, NULL }


typedef enum {
    ARGS_SUCCESS = 0,
    ARGS_ERROR,
    ARGS_INPUT_ERROR,
    ARGS_OUT_OF_RANGE,
    ARGS_TO_FEW_ARRAY_VALUES,
    ARGS_STRING_TOO_LONG,
    ARGS_CHECK_FAILED,
    ARGS_ARG_NOT_FOUND,
    ARGS_USAGE_PRINTED,
    ARGS_INTERNAL_ERROR,
    ARGS_NO_ARGUMENTS
} args_ParseError;

/*  Parse an argument and set the destinations value if found.
    the 'checker' function gets called afterwards. It may check
    the argument for validity. (Can be NULL, too.) */
args_ParseError args_parseArg(const args_arg *argList, size_t argListSize, const char *toParse);

/*  Parse all arguments in an argc/argv context. Returns ARGS_NO_ARGUMENTS if there are no arguments. */
args_ParseError args_parseAllArgs(int argc, const char *argv[], const args_arg *argList, size_t argListSize);

/*  Print program usage.
    Will do nothing if argList does not contain an ARGS_USAGE parameter. */
void args_printUsage(const args_arg *argList, size_t argListSize);
#endif /* ARGS_H */
