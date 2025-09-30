/**
 * POWER-SUPPLaY: Acoustic Data Transmission via Power Supply
 * 
 * Based on the research by Mordechai Guri (2020)
 * "POWER-SUPPLaY: Leaking Data from Air-Gapped Systems by Turning 
 * the Power-Supplies Into Speakers"
 * 
 * FOR EDUCATIONAL AND RESEARCH PURPOSES ONLY
 * 
 * This implementation demonstrates how CPU workload manipulation can
 * generate acoustic signals through power supply components.
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
#include <errno.h>

#define NANO_PER_SECOND 1000000000L
#define MAX_CORES 32
#define DEFAULT_DURATION_MS 1000
#define MIN_FREQ 20      // Minimum audible frequency
#define MAX_FREQ 24000   // Maximum frequency (near-ultrasonic)

// Global synchronization variables
pthread_barrier_t half_cycle_barrier_hi;
pthread_barrier_t half_cycle_barrier_lo;
volatile int LO = 0;
volatile int running = 1;
volatile long cycle_nano = 0;
volatile long half_cycle_nano = 0;

// Thread data structure
typedef struct {
    int core_id;
    pthread_t thread;
} worker_thread_t;

// FSK modulation parameters
typedef struct {
    int freq_0;     // Frequency for bit 0
    int freq_1;     // Frequency for bit 1
    int bit_duration_ms;  // Duration per bit
} fsk_params_t;

// Function prototypes
void* worker_routine(void* arg);
void generate_tone(int freq, int duration_ms, int num_cores);
void transmit_bit_fsk(int bit, fsk_params_t* params, int num_cores);
void transmit_data_fsk(uint8_t* data, int data_len, fsk_params_t* params, int num_cores);
void transmit_preamble(fsk_params_t* params, int num_cores);
uint8_t calculate_crc8(uint8_t* data, int len);
void signal_handler(int sig);
int validate_frequency(int freq);
void print_usage(char* program_name);

/**
 * Worker thread routine - generates CPU load patterns
 * Based on Algorithm 2 from the paper
 */
void* worker_routine(void* arg) {
    worker_thread_t* thread_data = (worker_thread_t*)arg;
    
    // Set CPU affinity to bind thread to specific core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_data->core_id, &cpuset);
    
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        fprintf(stderr, "Error setting affinity for core %d\n", thread_data->core_id);
        return NULL;
    }
    
    while (running) {
        // Synchronize threads at end of LO half cycle
        pthread_barrier_wait(&half_cycle_barrier_lo);
        
        // HI half cycle - busy loop to generate load
        while (!LO && running) {
            // Busy waiting - generates CPU load
            __asm__ __volatile__("" ::: "memory");
        }
        
        // Synchronize threads at end of HI half cycle
        pthread_barrier_wait(&half_cycle_barrier_hi);
        
        // LO half cycle - idle state
        while (LO && running) {
            // Let CPU idle
            sched_yield();
        }
    }
    
    return NULL;
}

/**
 * Generate acoustic tone at specified frequency
 * Based on Algorithm 1 from the paper
 */
void generate_tone(int freq, int duration_ms, int num_cores) {
    if (!validate_frequency(freq)) {
        fprintf(stderr, "Invalid frequency: %d Hz (valid range: %d-%d Hz)\n", 
                freq, MIN_FREQ, MAX_FREQ);
        return;
    }
    
    printf("Generating %d Hz tone for %d ms using %d cores...\n", 
           freq, duration_ms, num_cores);
    
    // Initialize barriers for thread synchronization
    pthread_barrier_init(&half_cycle_barrier_hi, NULL, num_cores + 1);
    pthread_barrier_init(&half_cycle_barrier_lo, NULL, num_cores + 1);
    
    // Calculate cycle timings
    cycle_nano = NANO_PER_SECOND / freq;
    half_cycle_nano = cycle_nano / 2;
    
    // Create worker threads
    worker_thread_t* workers = malloc(num_cores * sizeof(worker_thread_t));
    for (int i = 0; i < num_cores; i++) {
        workers[i].core_id = i;
        pthread_create(&workers[i].thread, NULL, worker_routine, &workers[i]);
    }
    
    // Main control loop
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);
    long duration_nano = (long)duration_ms * 1000000L;
    
    while (running) {
        clock_gettime(CLOCK_MONOTONIC, &current);
        long elapsed = (current.tv_sec - start.tv_sec) * NANO_PER_SECOND + 
                      (current.tv_nsec - start.tv_nsec);
        
        if (elapsed >= duration_nano) {
            break;
        }
        
        // LO half cycle
        LO = 0;
        pthread_barrier_wait(&half_cycle_barrier_lo);
        
        // Wait for half cycle duration
        struct timespec half_cycle_sleep = {0, half_cycle_nano};
        nanosleep(&half_cycle_sleep, NULL);
        
        // HI half cycle
        LO = 1;
        pthread_barrier_wait(&half_cycle_barrier_hi);
        
        // Wait for half cycle duration
        nanosleep(&half_cycle_sleep, NULL);
    }
    
    // Clean up
    running = 0;
    for (int i = 0; i < num_cores; i++) {
        pthread_cancel(workers[i].thread);
    }
    free(workers);
    
    pthread_barrier_destroy(&half_cycle_barrier_hi);
    pthread_barrier_destroy(&half_cycle_barrier_lo);
}

/**
 * Transmit a single bit using FSK modulation
 */
void transmit_bit_fsk(int bit, fsk_params_t* params, int num_cores) {
    int freq = bit ? params->freq_1 : params->freq_0;
    generate_tone(freq, params->bit_duration_ms, num_cores);
}

/**
 * Transmit preamble sequence (10101010)
 */
void transmit_preamble(fsk_params_t* params, int num_cores) {
    printf("Transmitting preamble...\n");
    uint8_t preamble = 0xAA; // 10101010
    
    for (int i = 7; i >= 0; i--) {
        int bit = (preamble >> i) & 1;
        transmit_bit_fsk(bit, params, num_cores);
    }
}

/**
 * Calculate CRC-8 checksum
 */
uint8_t calculate_crc8(uint8_t* data, int len) {
    uint8_t crc = 0x00;
    uint8_t polynomial = 0x07;
    
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * Transmit data using FSK modulation with framing
 */
void transmit_data_fsk(uint8_t* data, int data_len, fsk_params_t* params, int num_cores) {
    // Transmit preamble
    transmit_preamble(params, num_cores);
    
    // Transmit payload
    printf("Transmitting %d bytes of data...\n", data_len);
    for (int byte_idx = 0; byte_idx < data_len; byte_idx++) {
        for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
            int bit = (data[byte_idx] >> bit_idx) & 1;
            transmit_bit_fsk(bit, params, num_cores);
        }
    }
    
    // Calculate and transmit CRC
    uint8_t crc = calculate_crc8(data, data_len);
    printf("Transmitting CRC: 0x%02X\n", crc);
    for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
        int bit = (crc >> bit_idx) & 1;
        transmit_bit_fsk(bit, params, num_cores);
    }
    
    printf("Transmission complete!\n");
}

/**
 * Validate frequency is within supported range
 */
int validate_frequency(int freq) {
    return (freq >= MIN_FREQ && freq <= MAX_FREQ);
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
    printf("POWER-SUPPLaY Acoustic Transmitter\n");
    printf("==================================\n");
    printf("Research implementation - FOR EDUCATIONAL PURPOSES ONLY\n\n");
    printf("Usage:\n");
    printf("  Tone generation:  %s tone <frequency_hz> <duration_ms> <num_cores>\n", program_name);
    printf("  FSK transmission: %s fsk <freq0_hz> <freq1_hz> <bit_duration_ms> <num_cores> \"message\"\n", program_name);
    printf("  WAV playback:     %s wav <wav_file> <num_cores> [am|pwm]\n", program_name);
    printf("\nExamples:\n");
    printf("  %s tone 440 5000 4          # Generate 440Hz tone for 5 seconds\n", program_name);
    printf("  %s fsk 8000 8500 50 4 \"HI\"  # Send \"HI\" using FSK\n", program_name);
    printf("\nFrequency range: %d - %d Hz\n", MIN_FREQ, MAX_FREQ);
    printf("Recommended frequencies for covert channel: 18000-22000 Hz (ultrasonic)\n");
}

/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
    // Install signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Check if running with appropriate permissions
    if (geteuid() == 0) {
        printf("Warning: Running as root. This is not required and not recommended.\n");
    }
    
    // Parse command mode
    if (strcmp(argv[1], "tone") == 0) {
        // Tone generation mode
        if (argc != 5) {
            printf("Error: Invalid arguments for tone mode\n");
            print_usage(argv[0]);
            return 1;
        }
        
        int freq = atoi(argv[2]);
        int duration = atoi(argv[3]);
        int cores = atoi(argv[4]);
        
        if (cores < 1 || cores > MAX_CORES) {
            fprintf(stderr, "Error: Number of cores must be between 1 and %d\n", MAX_CORES);
            return 1;
        }
        
        generate_tone(freq, duration, cores);
        
    } else if (strcmp(argv[1], "fsk") == 0) {
        // FSK transmission mode
        if (argc != 7) {
            printf("Error: Invalid arguments for FSK mode\n");
            print_usage(argv[0]);
            return 1;
        }
        
        fsk_params_t params;
        params.freq_0 = atoi(argv[2]);
        params.freq_1 = atoi(argv[3]);
        params.bit_duration_ms = atoi(argv[4]);
        int cores = atoi(argv[5]);
        char* message = argv[6];
        
        if (!validate_frequency(params.freq_0) || !validate_frequency(params.freq_1)) {
            fprintf(stderr, "Error: Frequencies must be between %d and %d Hz\n", 
                    MIN_FREQ, MAX_FREQ);
            return 1;
        }
        
        if (cores < 1 || cores > MAX_CORES) {
            fprintf(stderr, "Error: Number of cores must be between 1 and %d\n", MAX_CORES);
            return 1;
        }
        
        printf("FSK Transmission Parameters:\n");
        printf("  Frequency 0: %d Hz\n", params.freq_0);
        printf("  Frequency 1: %d Hz\n", params.freq_1);
        printf("  Bit duration: %d ms\n", params.bit_duration_ms);
        printf("  Message: \"%s\"\n", message);
        printf("  Using %d CPU cores\n\n", cores);
        
        transmit_data_fsk((uint8_t*)message, strlen(message), &params, cores);
        
    } else if (strcmp(argv[1], "wav") == 0) {
        // WAV playback mode - to be implemented in separate file
        printf("WAV playback mode not yet implemented in this version\n");
        printf("See wav_player.c for WAV playback functionality\n");
        return 1;
        
    } else {
        printf("Error: Unknown mode '%s'\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }
    
    return 0;
}
