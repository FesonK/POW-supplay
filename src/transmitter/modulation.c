/**
 * Modulation Utilities for POWER-SUPPLaY
 * 
 * FOR EDUCATIONAL AND RESEARCH PURPOSES ONLY
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

// CRC polynomial for CRC-8
#define CRC8_POLYNOMIAL 0x07

// Frame structure constants
#define PREAMBLE_LENGTH 8
#define PAYLOAD_SIZE 32
#define CRC_SIZE 8
#define FRAME_SIZE (PREAMBLE_LENGTH + PAYLOAD_SIZE + CRC_SIZE)

// Modulation parameters structure
typedef struct {
    int base_freq;
    int freq_spacing;
    int symbol_duration_ms;
    int guard_interval_ms;
} modulation_params_t;

// Frame structure
typedef struct {
    uint8_t preamble[PREAMBLE_LENGTH];
    uint8_t payload[PAYLOAD_SIZE];
    uint8_t crc;
} frame_t;

/**
 * Calculate CRC-8 checksum
 */
uint8_t calculate_crc8(const uint8_t* data, size_t length) {
    uint8_t crc = 0x00;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ CRC8_POLYNOMIAL;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

/**
 * Verify CRC-8 checksum
 */
int verify_crc8(const uint8_t* data, size_t length, uint8_t expected_crc) {
    uint8_t calculated_crc = calculate_crc8(data, length);
    return (calculated_crc == expected_crc);
}

/**
 * Generate preamble pattern
 * Standard alternating pattern for synchronization
 */
void generate_preamble(uint8_t* preamble, size_t length) {
    for (size_t i = 0; i < length; i++) {
        preamble[i] = (i % 2 == 0) ? 0xAA : 0x55;  // 10101010 / 01010101
    }
}

/**
 * Create frame from data
 */
int create_frame(frame_t* frame, const uint8_t* data, size_t data_length) {
    if (data_length > PAYLOAD_SIZE) {
        fprintf(stderr, "Error: Data too large for frame (max %d bytes)\n", PAYLOAD_SIZE);
        return -1;
    }
    
    // Generate preamble
    generate_preamble(frame->preamble, PREAMBLE_LENGTH);
    
    // Copy data to payload (pad with zeros if necessary)
    memset(frame->payload, 0, PAYLOAD_SIZE);
    memcpy(frame->payload, data, data_length);
    
    // Calculate CRC
    frame->crc = calculate_crc8(frame->payload, PAYLOAD_SIZE);
    
    return 0;
}

/**
 * Manchester encoding - encodes each bit as two symbols
 * 0 -> 01, 1 -> 10
 */
void manchester_encode(const uint8_t* input, uint8_t* output, size_t input_bytes) {
    size_t output_index = 0;
    
    for (size_t i = 0; i < input_bytes; i++) {
        uint8_t byte = input[i];
        uint16_t encoded = 0;
        
        for (int bit = 7; bit >= 0; bit--) {
            if ((byte >> bit) & 1) {
                // Bit 1 -> 10
                encoded = (encoded << 2) | 0b10;
            } else {
                // Bit 0 -> 01
                encoded = (encoded << 2) | 0b01;
            }
        }
        
        // Store encoded bytes
        output[output_index++] = (encoded >> 8) & 0xFF;
        output[output_index++] = encoded & 0xFF;
    }
}

/**
 * Manchester decoding - decodes two symbols back to one bit
 */
int manchester_decode(const uint8_t* input, uint8_t* output, size_t input_bytes) {
    if (input_bytes % 2 != 0) {
        fprintf(stderr, "Error: Invalid Manchester encoded data length\n");
        return -1;
    }
    
    size_t output_index = 0;
    
    for (size_t i = 0; i < input_bytes; i += 2) {
        uint16_t encoded = (input[i] << 8) | input[i + 1];
        uint8_t decoded = 0;
        
        for (int j = 0; j < 8; j++) {
            uint8_t symbol = (encoded >> (14 - j * 2)) & 0b11;
            
            if (symbol == 0b10) {
                decoded = (decoded << 1) | 1;
            } else if (symbol == 0b01) {
                decoded = (decoded << 1) | 0;
            } else {
                fprintf(stderr, "Error: Invalid Manchester symbol\n");
                return -1;
            }
        }
        
        output[output_index++] = decoded;
    }
    
    return 0;
}

/**
 * Hamming(7,4) error correction encoding
 * Encodes 4 data bits into 7 bits with error correction
 */
uint8_t hamming74_encode(uint8_t data) {
    // Extract 4 data bits
    uint8_t d1 = (data >> 0) & 1;
    uint8_t d2 = (data >> 1) & 1;
    uint8_t d3 = (data >> 2) & 1;
    uint8_t d4 = (data >> 3) & 1;
    
    // Calculate parity bits
    uint8_t p1 = d1 ^ d2 ^ d4;
    uint8_t p2 = d1 ^ d3 ^ d4;
    uint8_t p3 = d2 ^ d3 ^ d4;
    
    // Construct encoded byte
    uint8_t encoded = (p1 << 0) | (p2 << 1) | (d1 << 2) | 
                      (p3 << 3) | (d2 << 4) | (d3 << 5) | (d4 << 6);
    
    return encoded;
}

/**
 * Hamming(7,4) error correction decoding
 * Decodes 7 bits back to 4 data bits with single-bit error correction
 */
uint8_t hamming74_decode(uint8_t encoded) {
    // Extract bits
    uint8_t p1 = (encoded >> 0) & 1;
    uint8_t p2 = (encoded >> 1) & 1;
    uint8_t d1 = (encoded >> 2) & 1;
    uint8_t p3 = (encoded >> 3) & 1;
    uint8_t d2 = (encoded >> 4) & 1;
    uint8_t d3 = (encoded >> 5) & 1;
    uint8_t d4 = (encoded >> 6) & 1;
    
    // Calculate syndrome
    uint8_t s1 = p1 ^ d1 ^ d2 ^ d4;
    uint8_t s2 = p2 ^ d1 ^ d3 ^ d4;
    uint8_t s3 = p3 ^ d2 ^ d3 ^ d4;
    
    uint8_t syndrome = (s3 << 2) | (s2 << 1) | s1;
    
    // Correct single-bit error if syndrome is non-zero
    if (syndrome != 0) {
        encoded ^= (1 << (syndrome - 1));
        
        // Re-extract corrected data bits
        d1 = (encoded >> 2) & 1;
        d2 = (encoded >> 4) & 1;
        d3 = (encoded >> 5) & 1;
        d4 = (encoded >> 6) & 1;
    }
    
    // Return 4 data bits
    return (d4 << 3) | (d3 << 2) | (d2 << 1) | d1;
}

/**
 * Gray code encoding - reduces errors in multi-level signaling
 */
uint8_t gray_encode(uint8_t binary) {
    return binary ^ (binary >> 1);
}

/**
 * Gray code decoding
 */
uint8_t gray_decode(uint8_t gray) {
    uint8_t binary = gray;
    
    while (gray >>= 1) {
        binary ^= gray;
    }
    
    return binary;
}

/**
 * Calculate optimal frequency spacing to avoid interference
 */
int calculate_frequency_spacing(int base_freq, int num_channels, int bandwidth) {
    // Ensure minimum spacing to avoid overlap
    int min_spacing = 50;  // 50 Hz minimum
    int optimal_spacing = bandwidth / num_channels;
    
    if (optimal_spacing < min_spacing) {
        fprintf(stderr, "Warning: Frequency spacing too small, using minimum %d Hz\n", 
                min_spacing);
        return min_spacing;
    }
    
    return optimal_spacing;
}

/**
 * Generate frequency hopping sequence (pseudo-random)
 */
void generate_hopping_sequence(int* sequence, int length, int base_freq, 
                              int max_freq, int seed) {
    srand(seed);
    
    int range = max_freq - base_freq;
    int hop_size = range / length;
    
    for (int i = 0; i < length; i++) {
        // Generate pseudo-random frequency within range
        sequence[i] = base_freq + (rand() % range);
        
        // Ensure minimum separation between consecutive frequencies
        if (i > 0 && abs(sequence[i] - sequence[i-1]) < 100) {
            sequence[i] = base_freq + ((sequence[i-1] + 200) % range);
        }
    }
}

/**
 * Convert text to binary representation
 */
void text_to_binary(const char* text, uint8_t* binary, size_t* binary_len) {
    size_t text_len = strlen(text);
    *binary_len = text_len;
    
    for (size_t i = 0; i < text_len; i++) {
        binary[i] = (uint8_t)text[i];
    }
}

/**
 * Convert binary to hex string for display
 */
void binary_to_hex(const uint8_t* binary, size_t len, char* hex_str) {
    for (size_t i = 0; i < len; i++) {
        sprintf(hex_str + (i * 2), "%02X", binary[i]);
    }
    hex_str[len * 2] = '\0';
}

/**
 * Calculate Shannon capacity for channel
 */
double calculate_channel_capacity(double bandwidth, double snr_db) {
    // C = B * log2(1 + S/N)
    double snr_linear = pow(10, snr_db / 10.0);
    return bandwidth * log2(1 + snr_linear);
}

/**
 * Calculate theoretical maximum bit rate
 */
int calculate_max_bitrate(int bandwidth, double snr_db) {
    double capacity = calculate_channel_capacity(bandwidth, snr_db);
    return (int)capacity;
}

/**
 * Print modulation parameters
 */
void print_modulation_params(modulation_params_t* params) {
    printf("=== Modulation Parameters ===\n");
    printf("Base Frequency: %d Hz\n", params->base_freq);
    printf("Frequency Spacing: %d Hz\n", params->freq_spacing);
    printf("Symbol Duration: %d ms\n", params->symbol_duration_ms);
    printf("Guard Interval: %d ms\n", params->guard_interval_ms);
    printf("Effective Bit Rate: %.2f bps\n", 
           1000.0 / (params->symbol_duration_ms + params->guard_interval_ms));
    printf("=============================\n");
}

/**
 * Validate frequency is within human hearing range
 */
int is_audible_frequency(int freq) {
    return (freq >= 20 && freq <= 20000);
}

/**
 * Validate frequency is ultrasonic (above typical adult hearing)
 */
int is_ultrasonic_frequency(int freq) {
    return (freq > 18000 && freq <= 24000);
}

/**
 * Get recommended frequencies for covert channel
 */
void get_covert_frequencies(int* freq_list, int* count) {
    // Near-ultrasonic frequencies that are less detectable
    int covert_freqs[] = {18500, 19000, 19500, 20000, 20500, 21000, 21500, 22000};
    *count = sizeof(covert_freqs) / sizeof(covert_freqs[0]);
    
    for (int i = 0; i < *count; i++) {
        freq_list[i] = covert_freqs[i];
    }
}

// Export functions for use in other modules
#ifdef MODULATION_MAIN
int main() {
    printf("Modulation Utilities Test\n");
    printf("=========================\n\n");
    
    // Test CRC
    uint8_t test_data[] = "Hello";
    uint8_t crc = calculate_crc8(test_data, 5);
    printf("CRC-8 of 'Hello': 0x%02X\n", crc);
    
    // Test Manchester encoding
    uint8_t input[] = {0xAA, 0x55};
    uint8_t encoded[4];
    uint8_t decoded[2];
    
    manchester_encode(input, encoded, 2);
    printf("\nManchester encoding:\n");
    printf("Input:  0x%02X%02X\n", input[0], input[1]);
    printf("Output: 0x%02X%02X%02X%02X\n", 
           encoded[0], encoded[1], encoded[2], encoded[3]);
    
    manchester_decode(encoded, decoded, 4);
    printf("Decoded: 0x%02X%02X\n", decoded[0], decoded[1]);
    
    // Test channel capacity
    double capacity = calculate_channel_capacity(1000, 20);
    printf("\nChannel capacity (1kHz bandwidth, 20dB SNR): %.2f bps\n", capacity);
    
    return 0;
}
#endif