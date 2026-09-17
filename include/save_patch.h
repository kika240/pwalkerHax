#ifndef SAVE_PATCH_H
#define SAVE_PATCH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PW_EEPROM_SIZE 65536u
#define PW_PATCH_MAX 1024u
#define PW_PATCH_RECORDS 19u
#define PW_PATCH_HEADER 88u

typedef struct {
    uint16_t address, length;
    uint8_t before[16], after[16];
} pw_edit;
typedef struct {
    uint8_t identity[40], trainer[4], walk_time[4], companion[16];
    size_t count;
    pw_edit edits[PW_PATCH_RECORDS];
} pw_patch;

uint32_t pw_crc32(const uint8_t *data, size_t length);
bool pw_patch_parse(pw_patch *out, const uint8_t *data, size_t length, char *error, size_t capacity);
bool pw_patch_check(const pw_patch *patch, const uint8_t *eeprom, size_t length, char *error, size_t capacity);
bool pw_patch_apply_image(const pw_patch *patch, uint8_t *eeprom, size_t length, char *error, size_t capacity);
bool pw_edit_is_health(const pw_edit *edit);
const char *pw_edit_name(const pw_edit *edit);
#endif
