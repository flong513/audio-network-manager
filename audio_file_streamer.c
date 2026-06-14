#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

#define BUFFER_SIZE 1024  // Number of frames processed per iteration block


/* Standard Canonical RIFF WAV Header Structure Specification */
typedef struct {
    char     chunk_id[4];        // Must contain "RIFF" ASCII markers
    uint32_t chunk_size;
    char     format[4];          // Must contain "WAVE" ASCII markers
    char     subchunk1_id[4];    // Contains "fmt " sub-chunk descriptor
    uint32_t subchunk1_size;
    uint16_t audio_format;       // 1 = PCM (Linear uncompressed quantification)
    uint16_t num_channels;       // 1 = Mono, 2 = Stereo layout
    uint32_t sample_rate;        // Internal sampling resolution clock (e.g. 44100)
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;    // E.g. 16-bit depth bounds
    char     subchunk2_id[4];    // Contains "data" string identifier
    uint32_t subchunk2_size;     // Total payload size of raw sound bytes
} WavHeader;

/* Helper function to initialize the ALSA Sound Card with file context attributes */
snd_pcm_t* init_audio_device(uint32_t sample_rate, uint16_t channels) {
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val = sample_rate;
    int dir = 0;

    // Open ALSA device in standard blocking mode to match execution speed to hardware
    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr, "Error opening PCM device: %s\n", snd_strerror(rc));
        return NULL;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE); // Signed 16-bit Little Endian
    snd_pcm_hw_params_set_channels(handle, params, channels);
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

    // Give the DMA ring buffer structural room to breathe
    snd_pcm_uframes_t buffer_frames = BUFFER_SIZE * 4;
    snd_pcm_uframes_t period_frames = BUFFER_SIZE;
    snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_frames);
    snd_pcm_hw_params_set_period_size_near(handle, params, &period_frames, &dir);

    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr, "Error setting HW parameters: %s\n", snd_strerror(rc));
        return NULL;
    }
    return handle;
}

int main() {
    FILE *wav_file;
    WavHeader header;
    snd_pcm_t *audio_handle;
    int16_t audio_buffer[BUFFER_SIZE * 2]; // Multiplied by 2 to safe-house multi-channel interleaving samples

    printf("=== Automotive Standalone Media Streaming Engine ===\n");

    /* 1. Open and Parse input asset header from storage */
    wav_file = fopen("assets/test_song.wav", "rb");
    if (!wav_file) {
        perror("Critical error opening test_song.wav file asset target");
        return 1;
    }

    // Capture metadata tracking metrics out of the file prefix header block
    if (fread(&header, sizeof(WavHeader), 1, wav_file) != 1) {
        fprintf(stderr, "Critical Error: Failed to read the WAV header format.\n");
        fclose(wav_file);
        return 1;
    }

    if (strncmp(header.chunk_id, "RIFF", 4) != 0 || strncmp(header.format, "WAVE", 4) != 0) {
        fprintf(stderr, "Invalid asset payload type. File must be an uncompressed RIFF/WAV asset.\n");
        fclose(wav_file);
        return 1;
    }

    printf("[MEDIA] Track parsed successfully!\n");
    printf("  └─> Clock Speed: %d Hz\n", header.sample_rate);
    printf("  └─> Channel Topology: %d (%s)\n", header.num_channels, (header.num_channels == 2) ? "Stereo" : "Mono");
    printf("  └─> Bit Depth Bounds: %d-bit signed integer format\n", header.bits_per_sample);

    /* 2. Configure the Audio Hardware Pipeline using parsed context metadata */
    audio_handle = init_audio_device(header.sample_rate, header.num_channels);
    if (!audio_handle) {
        fclose(wav_file);
        return 1;
    }
    printf("[AUDIO] Driver core initialized. Streaming data blocks to DMA ring buffer...\n\n");

    /* 3. Real-Time Streaming Evaluation Loop */
    while (1) {
        // Read an interleaved block of raw samples from disk storage
        // Each sample structure takes up 2 bytes (sizeof(int16_t))
        size_t samples_to_read = BUFFER_SIZE * header.num_channels;
        size_t samples_read = fread(audio_buffer, sizeof(int16_t), samples_to_read, wav_file);

        if (samples_read == 0) {
            printf("[MEDIA] End of track reached. Looping track back to zero boundary point.\n");
            // Seek back past the metadata header directly to raw entry sound data bytes
            fseek(wav_file, sizeof(WavHeader), SEEK_SET);
            continue;
        }

        // Calculate explicit frame depth bounds (Total individual samples divided out by channel topology layout)
        int frames_to_write = samples_read / header.num_channels;

        /* Ship the data block array straight to the kernel audio subsystem.
           Because we are in blocking mode, this function will automatically block (sleep) 
           this process loop for roughly ~23ms while the DMA controller streams hardware outputs. */
        snd_pcm_sframes_t rc = snd_pcm_writei(audio_handle, audio_buffer, frames_to_write);

        if (rc == -EPIPE) {
            // Smoothly repair accidental ring buffer underruns transparently
            snd_pcm_prepare(audio_handle);
        } else if (rc < 0) {
            fprintf(stderr, "ALSA system exception encountered: %s\n", snd_strerror(rc));
            snd_pcm_prepare(audio_handle);
        }
    }

    /* Clean Up System Context allocations */
    fclose(wav_file);
    snd_pcm_drain(audio_handle);
    snd_pcm_close(audio_handle);
    return 0;
}