/*
 * Offline event compiler: parses a human-readable event definition file
 * (see src/data/events.def) and emits a flat binary blob that the game
 * loads directly at startup via event_table_load() (src/sim/event.h/c).
 *
 * This is a build-time tool, not part of the running game -- it depends
 * only on the shared sim headers for the EventDef layout (so the binary
 * it writes is guaranteed to match what the game reads, since both are
 * built from the same struct definition), not on any sim implementation
 * code.
 *
 * DSL format (line-based, deliberately simple): a series of
 *
 *   event
 *     key value
 *     key value ...
 *   end
 *
 * blocks. Unknown keywords or malformed lines abort compilation with a
 * line number and message -- silently ignoring bad content would be far
 * worse than failing the build. See src/data/events.def for real
 * examples covering every field this parser understands.
 */

#include "sim/event.h"
#include "sim/relation.h"
#include "sim/traits.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EVENTS   256
#define MAX_LINE_LEN 256

static int trait_id_from_name(const char* name) {
    if (strcmp(name, "loyalty") == 0)      return WARM_TRAIT_LOYALTY;
    if (strcmp(name, "charisma") == 0)     return WARM_TRAIT_CHARISMA;
    if (strcmp(name, "strength") == 0)     return WARM_TRAIT_STRENGTH;
    if (strcmp(name, "beauty") == 0)       return WARM_TRAIT_BEAUTY;
    if (strcmp(name, "intelligence") == 0) return WARM_TRAIT_INTELLIGENCE;
    if (strcmp(name, "libido") == 0)       return WARM_TRAIT_LIBIDO;
    return -1;
}

static uint32_t flag_bit_from_name(const char* name) {
    if (strcmp(name, "loyal") == 0)            return TRAIT_FLAG_LOYAL;
    if (strcmp(name, "criminal_record") == 0)  return TRAIT_FLAG_CRIMINAL_RECORD;
    if (strcmp(name, "wanted") == 0)           return TRAIT_FLAG_WANTED;
    if (strcmp(name, "married") == 0)          return TRAIT_FLAG_MARRIED;
    if (strcmp(name, "employed") == 0)         return TRAIT_FLAG_EMPLOYED;
    return 0;
}

static int status_from_name(const char* name) {
    if (strcmp(name, "keep") == 0)          return REL_STATUS_KEEP;
    if (strcmp(name, "none") == 0)          return REL_STATUS_NONE;
    if (strcmp(name, "acquaintance") == 0)  return REL_STATUS_ACQUAINTANCE;
    if (strcmp(name, "friend") == 0)        return REL_STATUS_FRIEND;
    if (strcmp(name, "best_friend") == 0)   return REL_STATUS_BEST_FRIEND;
    if (strcmp(name, "rival") == 0)         return REL_STATUS_RIVAL;
    if (strcmp(name, "enemy") == 0)         return REL_STATUS_ENEMY;
    if (strcmp(name, "dating") == 0)        return REL_STATUS_DATING;
    if (strcmp(name, "exclusive") == 0)     return REL_STATUS_EXCLUSIVE;
    if (strcmp(name, "fiance") == 0)        return REL_STATUS_FIANCE;
    if (strcmp(name, "spouse") == 0)        return REL_STATUS_SPOUSE;
    if (strcmp(name, "fwb") == 0)           return REL_STATUS_FWB;
    if (strcmp(name, "affair") == 0)        return REL_STATUS_AFFAIR;
    if (strcmp(name, "ex") == 0)            return REL_STATUS_EX;
    return -1;
}

static bool parse_bool(const char* s) {
    return strcmp(s, "true") == 0 || strcmp(s, "1") == 0;
}

/* Splits `line` into up to `max_tokens` whitespace-separated tokens,
 * treating a "quoted phrase" as a single token (for the `name` field).
 * Modifies `line` in place (inserts NULs at token boundaries). Returns
 * the number of tokens found. */
static bool is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int tokenize(char* line, char* tokens[], int max_tokens) {
    int count = 0;
    char* p = line;
    while (*p != '\0' && count < max_tokens) {
        while (is_ws(*p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }

        if (*p == '"') {
            p++;
            tokens[count++] = p;
            while (*p != '"' && *p != '\0') {
                p++;
            }
            if (*p == '"') {
                *p = '\0';
                p++;
            }
        } else {
            tokens[count++] = p;
            while (!is_ws(*p) && *p != '\0') {
                p++;
            }
            if (*p != '\0') {
                *p = '\0';
                p++;
            }
        }
    }
    return count;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input.def> <output.bin>\n", argv[0]);
        return 1;
    }

    FILE* in = fopen(argv[1], "r");
    if (in == NULL) {
        fprintf(stderr, "error: cannot open %s\n", argv[1]);
        return 1;
    }

    static EventDef events[MAX_EVENTS];
    uint32_t event_count = 0;
    bool in_event = false;
    EventDef current;
    memset(&current, 0, sizeof(current));

    char line[MAX_LINE_LEN];
    int line_no = 0;

    while (fgets(line, sizeof(line), in) != NULL) {
        line_no++;

        char* comment = strchr(line, '#');
        if (comment != NULL) {
            *comment = '\0';
        }

        char* tokens[16];
        int n = tokenize(line, tokens, 16);
        if (n == 0) {
            continue;
        }

        if (strcmp(tokens[0], "event") == 0) {
            if (in_event) {
                fprintf(stderr, "%s:%d: nested 'event' block\n", argv[1], line_no);
                return 1;
            }
            memset(&current, 0, sizeof(current));
            in_event = true;
            continue;
        }

        if (strcmp(tokens[0], "end") == 0) {
            if (!in_event) {
                fprintf(stderr, "%s:%d: 'end' without matching 'event'\n", argv[1], line_no);
                return 1;
            }
            if (event_count >= MAX_EVENTS) {
                fprintf(stderr, "error: too many events (max %d)\n", MAX_EVENTS);
                return 1;
            }
            events[event_count++] = current;
            in_event = false;
            continue;
        }

        if (!in_event) {
            fprintf(stderr, "%s:%d: '%s' outside an event block\n", argv[1], line_no, tokens[0]);
            return 1;
        }

        const char* key = tokens[0];

        if (strcmp(key, "id") == 0 && n >= 2) {
            current.event_id = (uint16_t)atoi(tokens[1]);
        } else if (strcmp(key, "name") == 0 && n >= 2) {
            strncpy(current.name, tokens[1], EVENT_NAME_MAX_LEN - 1);
        } else if (strcmp(key, "min_age") == 0 && n >= 2) {
            current.min_age = (uint8_t)atoi(tokens[1]);
        } else if (strcmp(key, "max_age") == 0 && n >= 2) {
            current.max_age = (uint8_t)atoi(tokens[1]);
        } else if (strcmp(key, "weight") == 0 && n >= 2) {
            current.weight_base = (uint16_t)atoi(tokens[1]);
        } else if (strcmp(key, "required_trait") == 0 && n >= 2) {
            uint32_t bit = flag_bit_from_name(tokens[1]);
            if (bit == 0) {
                fprintf(stderr, "%s:%d: unknown trait '%s'\n", argv[1], line_no, tokens[1]);
                return 1;
            }
            current.required_trait_mask |= bit;
        } else if (strcmp(key, "forbidden_trait") == 0 && n >= 2) {
            uint32_t bit = flag_bit_from_name(tokens[1]);
            if (bit == 0) {
                fprintf(stderr, "%s:%d: unknown trait '%s'\n", argv[1], line_no, tokens[1]);
                return 1;
            }
            current.forbidden_trait_mask |= bit;
        } else if (strcmp(key, "trait_delta") == 0 && n >= 3) {
            int idx = trait_id_from_name(tokens[1]);
            if (idx < 0) {
                fprintf(stderr, "%s:%d: unknown trait '%s'\n", argv[1], line_no, tokens[1]);
                return 1;
            }
            current.trait_deltas[idx] = (int8_t)atoi(tokens[2]);
        } else if (strcmp(key, "flag_set") == 0 && n >= 2) {
            current.flag_set |= flag_bit_from_name(tokens[1]);
        } else if (strcmp(key, "flag_clear") == 0 && n >= 2) {
            current.flag_clear |= flag_bit_from_name(tokens[1]);
        } else if (strcmp(key, "requires_partner") == 0 && n >= 2) {
            current.requires_partner = parse_bool(tokens[1]) ? 1 : 0;
        } else if (strcmp(key, "partner_source") == 0 && n >= 2) {
            current.partner_source = (strcmp(tokens[1], "existing_relation") == 0)
                ? PARTNER_SOURCE_EXISTING_RELATION
                : PARTNER_SOURCE_ANY_POPULATION;
        } else if (strcmp(key, "partner_required_status") == 0 && n >= 2) {
            int s = status_from_name(tokens[1]);
            if (s < 0) {
                fprintf(stderr, "%s:%d: unknown status '%s'\n", argv[1], line_no, tokens[1]);
                return 1;
            }
            current.partner_required_status = (uint8_t)s;
        } else if (strcmp(key, "partner_exclude_spouse") == 0 && n >= 2) {
            current.partner_exclude_spouse = parse_bool(tokens[1]) ? 1 : 0;
        } else if (strcmp(key, "partner_min_age") == 0 && n >= 2) {
            current.partner_min_age = (uint8_t)atoi(tokens[1]);
        } else if (strcmp(key, "partner_max_age") == 0 && n >= 2) {
            current.partner_max_age = (uint8_t)atoi(tokens[1]);
        } else if (strcmp(key, "partner_required_trait") == 0 && n >= 2) {
            current.partner_required_trait_mask |= flag_bit_from_name(tokens[1]);
        } else if (strcmp(key, "partner_forbidden_trait") == 0 && n >= 2) {
            current.partner_forbidden_trait_mask |= flag_bit_from_name(tokens[1]);
        } else if (strcmp(key, "partner_trait_delta") == 0 && n >= 3) {
            int idx = trait_id_from_name(tokens[1]);
            if (idx < 0) {
                fprintf(stderr, "%s:%d: unknown trait '%s'\n", argv[1], line_no, tokens[1]);
                return 1;
            }
            current.partner_trait_deltas[idx] = (int8_t)atoi(tokens[2]);
        } else if (strcmp(key, "partner_flag_set") == 0 && n >= 2) {
            current.partner_flag_set |= flag_bit_from_name(tokens[1]);
        } else if (strcmp(key, "partner_flag_clear") == 0 && n >= 2) {
            current.partner_flag_clear |= flag_bit_from_name(tokens[1]);
        } else if (strcmp(key, "sets_relation_status") == 0 && n >= 2) {
            int s = status_from_name(tokens[1]);
            if (s < 0) {
                fprintf(stderr, "%s:%d: unknown status '%s'\n", argv[1], line_no, tokens[1]);
                return 1;
            }
            current.sets_relation_status = (uint8_t)s;
        } else if (strcmp(key, "relation_delta") == 0 && n >= 3) {
            int delta = atoi(tokens[2]);
            if (strcmp(tokens[1], "friendship") == 0) {
                current.relation_friendship_delta = (int8_t)delta;
            } else if (strcmp(tokens[1], "romance") == 0) {
                current.relation_romance_delta = (int8_t)delta;
            } else if (strcmp(tokens[1], "lust") == 0) {
                current.relation_lust_delta = (int8_t)delta;
            } else {
                fprintf(stderr, "%s:%d: unknown relation axis '%s'\n", argv[1], line_no, tokens[1]);
                return 1;
            }
        } else {
            fprintf(stderr, "%s:%d: unknown key '%s'\n", argv[1], line_no, key);
            return 1;
        }
    }

    fclose(in);

    if (in_event) {
        fprintf(stderr, "%s: error: unterminated 'event' block at end of file\n", argv[1]);
        return 1;
    }

    FILE* out = fopen(argv[2], "wb");
    if (out == NULL) {
        fprintf(stderr, "error: cannot open %s for writing\n", argv[2]);
        return 1;
    }

    EventFileHeader header;
    header.magic = EVENT_FILE_MAGIC;
    header.version = EVENT_FILE_VERSION;
    header.event_count = event_count;

    bool ok = fwrite(&header, sizeof(header), 1, out) == 1;
    ok = ok && (event_count == 0 || fwrite(events, sizeof(EventDef), event_count, out) == event_count);
    fclose(out);

    if (!ok) {
        fprintf(stderr, "error: failed writing %s\n", argv[2]);
        return 1;
    }

    printf("event_compiler: compiled %u event(s) from %s to %s\n", event_count, argv[1], argv[2]);
    return 0;
}
