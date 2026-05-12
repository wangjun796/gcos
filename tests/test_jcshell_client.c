/**
 * @file test_jcshell_client.c
 * @brief Test client for JCShell server
 * 
 * Connects to JCShell server on port 9000, sends TLP224 messages,
 * and verifies responses.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#define SERVER_PORT     9000
#define SERVER_ADDR     "127.0.0.1"
#define BUFFER_SIZE     1024

/* TLP224 constants */
#define TLP_ACK         0x60
#define TLP_EOT         0x03
#define TLP_POWER_UP    0x6E
#define TLP_ISO_OUTPUT  0xDB

/**
 * @brief Compute LRC (XOR of all bytes)
 */
static unsigned char compute_lrc(const unsigned char* buf, int length) {
    unsigned char lrc = 0;
    for (int i = 0; i < length; i++) {
        lrc ^= buf[i];
    }
    return lrc;
}

/**
 * @brief Convert byte to ASCII hex pair
 */
static void byte_to_ascii_hex(unsigned char byte, char* hi, char* lo) {
    unsigned char h = (byte >> 4) & 0x0F;
    unsigned char l = byte & 0x0F;
    
    *hi = (h < 10) ? (char)(h + '0') : (char)(h - 10 + 'A');
    *lo = (l < 10) ? (char)(l + '0') : (char)(l - 10 + 'A');
}

/**
 * @brief Send TLP224 encoded message
 */
static int send_tlp224_message(int sock, const unsigned char* msg_data, int size) {
    char buffer[BUFFER_SIZE];
    int pos = 0;
    
    /* Encode each byte as 2 ASCII hex characters */
    for (int i = 0; i < size; i++) {
        char hi, lo;
        byte_to_ascii_hex(msg_data[i], &hi, &lo);
        buffer[pos++] = hi;
        buffer[pos++] = lo;
    }
    
    /* Append EOT */
    buffer[pos++] = (char)TLP_EOT;
    
    /* Send */
    int sent = send(sock, buffer, pos, 0);
    if (sent != pos) {
        printf("ERROR: Failed to send message (sent %d/%d)\n", sent, pos);
        return -1;
    }
    
    printf("Sent TLP224 (%d bytes binary -> %d ASCII chars + EOT)\n", size, pos - 1);
    printf("  Hex: ");
    for (int i = 0; i < size && i < 20; i++) {
        printf("%02X", msg_data[i]);
    }
    if (size > 20) printf("...");
    printf("\n");
    
    return 0;
}

/**
 * @brief Receive TLP224 encoded message
 */
static int receive_tlp224_message(int sock, unsigned char* msg_data, int max_size) {
    char buffer[BUFFER_SIZE];
    int got = 0;
    
    /* Read until EOT */
    while (1) {
        char c;
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) {
            printf("ERROR: Connection closed or error\n");
            return -1;
        }
        
        if (c == (char)TLP_EOT) {
            break;
        }
        
        if (got >= BUFFER_SIZE - 1) {
            printf("ERROR: Buffer overflow\n");
            return -1;
        }
        
        buffer[got++] = c;
    }
    
    /* Decode ASCII hex pairs to binary */
    int msg_len = 0;
    for (int i = 0; i < got; i += 2) {
        if (i + 1 >= got) break;
        
        /* Decode high nibble */
        int hi = buffer[i] - '0';
        if (hi > 9) hi -= 7;
        
        /* Decode low nibble */
        int lo = buffer[i + 1] - '0';
        if (lo > 9) lo -= 7;
        
        if (msg_len >= max_size) {
            printf("ERROR: Message too long\n");
            return -1;
        }
        
        msg_data[msg_len++] = (unsigned char)((hi << 4) | lo);
    }
    
    printf("Received TLP224 (%d ASCII chars -> %d bytes binary)\n", got, msg_len);
    printf("  Hex: ");
    for (int i = 0; i < msg_len && i < 20; i++) {
        printf("%02X", msg_data[i]);
    }
    if (msg_len > 20) printf("...");
    printf("\n");
    
    return msg_len;
}

/**
 * @brief Build POWER_UP message
 */
static int build_powerup_message(unsigned char* msg) {
    msg[0] = TLP_ACK;
    msg[1] = 0x00;  /* Length high */
    msg[2] = 0x04;  /* Length low */
    msg[3] = TLP_POWER_UP;
    msg[4] = 0x00;
    msg[5] = 0x00;
    msg[6] = 0x00;
    msg[7] = compute_lrc(msg, 7);
    
    return 8;
}

/**
 * @brief Build SELECT APDU message (ISO_OUTPUT)
 */
static int build_select_message(unsigned char* msg) {
    /* SELECT AID: 00 A4 04 00 08 A0 00 00 00 03 00 00 00 */
    unsigned char apdu[] = {
        0x00, 0xA4, 0x04, 0x00, 0x08,
        0xA0, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00
    };
    
    msg[0] = TLP_ACK;
    msg[1] = 0x00;  /* Length high */
    msg[2] = 0x06;  /* Length low (6 bytes: CLA INS P1 P2 Le) */
    msg[3] = TLP_ISO_OUTPUT;
    msg[4] = apdu[0];  /* CLA */
    msg[5] = apdu[1];  /* INS */
    msg[6] = apdu[2];  /* P1 */
    msg[7] = apdu[3];  /* P2 */
    msg[8] = 0x00;     /* Le */
    msg[9] = compute_lrc(msg, 9);
    
    return 10;
}

/**
 * @brief Validate response message
 */
static int validate_response(const unsigned char* msg, int len) {
    if (len < 5) {
        printf("ERROR: Response too short (%d bytes)\n", len);
        return 0;
    }
    
    /* Check ACK */
    if (msg[0] != TLP_ACK) {
        printf("ERROR: Expected ACK (0x%02X), got 0x%02X\n", TLP_ACK, msg[0]);
        return 0;
    }
    
    /* Validate LRC */
    unsigned char received_lrc = msg[len - 1];
    unsigned char computed_lrc = compute_lrc(msg, len - 1);
    
    if (received_lrc != computed_lrc) {
        printf("ERROR: LRC mismatch (received=0x%02X, computed=0x%02X)\n",
               received_lrc, computed_lrc);
        return 0;
    }
    
    printf("✓ Response validation passed\n");
    return 1;
}

/**
 * @brief Main test function
 */
int main(void) {
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
    
    printf("========================================\n");
    printf("  JCShell Client Test\n");
    printf("========================================\n\n");
    
    /* Connect to server */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("ERROR: Failed to create socket\n");
        return 1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);
    
    printf("Connecting to %s:%d...\n", SERVER_ADDR, SERVER_PORT);
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("ERROR: Connection failed\n");
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        return 1;
    }
    
    printf("Connected!\n\n");
    
    unsigned char msg_buffer[BUFFER_SIZE];
    int msg_len;
    
    /* Test 1: Send POWER_UP */
    printf("=== Test 1: POWER_UP ===\n");
    msg_len = build_powerup_message(msg_buffer);
    if (send_tlp224_message(sock, msg_buffer, msg_len) != 0) {
        goto cleanup;
    }
    
    msg_len = receive_tlp224_message(sock, msg_buffer, BUFFER_SIZE);
    if (msg_len < 0) {
        goto cleanup;
    }
    
    if (!validate_response(msg_buffer, msg_len)) {
        goto cleanup;
    }
    
    printf("Expected: ATR response\n");
    printf("Got: ");
    for (int i = 0; i < msg_len && i < 20; i++) {
        printf("%02X ", msg_buffer[i]);
    }
    printf("\n\n");
    
    /* Test 2: Send SELECT APDU */
    printf("=== Test 2: SELECT APDU ===\n");
    msg_len = build_select_message(msg_buffer);
    if (send_tlp224_message(sock, msg_buffer, msg_len) != 0) {
        goto cleanup;
    }
    
    msg_len = receive_tlp224_message(sock, msg_buffer, BUFFER_SIZE);
    if (msg_len < 0) {
        goto cleanup;
    }
    
    if (!validate_response(msg_buffer, msg_len)) {
        goto cleanup;
    }
    
    /* Extract SW1SW2 */
    if (msg_len >= 6) {
        unsigned char sw1 = msg_buffer[msg_len - 3];
        unsigned char sw2 = msg_buffer[msg_len - 2];
        printf("Status Word: %02X %02X\n", sw1, sw2);
    }
    
    printf("\n========================================\n");
    printf("  All tests completed successfully!\n");
    printf("========================================\n");
    
cleanup:
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
    
    return 0;
}
