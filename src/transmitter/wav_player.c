/**
 * WAV Audio Player via Power Supply
 * 
 * Plays WAV audio files through computer power supply using
 * Amplitude Modulation (AM) or Pulse Width Modulation (PWM)
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
#define MAX_CORES 32
#define MAX_SAMPLE_RATE 48000
#define DEFAULT_CORES 4

// WAV file header structure
typedef struct {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // File size - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmt_size;      // Format chunk size
    uint16_t format;        // Audio format (1 = PCM)
    uint16_t channels;      // Number of channels
    uint32_t sample_rate;   // Sample rate
    uint32_t byte_rate;     // Byte rate
    uint16_t block_align;   // Block align
    uint16_t bits_per_sample; // Bits per sample
    char data[4];           // "data"
    uint32_t data_size;     // Data chunk size
} wav_header_t;

// Modulation types
typedef enum {
    MOD_AM,   // Amplitude Modulation
    MOD_PWM   // Pulse Width Modulation
} modulation_type_t;

// Global variables
volatile int running = 1;
volatile int active_cores = 0;
pthread_mutex_t core_mutex = PTHREAD_MUTEX_INITIALIZER;

// Worker thread data
typedef struct {
    int core_id;
    pthread_t thread;
    volatile int active;
    volatile double duty_cycle;
    volatile int frequency;
} worker_t;

// Function prototypes
int load_wav_file(const char* filename, wav_header_t* header, int16_t** samples);
void* worker_routine_am(void* arg);
void* worker_routine_pwm(void* arg);
void play_wav_am(int16_t* samples, int num_samples, int sample_rate, int num_cores);
void play_wav_pwm(int16_t* samples, int num_samples, int sample_rate, int num_cores);
void normalize_samples(int16_t* samples, int num_samples);
double sample_to_duty_cycle(int16_t sample);
int sample_to_cores(int16_t sample, int max_cores);
void signal_handler(int sig);
void print_wav_info(wav_header_t* header);

/**
 * Load WAV file into memory
 */
int load_wav_file(const char* filename, wav_header_t* header, int16_t** samples) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return -1;
    }
    
    // Read WAV header
    if (fread(header, sizeof(wav_header_t), 1, file) != 1) {
        fprintf(stderr, "Error: Cannot read WAV header\n");
        fclose(file);
        return -1;
    }
    
    // Validate WAV format
    if (strncmp(header->riff, "RIFF", 4) != 0 || 
        strncmp(header->wave, "WAVE", 4) != 0) {
        fprintf(stderr, "Error: Invalid WAV file format\n");
        fclose(file);
        return -1;
    }
    
    if (header->format != 1) {
        fprintf(stderr, "Error: Only PCM format is supported\n");
        fclose(file);
        return -1;
    }
    
    if (header->bits_per_sample != 16) {
        fprintf(stderr, "Error: Only 16-bit samples are supported\n");
        fclose(file);
        return -1;
    }
    
    // Allocate memory for samples
    int num_samples = header->data_size / sizeof(int16_t);
    *samples = (int16_t*)malloc(header->data_size);
    if (!*samples) {
        fprintf(stderr, "Error: Cannot allocate memory for samples\n");
        fclose(file);
        return -1;
    }
    
    // Read samples
    if (fread(*samples, header->data_size, 1, file) != 1) {
        fprintf(stderr, "Error: Cannot read WAV data\n");
        free(*samples);
        fclose(file);
        return -1;
    }
    
    fclose(file);
    return num_samples;
}

/**
 * Print WAV file information
 */
void print_wav_info(wav_header_t* header) {
    printf("=== WAV File Information ===\n");
    printf("Format: PCM\n");
    printf("Channels: %d\n", header->channels);
    printf("Sample Rate: %d Hz\n", header->sample_rate);
    printf("Bits per Sample: %d\n", header->bits_per_sample);
    printf("Data Size: %d bytes\n", header->data_size);
    printf("Duration: %.2f seconds\n", 
           (float)header->data_size / header->byte_rate);
    printf("============================\n\n");
}

/**
 * Worker routine for Amplitude Modulation
 */
void* worker_routine_am(void* arg) {
    worker_t* worker = (worker_t*)arg;
    
    // Set CPU affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(worker->core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    
    while (running) {
        if (worker->active && worker->frequency > 0) {
            // Generate tone at specified frequency
            long cycle_nano = NANO_PER_SECOND / worker->frequency;
            long half_cycle = cycle_nano / 2;
            
            struct timespec start, current;
            clock_gettime(CLOCK_MONOTONIC, &start);
            
            while (worker->active && running) {
                clock_gettime(CLOCK_MONOTONIC, &current);
                long elapsed = (current.tv_sec - start.tv_sec) * NANO_PER_SECOND + 
                              (current.tv_nsec - start.tv_nsec);
                
                if (elapsed >= cycle_nano) break;
                
                long phase = elapsed % cycle_nano;
                
                if (phase < half_cycle) {
                    // High phase - generate load
                    while (phase < half_cycle && worker->active) {
                        __asm__ __volatile__("" ::: "memory");
                        clock_gettime(CLOCK_MONOTONIC, &current);
                        elapsed = (current.tv_sec - start.tv_sec) * NANO_PER_SECOND + 
                                 (current.tv_nsec - start.tv_nsec);
                        phase = elapsed % cycle_nano;
                    }
                } else {
                    // Low phase - idle
                    usleep(1);
                }
            }
        } else {
            usleep(100);
        }
    }
    return NULL;
}

/**
 * Worker routine for Pulse Width Modulation
 */
void* worker_routine_pwm(void* arg) {
    worker_t* worker = (worker_t*)arg;
    
    // Set CPU affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(worker->core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    
    while (running) {
        if (worker->active && worker->frequency > 0) {
            long cycle_nano = NANO_PER_SECOND / worker->frequency;
            long active_nano = cycle_nano * worker->duty_cycle;
            
            // Active phase - generate load
            struct timespec start;
            clock_gettime(CLOCK_MONOTONIC, &start);
            
            while (worker->active && running) {
                struct timespec current;
                clock_gettime(CLOCK_MONOTONIC, &current);
                long elapsed = (current.tv_sec - start.tv_sec) * NANO_PER_SECOND + 
                              (current.tv_nsec - start.tv_nsec);
                
                if (elapsed >= active_nano) break;
                __asm__ __volatile__("" ::: "memory");
            }
            
            // Idle phase
            if (worker->duty_cycle < 1.0) {
                long idle_nano = cycle_nano - active_nano;
                struct timespec idle_time = {0, idle_nano};
                nanosleep(&idle_time, NULL);
            }
        } else {
            usleep(100);
        }
    }
    return NULL;
}

/**
 * Convert sample amplitude to duty cycle (0.0 - 1.0)
 */
double sample_to_duty_cycle(int16_t sample) {
    // Normalize to 0.0 - 1.0
    double normalized = (sample + 32768.0) / 65536.0;
    // Scale to useful range (0.2 - 0.8) to avoid extremes
    return 0.2 + (normalized * 0.6);
}

/**
 * Convert sample amplitude to number of active cores
 */
int sample_to_cores(int16_t sample, int max_cores) {
    // Map sample value to number of cores
    int abs_sample = abs(sample);
    int cores = (abs_sample * max_cores) / 32768;
    if (cores == 0) cores = 1;
    if (cores > max_cores) cores = max_cores;
    return cores;
}

/**
 * Play WAV using Amplitude Modulation
 */
void play_wav_am(int16_t* samples, int num_samples, int sample_rate, int num_cores) {
    printf("Playing WAV using Amplitude Modulation (%d cores)...\n", num_cores);
    
    // Create worker threads
    worker_t workers[MAX_CORES];
    for (int i = 0; i < num_cores; i++) {
        workers[i].core_id = i;
        workers[i].active = 0;
        workers[i].frequency = 8000; // Base frequency
        pthread_create(&workers[i].thread, NULL, worker_routine_am, &workers[i]);
    }
    
    // Calculate sample period
    long sample_period_ns = NANO_PER_SECOND / sample_rate;
    struct timespec sample_time = {0, sample_period_ns};
    
    // Play samples
    for (int i = 0; i < num_samples && running; i++) {
        // Determine how many cores to activate based on amplitude
        int active = sample_to_cores(samples[i], num_cores);
        
        // Activate/deactivate cores
        pthread_mutex_lock(&core_mutex);
        for (int j = 0; j < num_cores; j++) {
            workers[j].active = (j < active) ? 1 : 0;
        }
        pthread_mutex_unlock(&core_mutex);
        
        // Progress indicator
        if (i % (sample_rate / 10) == 0) {
            printf("\rProgress: %d%%", (i * 100) / num_samples);
            fflush(stdout);
        }
        
        // Wait for next sample
        nanosleep(&sample_time, NULL);
    }
    
    printf("\rProgress: 100%%\n");
    
    // Cleanup
    running = 0;
    for (int i = 0; i < num_cores; i++) {
        pthread_cancel(workers[i].thread);
        pthread_join(workers[i].thread, NULL);
    }
}

/**
 * Play WAV using Pulse Width Modulation
 */
void play_wav_pwm(int16_t* samples, int num_samples, int sample_rate, int num_cores) {
    printf("Playing WAV using Pulse Width Modulation (%d cores)...\n", num_cores);
    
    // Create worker threads
    worker_t workers[MAX_CORES];
    for (int i = 0; i < num_cores; i++) {
        workers[i].core_id = i;
        workers[i].active = 1;
        workers[i].frequency = sample_rate;
        workers[i].duty_cycle = 0.5;
        pthread_create(&workers[i].thread, NULL, worker_routine_pwm, &workers[i]);
    }
    
    // Calculate sample period
    long sample_period_ns = NANO_PER_SECOND / sample_rate;
    struct timespec sample_time = {0, sample_period_ns};
    
    // Play samples
    for (int i = 0; i < num_samples && running; i++) {
        // Convert sample to duty cycle
        double duty = sample_to_duty_cycle(samples[i]);
        
        // Update all workers with new duty cycle
        pthread_mutex_lock(&core_mutex);
        for (int j = 0; j < num_cores; j++) {
            workers[j].duty_cycle = duty;
        }
        pthread_mutex_unlock(&core_mutex);
        
        // Progress indicator
        if (i % (sample_rate / 10) == 0) {
            printf("\rProgress: %d%%", (i * 100) / num_samples);
            fflush(stdout);
        }
        
        // Wait for next sample
        nanosleep(&sample_time, NULL);
    }
    
    printf("\rProgress: 100%%\n");
    
    // Cleanup
    running = 0;
    for (int i = 0; i < num_cores; i++) {
        pthread_cancel(workers[i].thread);
        pthread_join(workers[i].thread, NULL);
    }
}

/**
 * Signal handler
 */
void signal_handler(int sig) {
    printf("\nReceived signal %d, stopping playback...\n", sig);
    running = 0;
}

/**
 * Print usage information
 */
void print_usage(char* program_name) {
    printf("WAV Player for POWER-SUPPLaY\n");
    printf("=============================\n");
    printf("Play WAV audio files through computer power supply\n");
    printf("FOR EDUCATIONAL AND RESEARCH PURPOSES ONLY\n\n");
    printf("Usage: %s <wav_file> [num_cores] [am|pwm]\n", program_name);
    printf("\nParameters:\n");
    printf("  wav_file   - Path to WAV file (16-bit PCM)\n");
    printf("  num_cores  - Number of CPU cores to use (default: 4)\n");
    printf("  modulation - AM (amplitude) or PWM (pulse width) (default: PWM)\n");
    printf("\nExamples:\n");
    printf("  %s audio.wav              # Play with defaults\n", program_name);
    printf("  %s audio.wav 8 am         # Use 8 cores with AM\n", program_name);
    printf("  %s audio.wav 4 pwm        # Use 4 cores with PWM\n", program_name);
}

/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Parse arguments
    char* filename = argv[1];
    int num_cores = DEFAULT_CORES;
    modulation_type_t modulation = MOD_PWM;
    
    if (argc >= 3) {
        num_cores = atoi(argv[2]);
        if (num_cores < 1 || num_cores > MAX_CORES) {
            fprintf(stderr, "Error: num_cores must be between 1 and %d\n", MAX_CORES);
            return 1;
        }
    }
    
    if (argc >= 4) {
        if (strcasecmp(argv[3], "am") == 0) {
            modulation = MOD_AM;
        } else if (strcasecmp(argv[3], "pwm") == 0) {
            modulation = MOD_PWM;
        } else {
            fprintf(stderr, "Error: Modulation must be 'am' or 'pwm'\n");
            return 1;
        }
    }
    
    // Load WAV file
    wav_header_t header;
    int16_t* samples = NULL;
    int num_samples = load_wav_file(filename, &header, &samples);
    
    if (num_samples < 0) {
        return 1;
    }
    
    // Print file information
    printf("Loading: %s\n", filename);
    print_wav_info(&header);
    
    // Check sample rate
    if (header.sample_rate > MAX_SAMPLE_RATE) {
        fprintf(stderr, "Warning: Sample rate %d Hz exceeds maximum %d Hz\n",
                header.sample_rate, MAX_SAMPLE_RATE);
        fprintf(stderr, "Audio may be distorted\n\n");
    }
    
    // Convert stereo to mono if necessary
    if (header.channels == 2) {
        printf("Converting stereo to mono...\n");
        for (int i = 0; i < num_samples / 2; i++) {
            samples[i] = (samples[i * 2] + samples[i * 2 + 1]) / 2;
        }
        num_samples /= 2;
    }
    
    // Play the audio
    printf("\n*** WARNING: This will generate acoustic signals ***\n");
    printf("*** The sound may be audible or ultrasonic      ***\n");
    printf("*** Press Ctrl+C to stop playback                ***\n\n");
    
    sleep(2); // Give user time to read warning
    
    if (modulation == MOD_AM) {
        play_wav_am(samples, num_samples, header.sample_rate, num_cores);
    } else {
        play_wav_pwm(samples, num_samples, header.sample_rate, num_cores);
    }
    
    // Cleanup
    free(samples);
    printf("Playback complete!\n");
    
    return 0;
}