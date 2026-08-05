// Regression tests for the bounds of the JSON scanner (issue #448).
//
// gravity_vm_loadbuffer() and json_parse() take an explicit length and make no
// promise that the buffer is NUL terminated, so every lookahead in the scanner has
// to stay inside [buffer, buffer + length). The CLI happens to hide any mistake
// here because file_read() over-allocates one byte and writes a terminator, so
// these cases can only be reached through the C API -- which is why they live in a
// C test instead of a .gravity or .json fixture.
//
// Every input below is copied into an exact sized heap allocation, so an over-read
// of even a single byte is caught when this is built with -fsanitize=address:
//
//     make jsontest CC="clang -fsanitize=address,undefined"
//     ./jsontest
//
// Without a sanitizer the test still checks the behaviour the bounds bugs broke:
// truncated input must be rejected and well formed input must survive unchanged.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gravity_json.h"
#include "gravity_vm.h"
#include "gravity_core.h"

static int failures = 0;
static int checks = 0;

static void check (int ok, const char *what) {
    ++checks;
    if (ok) return;
    ++failures;
    printf("Fail! %s\n", what);
}

// parse a copy of buffer that is exactly len bytes long and has no terminator
static json_value *parse_exact (const char *buffer, size_t len) {
    char *exact = (char *)malloc(len ? len : 1);
    if (!exact) { printf("Fail! out of memory\n"); exit(1); }
    memcpy(exact, buffer, len);

    json_value *value = json_parse(exact, len);

    free(exact);
    return value;
}

// Same, but with one extra byte placed just past the end that the scanner is not
// allowed to look at. trap is chosen to complete the truncated token, so a parser
// that reads it returns a value instead of an error: that turns an out of bounds
// read into a wrong answer this test can see without help from a sanitizer.
static json_value *parse_with_trap (const char *buffer, size_t len, char trap) {
    char *padded = (char *)malloc(len + 1);
    if (!padded) { printf("Fail! out of memory\n"); exit(1); }
    memcpy(padded, buffer, len);
    padded[len] = trap;

    json_value *value = json_parse(padded, len);

    free(padded);
    return value;
}

// ---------------------------------------------------------------------------
// truncated values: the scanner used to read one byte past the buffer while
// looking ahead at "true"/"false"/"null" and at \uXXXX escapes
// ---------------------------------------------------------------------------

static const char *const truncated[] = {
    "tru", "fals", "nul",                       // literals cut one byte short
    "t", "tr", "f", "fa", "fal", "n", "nu",     // and cut shorter still
    "\"\\uD80", "\"\\u00", "\"\\u0", "\"\\u",   // \uXXXX escape cut short
    "\"\\ud800\\ud80", "\"\\ud800\\u", "\"\\ud800\\\\",  // trailing surrogate cut short
    "[tru", "[fals", "[nul", "{\"a\":tru", "{\"a\":\"\\uD80",
};

static void test_truncated (void) {
    for (size_t i = 0; i < sizeof(truncated) / sizeof(truncated[0]); ++i) {
        json_value *value = parse_exact(truncated[i], strlen(truncated[i]));
        char what[128];
        snprintf(what, sizeof(what), "truncated input `%s` was accepted", truncated[i]);
        check(value == NULL, what);
        json_value_free(value);
    }

    // A literal cut one byte short, with the missing byte sitting just past the
    // end of the buffer. Reading it completes the literal at top level, so the
    // parse succeeds and the check below fails on a build with no sanitizer.
    // Only these three are decisive: completing a truncated \uXXXX escape still
    // leaves the string unterminated, so the escape guards are covered by the
    // exact sized allocations above and need ASan to be seen.
    static const struct { const char *text; char trap; } traps[] = {
        {"tru",  'e'},
        {"fals", 'e'},
        {"nul",  'l'},
    };

    for (size_t i = 0; i < sizeof(traps) / sizeof(traps[0]); ++i) {
        json_value *value = parse_with_trap(traps[i].text, strlen(traps[i].text), traps[i].trap);
        char what[128];
        snprintf(what, sizeof(what), "`%s` was completed by reading the byte past the buffer",
                 traps[i].text);
        check(value == NULL, what);
        json_value_free(value);
    }
}

// ---------------------------------------------------------------------------
// every prefix of every seed: wherever the buffer stops, the scanner must not
// read past it. A prefix is allowed to parse or to be rejected, so the only
// assertion here is that the full seed still parses -- for the prefixes the
// sanitizer is the oracle.
// ---------------------------------------------------------------------------

static const char *const seeds[] = {
    "{\"key\":\"value\"}", "[1,2,3]", "true", "false", "null", "\"\\u0041\\uD83D\\uDE00\"",
    "{\"a\":{\"b\":[1,-2.5e+3,null,false]}}", "-0.0e-1", "[[[]]]", "{\"\":\"\"}",
    "\"\\\\\\\"\\/\\b\\f\\n\\r\\t\"", "  \t\r\n{\"x\":true}  ",
};

static void test_every_prefix (void) {
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        size_t full = strlen(seeds[i]);

        for (size_t len = 0; len < full; ++len)
            json_value_free(parse_exact(seeds[i], len));

        json_value *value = parse_exact(seeds[i], full);
        char what[128];
        snprintf(what, sizeof(what), "valid input `%s` was rejected", seeds[i]);
        check(value != NULL, what);
        json_value_free(value);
    }
}

// ---------------------------------------------------------------------------
// escapes and literals that end exactly on the last byte are legal and must not
// be rejected by an over-corrected bounds check
// ---------------------------------------------------------------------------

static void test_exact_fit (void) {
    static const struct { const char *text; json_type type; } cases[] = {
        {"true",  json_boolean},
        {"false", json_boolean},
        {"null",  json_null},
        {"\"\\u0041\"", json_string},
        {"\"\\uD83D\\uDE00\"", json_string},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        json_value *value = parse_exact(cases[i].text, strlen(cases[i].text));
        char what[128];
        snprintf(what, sizeof(what), "`%s` ending on the last byte was rejected", cases[i].text);
        check(value != NULL && value->type == cases[i].type, what);
        json_value_free(value);
    }

    // check the decoded content too, so a bounds fix cannot pass by silently
    // dropping the last escape
    json_value *value = parse_exact("\"\\u0041\"", 8);
    check(value != NULL && value->type == json_string
          && value->u.string.length == 1 && value->u.string.ptr[0] == 'A',
          "\\u0041 did not decode to `A`");
    json_value_free(value);

    // U+1F600 encodes to the four UTF-8 bytes F0 9F 98 80
    value = parse_exact("\"\\uD83D\\uDE00\"", 14);
    check(value != NULL && value->type == json_string
          && value->u.string.length == 4
          && memcmp(value->u.string.ptr, "\xF0\x9F\x98\x80", 4) == 0,
          "surrogate pair did not decode to U+1F600");
    json_value_free(value);
}

// ---------------------------------------------------------------------------
// objects: the first pass tallies the size of the key strings inside
// u.object.values, which used to be done with arithmetic on a null pointer
// ---------------------------------------------------------------------------

static void test_object_keys (void) {
    static const char text[] =
        "{\"a\":1,\"bb\":2,\"ccc\":3,\"\":4,\"dddddddddddddddddddd\":5}";
    static const char *const names[] = {"a", "bb", "ccc", "", "dddddddddddddddddddd"};

    json_value *value = parse_exact(text, sizeof(text) - 1);
    check(value != NULL && value->type == json_object, "object with mixed key lengths was rejected");
    if (!value || value->type != json_object) { json_value_free(value); return; }

    check(value->u.object.length == 5, "wrong number of object entries");

    for (unsigned int i = 0; i < value->u.object.length && i < 5; ++i) {
        json_object_entry *entry = &value->u.object.values[i];
        char what[160];
        snprintf(what, sizeof(what), "entry %u: expected key `%s`, got `%s`", i, names[i], entry->name);
        check(entry->name_length == strlen(names[i])
              && strcmp(entry->name, names[i]) == 0, what);
        snprintf(what, sizeof(what), "entry %u: expected value %u", i, i + 1);
        check(entry->value != NULL && entry->value->type == json_integer
              && entry->value->u.integer == (json_int_t)(i + 1), what);
    }

    json_value_free(value);
}

// ---------------------------------------------------------------------------
// the input reported in issue #448, driven through the entry point it came in by
// ---------------------------------------------------------------------------

// 520 bytes of malformed bytecode: many double quotes and NUL bytes, so the
// parser is still mid-scan when the buffer ends
static const char issue448_poc[] =
    "0a20202020202020207b0a2020222222222222222222222222222222000000a5a5a5a7a5a5a5a5"
    "a5a5a5a5a5a5a5a5a5a5a4756e63206910697436000010006e322c6e332c6e34290a202020207b"
    "0a202020202020202078203d206e66756e202020202020202079203d206e32bb0a202043202020"
    "2020773f32206e0100102020202020202020202020202020202020202020202020202020202020"
    "2020202020202020202000000100206e333b0a2020202020202020201718171717172020203078"
    "394cae2869207d0a0a20171716fa17171717101717181717171720202030783939393939393939"
    "393939393939393939393939393939393939393939282828a82828202020766172743600001000"
    "6e322c392828282828282020207661727436000010006e322c6e332c6e332c6e34290a3e202069"
    "3b0a3b0a0000020000a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a4756e632069106974360026"
    "262626742020202020773f32206e332828282828282828282828282828282020207661723b0a20"
    "20202020202020202020202020202020202020202020ff6820303d20313b0a2020202020202020"
    "7d0a0a202020202020202072657475726f20662b963b0a202020207d1f7d0a0a0a66756e63206d"
    "61696e28290a7b0a202020207661722020202020ff68202b3e37313b0a20202020002020207d0a"
    "0a20202020202061202b20623b";

static int hex_digit (char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static void report_error (gravity_vm *vm, error_type_t error_type, const char *description,
                          error_desc_t error_desc, void *xdata) {
    #pragma unused(vm, error_type, description, error_desc, xdata)
}

static void test_loadbuffer (void) {
    const size_t hexlen = sizeof(issue448_poc) - 1;
    const size_t len = hexlen / 2;

    check(hexlen % 2 == 0 && len == 520, "issue #448 sample is not 520 bytes");

    // an exact sized buffer, exactly as a fuzz harness or an embedder loading
    // bytecode out of mapped memory would pass it
    char *poc = (char *)malloc(len);
    if (!poc) { printf("Fail! out of memory\n"); exit(1); }
    for (size_t i = 0; i < len; ++i)
        poc[i] = (char)((hex_digit(issue448_poc[i * 2]) << 4) | hex_digit(issue448_poc[i * 2 + 1]));

    gravity_delegate_t delegate = {.error_callback = report_error};
    gravity_vm *vm = gravity_vm_new(&delegate);
    check(vm != NULL, "unable to create a VM");

    if (vm) {
        gravity_closure_t *closure = gravity_vm_loadbuffer(vm, poc, len);
        check(closure == NULL, "issue #448 sample was accepted by gravity_vm_loadbuffer");

        // the degenerate case of the same path
        closure = gravity_vm_loadbuffer(vm, "", 0);
        check(closure == NULL, "empty buffer was accepted by gravity_vm_loadbuffer");

        gravity_vm_free(vm);
    }

    free(poc);
    gravity_core_free();
}

int main (void) {
    test_truncated();
    test_every_prefix();
    test_exact_fit();
    test_object_keys();
    test_loadbuffer();

    printf("Checks run successfully: %d/%d. %d failed\n", checks - failures, checks, failures);
    return (failures == 0) ? 0 : 1;
}
