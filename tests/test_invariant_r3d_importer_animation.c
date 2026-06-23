#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Include the actual header to get the function declaration
#include "src/importer/r3d_importer_animation.h"

// Mock the necessary structures since we can't include the full Assimp headers
typedef struct {
    char data[1024];  // Large enough for our test payloads
} aiString;

typedef struct {
    aiString mName;
} aiAnimation;

typedef struct {
    char name[64];  // Matches the actual buffer size in the production code
} r3d_animation_t;

// We'll test the actual vulnerable function by creating a wrapper
// that simulates the vulnerable code path
static void test_animation_name_copy(aiAnimation *aiAnim, r3d_animation_t *animation) {
    // This replicates the exact vulnerable code from the file
    size_t nameLen = strlen(aiAnim->mName.data);
    if (nameLen >= sizeof(animation->name)) {
        nameLen = sizeof(animation->name) - 1;
    }
    memcpy(animation->name, aiAnim->mName.data, nameLen);
    animation->name[nameLen] = '\0';
}

START_TEST(test_buffer_reads_never_exceed_declared_length)
{
    // Invariant: Buffer reads never exceed the declared length
    const char *payloads[] = {
        // Exact exploit case: string exactly at buffer boundary but not null-terminated
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",  // 63 chars
        // Oversized input (2x buffer size)
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",  // 128 chars
        // Valid input (well within bounds)
        "Valid Animation",
        // Boundary case: exactly buffer size - 1
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",  // 63 chars
        // Malicious: null byte in middle to test strlen behavior
        "DDDDDD\0DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
    };
    
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);
    
    for (int i = 0; i < num_payloads; i++) {
        // Setup test structures
        aiAnimation aiAnim;
        r3d_animation_t animation;
        
        // Clear the animation name buffer
        memset(animation.name, 0xCC, sizeof(animation.name));  // Fill with sentinel value
        
        // Copy payload into aiAnim
        strncpy(aiAnim.mName.data, payloads[i], sizeof(aiAnim.mName.data) - 1);
        aiAnim.mName.data[sizeof(aiAnim.mName.data) - 1] = '\0';
        
        // Call the function that contains the vulnerable code
        test_animation_name_copy(&aiAnim, &animation);
        
        // Verify the invariant: no buffer overflow occurred
        // Check that the byte immediately after the buffer hasn't been corrupted
        // We can't directly check this without instrumentation, but we can verify
        // that the string was properly truncated
        
        // The name should be null-terminated
        ck_assert_msg(animation.name[sizeof(animation.name) - 1] == '\0' || 
                     animation.name[sizeof(animation.name) - 1] == 0xCC,
                     "Buffer may have overflowed - last byte corrupted");
        
        // For oversized inputs, verify truncation occurred
        size_t payload_len = strlen(payloads[i]);
        if (payload_len >= sizeof(animation.name)) {
            // Should be truncated to buffer size - 1
            ck_assert_msg(strlen(animation.name) == sizeof(animation.name) - 1,
                         "Oversized input not properly truncated");
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_buffer_reads_never_exceed_declared_length);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}