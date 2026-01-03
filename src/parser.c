/*
 * parser.c - JSON parsing implementation
 *
 * Uses cJSON library for JSON parsing.
 */

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

/* Helper to safely copy string from cJSON */
static void copy_json_string(char *dest, size_t dest_size, const cJSON *item)
{
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        strncpy(dest, item->valuestring, dest_size - 1);
        dest[dest_size - 1] = '\0';
    } else {
        dest[0] = '\0';
    }
}

void parser_init_data(ParsedData *data)
{
    memset(data, 0, sizeof(ParsedData));
    data->idx = -1;
    data->ts = -1;
    data->len = -1;
    data->has_addr = false;
    data->has_idx = false;
    data->has_ts = false;
    data->has_len = false;
    data->has_ver = false;
}

int parser_parse_line(const char *json_line, ParsedData *data)
{
    parser_init_data(data);

    if (json_line == NULL || json_line[0] == '\0') {
        return -1;
    }

    cJSON *root = cJSON_Parse(json_line);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            /* Parse error - silently ignore for now */
        }
        return -1;
    }

    /* Parse required fields */
    cJSON *emu = cJSON_GetObjectItemCaseSensitive(root, "emu");
    cJSON *cat = cJSON_GetObjectItemCaseSensitive(root, "cat");
    cJSON *sec = cJSON_GetObjectItemCaseSensitive(root, "sec");
    cJSON *fld = cJSON_GetObjectItemCaseSensitive(root, "fld");
    cJSON *val = cJSON_GetObjectItemCaseSensitive(root, "val");

    /* Check required fields exist */
    if (!cJSON_IsString(emu) || !cJSON_IsString(cat) ||
        !cJSON_IsString(sec) || !cJSON_IsString(fld) ||
        !cJSON_IsString(val)) {
        cJSON_Delete(root);
        return -1;
    }

    copy_json_string(data->emu, sizeof(data->emu), emu);
    copy_json_string(data->cat, sizeof(data->cat), cat);
    copy_json_string(data->sec, sizeof(data->sec), sec);
    copy_json_string(data->fld, sizeof(data->fld), fld);
    copy_json_string(data->val, sizeof(data->val), val);

    /* Parse optional fields */
    cJSON *addr = cJSON_GetObjectItemCaseSensitive(root, "addr");
    if (cJSON_IsString(addr)) {
        copy_json_string(data->addr, sizeof(data->addr), addr);
        data->has_addr = true;
    }

    cJSON *idx = cJSON_GetObjectItemCaseSensitive(root, "idx");
    if (cJSON_IsNumber(idx)) {
        data->idx = (int)idx->valuedouble;
        data->has_idx = true;
    } else if (cJSON_IsString(idx) && idx->valuestring != NULL) {
        data->idx = (int)strtol(idx->valuestring, NULL, 10);
        data->has_idx = true;
    }

    cJSON *ts = cJSON_GetObjectItemCaseSensitive(root, "ts");
    if (cJSON_IsNumber(ts)) {
        data->ts = (long)ts->valuedouble;
        data->has_ts = true;
    }

    cJSON *len = cJSON_GetObjectItemCaseSensitive(root, "len");
    if (cJSON_IsNumber(len)) {
        data->len = (int)len->valuedouble;
        data->has_len = true;
    } else if (cJSON_IsString(len) && len->valuestring != NULL) {
        data->len = (int)strtol(len->valuestring, NULL, 10);
        data->has_len = true;
    }

    cJSON *ver = cJSON_GetObjectItemCaseSensitive(root, "ver");
    if (cJSON_IsString(ver)) {
        copy_json_string(data->ver, sizeof(data->ver), ver);
        data->has_ver = true;
    }

    cJSON_Delete(root);
    return 0;
}

bool parser_is_valid_emu(const char *emu)
{
    if (emu == NULL) return false;
    return (strcmp(emu, "apple") == 0 || strcmp(emu, "msx") == 0);
}

bool parser_is_valid_category(const char *cat)
{
    if (cat == NULL) return false;

    static const char *valid_cats[] = {
        "cpu", "mem", "io", "mach", "dbg", "sys", NULL
    };

    for (int i = 0; valid_cats[i] != NULL; i++) {
        if (strcmp(cat, valid_cats[i]) == 0) {
            return true;
        }
    }
    return false;
}

void parser_print_data(const ParsedData *data)
{
    printf("ParsedData:\n");
    printf("  emu: %s\n", data->emu);
    printf("  cat: %s\n", data->cat);
    printf("  sec: %s\n", data->sec);
    printf("  fld: %s\n", data->fld);
    printf("  val: %s\n", data->val);

    if (data->has_addr) {
        printf("  addr: %s\n", data->addr);
    }
    if (data->has_idx) {
        printf("  idx: %d\n", data->idx);
    }
    if (data->has_ts) {
        printf("  ts: %ld\n", data->ts);
    }
    if (data->has_len) {
        printf("  len: %d\n", data->len);
    }
    if (data->has_ver) {
        printf("  ver: %s\n", data->ver);
    }
}
