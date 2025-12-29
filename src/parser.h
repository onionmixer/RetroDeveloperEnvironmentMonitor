/*
 * parser.h - JSON parsing for rdemonitor
 *
 * Parses JSON Lines data according to OUTPUT_SPEC_V01 format.
 */

#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

#define PARSER_MAX_EMU_LEN   16
#define PARSER_MAX_CAT_LEN   16
#define PARSER_MAX_SEC_LEN   32
#define PARSER_MAX_FLD_LEN   32
#define PARSER_MAX_VAL_LEN   512
#define PARSER_MAX_ADDR_LEN  16

typedef struct {
    /* Required fields */
    char emu[PARSER_MAX_EMU_LEN];   /* Emulator: "apple" | "msx" */
    char cat[PARSER_MAX_CAT_LEN];   /* Category: cpu, mem, io, mach, dbg, sys */
    char sec[PARSER_MAX_SEC_LEN];   /* Section */
    char fld[PARSER_MAX_FLD_LEN];   /* Field name */
    char val[PARSER_MAX_VAL_LEN];   /* Value (always string) */

    /* Optional fields */
    char addr[PARSER_MAX_ADDR_LEN]; /* Memory/IO address (hex) */
    int  idx;                       /* Index (-1 if not present) */
    long ts;                        /* Timestamp in ms (-1 if not present) */
    int  len;                       /* Data length (-1 if not present) */
    char ver[16];                   /* Protocol version (for hello message) */

    /* Validity flags */
    bool has_addr;
    bool has_idx;
    bool has_ts;
    bool has_len;
    bool has_ver;
} ParsedData;

/*
 * Initialize parsed data structure with default values
 */
void parser_init_data(ParsedData *data);

/*
 * Parse a JSON line into ParsedData structure
 * Returns: 0 on success, -1 on parse error
 */
int parser_parse_line(const char *json_line, ParsedData *data);

/*
 * Check if this is a valid emulator identifier
 */
bool parser_is_valid_emu(const char *emu);

/*
 * Check if this is a known category
 */
bool parser_is_valid_category(const char *cat);

/*
 * Print parsed data (for debugging)
 */
void parser_print_data(const ParsedData *data);

#endif /* PARSER_H */
