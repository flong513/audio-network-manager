#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

//To avoid aliasing, we calculated 40k from the 20k human hearing limit from the Nyquist Sampling 
// 2 * 20k = 40k
#define SAMPLE_RATE 44100
#define CHANNELS    1
#define BUFFER_SIZE 1024  // Lower buffer size = lower latency, critical for automotive safety chimes

/* Global application states */
double current_frequency = 440.0; // Default tone frequency (A4 note)
int is_muted = 0;                 // Global mute status

/* Helper function to initialize the ALSA Sound Device */
snd_pcm_t* init_audio_device() {
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val = SAMPLE_RATE;
    int dir;
    
    // Open the default audio device for playback
    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (rc < 0) {
        fprintf(stderr, "Error opening PCM device: %s\n", snd_strerror(rc));
        return NULL;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, CHANNELS);
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

    // Set the hardware parameters
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr, "Error setting HW parameters: %s\n", snd_strerror(rc));
        return NULL;
    }
    return handle;
}

int main() {
    snd_pcm_t *audio_handle;
    short audio_buffer[BUFFER_SIZE];
    double angle = 0.0;

    printf("=== Audio Engine Synthesis Sandbox ===\n");
    printf("[ NET ] CAN interface bypassed for standalone desktop testing.\n");

    /* 1. Initialize Audio Hardware Device */
    audio_handle = init_audio_device();
    if (!audio_handle) return 1;
    printf("[AUDIO] ALSA audio device initialized at %dHz.\n", SAMPLE_RATE);
    printf("System active. Synthesizing signal...\n\n");

    /* 2. Real-Time Processing Loop */
    while (1) {
        /* Part A: Stream Audio to Hardware Controllers */
        double sample_increment = (2.0 * M_PI * current_frequency) / SAMPLE_RATE;
        
        for (int i = 0; i < BUFFER_SIZE; i++) {
            if (is_muted) {
                audio_buffer[i] = 0; // Absolute zero signal
            } else {
                audio_buffer[i] = (short)(sin(angle) * 15000.0); // Scaled amplitude
                angle += sample_increment;
                if (angle >= 2.0 * M_PI) angle -= 2.0 * M_PI;
            }
        }

        // Write the chunk to the DMA buffer via the ALSA Kernel Driver
        int rc = snd_pcm_writei(audio_handle, audio_buffer, BUFFER_SIZE);
        
        if (rc == -EPIPE) {
            // Recover transparently from an underrun buffer loop starvation
            snd_pcm_prepare(audio_handle);
        } else if (rc < 0 && rc != -EAGAIN) {
            // Handle any other critical hardware driver errors
            snd_pcm_prepare(audio_handle);
        }
        
        // Minor sleep to prevent pinning the CPU core to 100% while idling
        // usleep(1000); 
    }

    /* Clean Up System Context */
    snd_pcm_drain(audio_handle);
    snd_pcm_close(audio_handle);
    return 0;
}