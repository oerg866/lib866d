/*  LIB866D
    Argument Parsing System

    (C) 2024 E. Voirin (oerg866)
*/

#ifndef _ARGS_H_
#define _ARGS_H_

#include "types.h"

#include <stddef.h>

/* EXAMPLE ON HOW TO DEFINE ARGUMENT LISTS:
    static bool orderBurgers;
    static u8   burgerCount;
 
    // Checker function for the "burgers" argument
    static bool burgerCheck(const void *arg) {
        const u8 *count = (const u8 *) arg;
        if (count > 10) {
            printf("Too many burgers!");
            return false;
        }
        return true;
    }

    static const args_arg k6init_args[] = {
    //  { <argname>, <parameter short form description(s)>, <argument description>,       <argument type>, <found flag>,  <destination for value>, <checker function>}
        { "burgers", "count",                               "Amount of burgers to order", ARG_U8,          &orderBurgers, &burgerCount,            NULL }
    }

    The argname is the actual argument to use (in this case /burgers:<count>).
    The parameter short form is purely for usage printing reasons.
    The argument description likewise is for usage printing reasons.
    The argument type is one of the argument types listed below.
    The found flag gets set when the argument is encountered. 
    The destination is a pointer to where the value will be stored.
    The checker function is a function called afterwards that can be used to do a range check or similar.
*/

#define ARG_STR         0x0000  /* for internal use; Use ARG_STRING instead */
#define ARG_U8          0x0100  /* Argument is a 8 bit Unsigned Integer */
#define ARG_U16         0x0200  /* Argument is a 16 bit Unsigned Integer */
#define ARG_U32         0x0300  /* Argument is a 32 bit Unsigned Integer */
#define ARG_I8          0x0400  /* Argument is a 8 bit Signed Integer */
#define ARG_I16         0x0500  /* Argument is a 16 bit Signed Integer */
#define ARG_I32         0x0600  /* Argument is a 32 bit Signed Integer */
#define ARG_BOOL        0x0700  /* Argument is a Boolean */
#define ARG_FLAG        0x0800  /* Argument is a flag (no parameter; the destination will just be set to 'true' when it is given) */
#define ARG_NFLAG       0x0900  /* Argument is a negative flag (no parameter; the destination will just be set to 'false' when it is given) */
#define ARG_BLANK       0xFB00  /* for internal use; use ARGS_BLANK instead */
#define ARG_HEADER      0xFC00  /* for internal use; use ARGS_HEADER instead */
#define ARG_EXPLAIN     0xFD00  /* for internal use; use ARGS_EXPLAIN instead */
#define ARG_USAGE       0xFE00  /* for internal use; use ARGS_USAGE instead */
#define ARG_NONE        0xFF00  /* for internal use */

/* Defines a string of given length (MAXIMUM 255) */
#define ARG_STRING(size) ( ARG_STR | (size & 0xFF) )

/* Defines an Array of <type> values. This is ONLY supported for numeric types and booleans!
   <count> must be 255 or lower!  */
#define ARG_ARRAY(type, count) ( type | (count & 0xFF) )

/* ARG_MAX is the minimum length a target string buffer for a string argument must have */
#define ARG_MAX  255

typedef u16 arg_type;

typedef bool (*arg_check_fn) (const void *); /* The void* argument is a pointer to the value to check */

typedef struct {
    const char     *prefix;         /* Prefix of the argument's value */
    const char     *paramNames;     /* Names of parameters, printed inside <...> brackets*/
    const char     *description;    /* Description of the argument */
    arg_type        type;           /* Argument type (see defines above) */
    bool           *foundFlag;      /* Flag to be set when the argument is found */
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
#define ARGS_HEADER(title, appInfo)     { title,  NULL, appInfo,  ARG_HEADER, NULL, NULL, NULL }
/*  ARGS_USAGE defines an argument which prints the entire argument list.
    param is the parameter (e.g. '?' or 'help')
    desc is a descroption for it (e.g. 'Shows supported parameters' - kind of redundant..) */
#define ARGS_USAGE(param, desc)         { param,  NULL, desc,     ARG_USAGE,  NULL, NULL, NULL }
/*  ARGS_BLANK indicates a blank entry. The usage will display a blank line here.
    This can be used to logically group parameters together. */
#define ARGS_BLANK                      { NULL,   NULL, NULL,     ARG_BLANK,  NULL, NULL, NULL }
/*  ARGS_EXPLAIN indicates that the previous argument requires further
    explanation. It will be printed separately. */
#define ARGS_EXPLAIN(x)                 { NULL,   NULL, x,        ARG_EXPLAIN,NULL, NULL, NULL }


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
