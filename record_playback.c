// Using ALSA library to record and play audio

#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

#define BUFFER_SIZE 1024

int main() {
    snd_pcm_t *capture_handle;
    snd_pcm_t *playback_handle;
    int16_t buffer[BUFFER_SIZE * 2];

    // 1. OPENING DEVICES
    // TODO: Use snd_pcm_open to open "default" for 
    // SND_PCM_STREAM_CAPTURE and SND_PCM_STREAM_PLAYBACK
    if (snd_pcm_open(&capture_handle, "default", SND_PCM_STREAM_CAPTURE, 0) < 0) {
        fprintf(stderr, "Cannot open capture device\n");
        return 1;
    }
    if (snd_pcm_open(&playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf(stderr, "Cannot open playback device\n");
        return 1;
    }
    
    // 2. CONFIGURE HARDWARE
    // TODO: Set hardware params (Sample rate, Format, Channels) 
    // for BOTH handles. Use snd_pcm_hw_params_set_... functions.

    snd_pcm_hw_params_t *params_capture;
    snd_pcm_hw_params_t *params_playback;

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params_capture);
    snd_pcm_hw_params_alloca(&params_playback);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any(capture_handle, params_capture);
    snd_pcm_hw_params_any(playback_handle, params_playback);

    /* Set the parameters. */
    snd_pcm_hw_params_set_access(capture_handle, params_capture, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(capture_handle, params_capture, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(capture_handle, params_capture, 2);
    snd_pcm_hw_params_set_rate(capture_handle, params_capture, 44100, 0);
    snd_pcm_hw_params(capture_handle, params_capture);

    snd_pcm_hw_params_set_access(playback_handle, params_playback, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(playback_handle, params_playback, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(playback_handle, params_playback, 2);
    snd_pcm_hw_params_set_rate(playback_handle, params_playback, 44100, 0);
    snd_pcm_hw_params(playback_handle, params_playback);

    printf("Starting loopback... Press Ctrl+C to stop.\n");

    while (1) {
        // 3. CAPTURE
        // TODO: Call snd_pcm_readi to get samples from the capture_handle
        int captured = snd_pcm_readi(capture_handle, buffer, BUFFER_SIZE);
        // into the 'buffer' array. 
        // Check for error/underrun results (e.g., -EPIPE).
        if (captured == -EPIPE) { // overrun error
            snd_pcm_prepare(capture_handle);
            continue;
        } else if (captured < 0) {
            fprintf(stderr, "Error reading from capture device: %s\n", snd_strerror(captured));
            break;
        } 
        // 4. DSP (The "Code it up!" part)
        // TODO: Perform your processing here. 
        // Example: Amplify by multiplying each sample by a volume factor.
        // for(int i=0; i<BUFFER_SIZE; i++) { buffer[i] *= volume; }

        // 5. PLAYBACK
        // TODO: Call snd_pcm_writei to send the processed 'buffer'
        // to the playback_handle.
        int written = snd_pcm_writei(playback_handle, buffer, captured);
        if (written == -EPIPE) {
            snd_pcm_prepare(playback_handle);
            continue;
        } else if (written < 0) {
            fprintf(stderr, "Error writing to playback device: %s\n", snd_strerror(written));
            break;
        }
    }

    // 6. CLEANUP
    // TODO: Close both handles.
    snd_pcm_close(capture_handle);
    snd_pcm_close(playback_handle);
    
    return 0;
}


