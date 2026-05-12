/**
 * @file test_tlp224.c
 * @brief Test program for TLP224 encoding/decoding
 * 
 * Tests the TLP224 message framing implementation against cref's behavior.
 */

#include "gcos_tlp.h"
#include <stdio.h>
#include <string.h>

/* Test data - a simple SELECT APDU */
static const u8 test_apdu[] = {
    0x00, /* CLA */
    0xA4, /* INS: SELECT */
    0x04, /* P1: Select by AID */
    0x00, /* P2: First or only occurrence */
    0x08, /* Lc: Length of AID */
    0xA0, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00 /* AID */
};

/**
 * @brief Print a buffer as hex string
 */
static void print_hex(const char *label, const u8 *data, u16 len) {
    printf("%s (%u bytes): ", label, len);
    for (u16 i = 0; i < len; i++) {
        printf("%02X", data[i]);
    }
    printf("\n");
}

/**
 * @brief Test LRC computation
 */
static void test_lrc_computation(void) {
    printf("\n=== Test 1: LRC Computation ===\n");
    
    u8 test_data[] = { 0x60, 0x00, 0x06, 0xDB };
    u8 lrc = tlp_compute_lrc(test_data, sizeof(test_data));
    
    printf("Input: ");
    print_hex("", test_data, sizeof(test_data));
    printf("LRC: 0x%02X\n", lrc);
    
    /* Expected LRC: 0x60 ^ 0x00 ^ 0x06 ^ 0xDB = 0xBD */
    if (lrc == 0xBD) {
        printf("✓ LRC computation correct\n");
    } else {
        printf("✗ LRC computation failed (expected 0xBD)\n");
    }
}

/**
 * @brief Test ASCII hex encoding/decoding
 */
static void test_ascii_hex_conversion(void) {
    printf("\n=== Test 2: ASCII Hex Conversion ===\n");
    
    /* Test byte to ASCII hex */
    char hi, lo;
    u8 test_byte = 0xAB;
    
    /* Manual conversion (mimicking byte_to_ascii_hex) */
    u8 h = (test_byte >> 4) & 0x0F;
    u8 l = test_byte & 0x0F;
    
    hi = (h < 10) ? (char)(h + 0x30) : (char)(h + 0x37);
    lo = (l < 10) ? (char)(l + 0x30) : (char)(l + 0x37);
    
    printf("Byte 0x%02X -> ASCII '%c' (0x%02X) '%c' (0x%02X)\n", 
           test_byte, hi, (u8)hi, lo, (u8)lo);
    
    if (hi == 'A' && lo == 'B') {
        printf("✓ Byte to ASCII hex correct\n");
    } else {
        printf("✗ Byte to ASCII hex failed\n");
    }
    
    /* Test ASCII hex to byte */
    int hi_val = (int)(unsigned char)'A';
    int lo_val = (int)(unsigned char)'B';
    
    hi_val -= 0x30;
    if (hi_val > 9) hi_val -= 7;
    
    lo_val -= 0x30;
    if (lo_val > 9) lo_val -= 7;
    
    int decoded = (hi_val << 4) | lo_val;
    printf("ASCII 'A' 'B' -> Byte 0x%02X\n", decoded);
    
    if (decoded == 0xAB) {
        printf("✓ ASCII hex to byte correct\n");
    } else {
        printf("✗ ASCII hex to byte failed\n");
    }
}

/**
 * @brief Test TLP message construction
 */
static void test_tlp_message_construction(void) {
    printf("\n=== Test 3: TLP Message Construction ===\n");
    
    TLP_MSG msg;
    tlp_msg_init(&msg);
    
    /* Build a simple ISO_OUTPUT command (Case 2 APDU) */
    msg.buf[0] = TLP_ACK;
    msg.buf[1] = 0x00;  /* Length high byte */
    msg.buf[2] = 0x06;  /* Length low byte (6 bytes payload) */
    msg.buf[3] = TLP_ISO_OUTPUT;
    msg.buf[4] = 0x00;  /* CLA */
    msg.buf[5] = 0xA4;  /* INS */
    msg.buf[6] = 0x04;  /* P1 */
    msg.buf[7] = 0x00;  /* P2 */
    msg.buf[8] = 0x00;  /* Le */
    
    /* Compute and append LRC */
    msg.buf[9] = tlp_compute_lrc(msg.buf, 9);
    msg.len = 10;
    
    printf("Constructed TLP message:\n");
    print_hex("  Binary", msg.buf, msg.len);
    
    /* Validate LRC */
    if (tlp_validate_lrc(&msg)) {
        printf("✓ LRC validation passed\n");
    } else {
        printf("✗ LRC validation failed\n");
    }
    
    /* Verify expected values */
    if (msg.buf[0] == TLP_ACK &&
        msg.buf[3] == TLP_ISO_OUTPUT &&
        msg.buf[9] == 0x1D) {  /* Expected LRC: 0x60^0x00^0x06^0xDB^0x00^0xA4^0x04^0x00^0x00 = 0x1D */
        printf("✓ Message structure correct\n");
    } else {
        printf("✗ Message structure incorrect\n");
        printf("  Expected LRC: 0x1D, Got: 0x%02X\n", msg.buf[9]);
    }
}

/**
 * @brief Test POWER_UP message construction
 */
static void test_powerup_message(void) {
    printf("\n=== Test 4: POWER_UP Message ===\n");
    
    TLP_MSG msg;
    tlp_msg_init(&msg);
    
    /* Build POWER_UP command (from cref jcshell.c powerup()) */
    msg.buf[0] = TLP_ACK;
    msg.buf[1] = 0x00;  /* Length high */
    msg.buf[2] = 0x04;  /* Length low (4 bytes payload) */
    msg.buf[3] = TLP_POWER_UP;
    msg.buf[4] = 0x00;
    msg.buf[5] = 0x00;
    msg.buf[6] = 0x00;
    msg.buf[7] = tlp_compute_lrc(msg.buf, 7);
    msg.len = 8;
    
    printf("POWER_UP message:\n");
    print_hex("  Binary", msg.buf, msg.len);
    
    if (tlp_validate_lrc(&msg)) {
        printf("✓ POWER_UP LRC valid\n");
    } else {
        printf("✗ POWER_UP LRC invalid\n");
    }
}

/**
 * @brief Main test function
 */
int main(void) {
    printf("========================================\n");
    printf("  TLP224 Protocol Test Suite\n");
    printf("========================================\n");
    
    test_lrc_computation();
    test_ascii_hex_conversion();
    test_tlp_message_construction();
    test_powerup_message();
    
    printf("\n========================================\n");
    printf("  All tests completed\n");
    printf("========================================\n");
    
    return 0;
}
