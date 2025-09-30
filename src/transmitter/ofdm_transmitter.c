/**
 * OFDM (Orthogonal Frequency-Division Multiplexing) Transmitter
 * 
 * Implements parallel data transmission using multiple CPU cores
 * Each core generates a different sub-carrier frequency
 * 
 * Based on POWER-SUPPLaY research by Mordechai Guri (2020)
 * FOR EDUCATIONAL AND RESEARCH PURPOSES ONLY
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <signal.h>
#include <math.h>

#define NANO_PER_SECOND 1000000000L
#define MAX_SUBCARRIERS 8
#define SYMBOL_DURATION_MS 100
#define GUARD_INTERVAL_MS 10

// Global control variables
volatile int running = 1;
volatile int transmitting[MAX_SUBCARRIERS] = {0};

// OFDM parameters
typedef struct {
    int num_subcarriers;
    int base_freq;
    int freq_spacing;
    int symbol_duration_ms;
} ofdm_params_t;

// Subcarrier thread data
typedef struct {
    int core_id;
    int subcarrier_id;
    int frequency;
    pthread_t thread;
    pthread_barrier_t* start_barrier;
    pthread_barrier_t* sync_barrier;
    volatile int* symbol_data;
} subcarrier_thread_t;

// Function prototypes
void* subcarrier_worker(void* arg);
void generate_subcarrier_tone(int freq, int duration_ms, int core_id);
void transmit_ofdm_symbol(uint8_t symbol, ofdm_params_t* params, subcarrier_thread_t* carriers);
void transmit_ofdm_frame(uint8_t* data, int data_len, ofdm_params_t* params);
void setup_ofdm_carriers(subcarrier_thread_t* carriers, ofdm_params_t* params, 
                        pthread_barrier_t* start_barrier, pthread_barrier_t* sync_barrier);
void cleanup_carriers(subcarrier_thread_t* carriers, int num_carriers);
void signal_handler(int sig);
void print_ofdm_spectrum(ofdm_params_t* params);

/**
 * Subcarrier worker thread
 * Each thread handles one frequency/subcarrier on a dedicated CPU core
 */
void* subcarrier_worker(void* arg) {
    subcarrier_thread_t* carrier = (subcarrier_thread_t*)arg;
    
    // Set CPU affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(carrier->core_id, &cpuset);
    
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        fprintf(stderr, "Error setting affinity for subcarrier %d on core %d\n", 
                carrier->subcarrier_id, carrier->core_id);
        return NULL;
    }
    
    printf("Subcarrier %d initialized on core %d at %d Hz\n", 
           carrier->subcarrier_id, carrier->core_id, carrier->frequency);
    
    // Wait for all subcarriers to initialize
    pthread_barrier_wait(carrier->start_barrier);
    
    while (running) {
        // Wait for synchronization
        pthread_barrier_wait(carrier->sync_barrier);
        
        // Check if this subcarrier should transmit
        if (transmitting[carrier->subcarrier_id]) {
            // Generate load pattern at subcarrier frequency
            long cycle_nano = NANO_PER_SECOND / carrier->frequency;
            long half_cycle = cycle_nano / 2;
            
            struct timespec start, current;
            clock_gettime(CLOCK_MONOTONIC, &start);
            
            while (transmitting[carrier->subcarrier_id] && running) {
                clock_gettime(CLOCK_MONOTONIC, &current);
                long elapsed = (current.tv_sec - start.tv_sec) * NANO_PER_SECOND + 
                              (current.tv_nsec - start.tv_nsec);
                
                // Determine phase within cycle
                long phase = elapsed % cycle_nano;
                
                if (phase < half_cycle) {
                    // High phase - generate CPU load
                    while (phase < half_cycle && transmitting[carrier->subcarrier_id]) {
                        __asm__ __volatile__("" ::: "memory");
                        clock_gettime(CLOCK_MONOTONIC, &current);
                        elapsed = (current.tv_sec - start.tv_sec) * NANO_PER_SECOND + 
                                 (current.tv_nsec - start.tv_nsec);
                        phase = elapsed % cycle_nano;
                    }
                } else {
                    // Low phase - idle
                    struct timespec sleep_time = {0, cycle_nano - phase};
                    nanosleep(&sleep_time, NULL);
                }
            }
        } else {
            // Idle when not transmitting
            usleep(1000);
        }
    }
    
    return NULL;
}

/**
 * Setup OFDM subcarrier threads
 */
void setup_ofdm_carriers(subcarrier_thread_t* carriers, ofdm_params_t* params,
                        pthread_barrier_t* start_barrier, pthread_barrier_t* sync_barrier) {
    
    pthread_barrier_init(start_barrier, NULL, params->num_subcarriers + 1);
    pthread_barrier_init(sync_barrier, NULL, params->num_subcarriers + 1);
    
    for (int i = 0; i < params->num_subcarriers; i++) {
        carriers[i].core_id = i;
        carriers[i].subcarrier_id = i;
        carriers[i].frequency = params->base_freq + (i * params->freq_spacing);
        carriers[i].start_barrier = start_barrier;
        carriers[i].sync_barrier = sync_barrier;
        
        pthread_create(&carriers[i].thread, NULL, subcarrier_worker, &carriers[i]);
    }
    
    // Wait for all subcarriers to initialize
    pthread_barrier_wait(start_barrier);
    printf("All %d subcarriers initialized\n\n", params->num_subcarriers);
}

/**
 * Transmit OFDM symbol
 * Encodes data bits across multiple subcarriers transmitting in parallel
 */
void transmit_ofdm_symbol(uint8_t symbol, ofdm_params_t* params, subcarrier_thread_t* carriers) {
    printf("Transmitting symbol: 0x%02X [", symbol);
    
    // Determine which subcarriers to activate based on symbol bits
    for (int i = 0; i < params->num_subcarriers && i < 8; i++) {
        transmitting[i] = (symbol >> i) & 1;
        printf("%d", transmitting[i]);
    }
    printf("]\n");
    
    // Synchronize all subcarriers to start transmission
    pthread_barrier_wait(carriers[0].sync_barrier);
    
    // Transmit for symbol duration
    usleep(params->symbol_duration_ms * 1000);
    
    // Stop all subcarriers
    for (int i = 0; i < params->num_subcarriers; i++) {
        transmitting[i] = 0;
    }
    
    // Guard interval between symbols
    usleep(GUARD_INTERVAL_MS * 1000);
}

/**
 * Transmit complete OFDM frame
 */
void transmit_ofdm_frame(uint8_t* data, int data_len, ofdm_params_t* params) {
    pthread_barrier_t start_barrier, sync_barrier;
    subcarrier_thread_t carriers[MAX_SUBCARRIERS];
    
    printf("=== OFDM Transmission Parameters ===\n");
    printf("Subcarriers: %d\n", params->num_subcarriers);
    printf("Base frequency: %d Hz\n", params->base_freq);
    printf("Frequency spacing: %d Hz\n", params->freq_spacing);
    printf("Symbol duration: %d ms\n", params->symbol_duration_ms);
    printf("Data length: %d bytes\n\n", data_len);
    
    print_ofdm_spectrum(params);
    
    // Setup subcarrier threads
    setup_ofdm_carriers(carriers, params, &start_barrier, &sync_barrier);
    
    // Transmit preamble (alternating pattern for synchronization)
    printf("Transmitting preamble...\n");
    for (int i = 0; i < 4; i++) {
        transmit_ofdm_symbol(0xAA, params, carriers);  // 10101010
        transmit_ofdm_symbol(0x55, params, carriers);  // 01010101
    }
    
    // Transmit data
    printf("\nTransmitting data payload...\n");
    for (int i = 0; i < data_len; i++) {
        printf("Byte %d/%d: ", i + 1, data_len);
        transmit_ofdm_symbol(data[i], params, carriers);
    }
    
    // Transmit end-of-frame marker
    printf("\nTransmitting EOF marker...\n");
    transmit_ofdm_symbol(0xFF, params, carriers);
    transmit_ofdm_symbol(0x00, params, carriers);
    transmit_ofdm_symbol(0xFF, params, carriers);
    
    // Cleanup
    running = 0;
    cleanup_carriers(carriers, params->num_subcarriers);
    pthread_barrier_destroy(&start_barrier);
    pthread_barrier_destroy(&sync_barrier);
    
    printf("\nOFDM transmission complete!\n");
}

/**
 * Display OFDM spectrum allocation
 */
void print_ofdm_spectrum(ofdm_params_t* params) {
    printf("=== OFDM Spectrum Allocation ===\n");
    for (int i = 0; i < params->num_subcarriers; i++) {
        int freq = params->base_freq + (i * params->freq_spacing);
        printf("Subcarrier %d: %d Hz (Core %d)\n", i, freq, i);
    }
    printf("Total bandwidth: %d Hz\n\n", 
           params->freq_spacing * (params->num_subcarriers - 1));
}

/**
 * Cleanup carrier threads
 */
void cleanup_carriers(subcarrier_thread_t* carriers, int num_carriers) {
    for (int i = 0; i < num_carriers; i++) {
        pthread_cancel(carriers[i].thread);
        pthread_join(carriers[i].thread, NULL);
    }
}

/**
 * Signal handler for clean shutdown
 */
void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = 0;
}

/**
 * Print usage information
 */
void print_usage(char* program_name) {
    printf("OFDM Transmitter for POWER-SUPPLaY\n");
    printf("===================================\n");
    printf("Parallel data transmission using multiple CPU cores\n");
    printf("FOR EDUCATIONAL AND RESEARCH PURPOSES ONLY\n\n");
    printf("Usage: %s <base_freq> <freq_spacing> <num_subcarriers> \"message\"\n", program_name);
    printf("\nExample:\n");
    printf("  %s 8000 200 4 \"TEST\"  # 4 subcarriers at 8000,8200,8400,8600 Hz\n", program_name);
    printf("\nNote: num_subcarriers should not exceed available CPU cores\n");
    printf("Recommended: Use ultrasonic frequencies (>18kHz) for covert operation\n");
}

/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (argc != 5) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Parse parameters
    ofdm_params_t params;
    params.base_freq = atoi(argv[1]);
    params.freq_spacing = atoi(argv[2]);
    params.num_subcarriers = atoi(argv[3]);
    params.symbol_duration_ms = SYMBOL_DURATION_MS;
    char* message = argv[4];
    
    // Validate parameters
    if (params.num_subcarriers < 1 || params.num_subcarriers > MAX_SUBCARRIERS) {
        fprintf(stderr, "Error: num_subcarriers must be between 1 and %d\n", MAX_SUBCARRIERS);
        return 1;
    }
    
    if (params.base_freq < 20 || params.base_freq > 24000) {
        fprintf(stderr, "Error: base_freq must be between 20 and 24000 Hz\n");
        return 1;
    }
    
    // Check if we have enough CPU cores
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (params.num_subcarriers > num_cores) {
        fprintf(stderr, "Warning: Requesting %d subcarriers but only %d CPU cores available\n",
                params.num_subcarriers, num_cores);
        fprintf(stderr, "Performance may be degraded. Consider using fewer subcarriers.\n\n");
    }
    
    printf("Starting OFDM transmission of message: \"%s\"\n", message);
    printf("Using %d CPU cores for parallel transmission\n\n", params.num_subcarriers);
    
    // Transmit the message
    transmit_ofdm_frame((uint8_t*)message, strlen(message), &params);
    
    return 0;
}