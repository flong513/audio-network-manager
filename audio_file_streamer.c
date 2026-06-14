// Everytime a new track is loaded, the entire file is read into memory
// Then the audio data is streamed to the audio hardware device

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <termios.h>
#include <sys/select.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

#define BUFFER_SIZE 1024

/* Playlist Configuration Matrix */
const char *playlist[] = {
    "assets/test_song.wav",
    "assets/test_song2.wav",   // Make sure to drop another WAV file here later!
    "assets/test_song3.wav"
};
#define TOTAL_TRACKS (sizeof(playlist) / sizeof(playlist[0]))

/* Global System States */
int current_track_index = 0;
float volume_scale = 0.5f; 
int track_changed_flag = 0; // Signals the loop to tear down and reload assets

typedef struct {
    char     chunk_id[4];
    uint32_t chunk_size;
    char     format[4];
    char     subchunk1_id[4];
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     subchunk2_id[4];
    uint32_t subchunk2_size;
} WavHeader;

/* Linux Terminal Non-Blocking Controls */
struct termios orig_termios;
void reset_terminal_mode() { tcsetattr(0, TCSANOW, &orig_termios); }
void set_conio_mode() {
    struct termios new_termios;
    tcgetattr(0, &orig_termios);
    atexit(reset_terminal_mode);
    new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &new_termios);
}
int kbhit() {
    struct timeval tv = {0L, 0L};
    fd_set fds; FD_ZERO(&fds); FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

/* ALSA Hardware Lifecycle Provisions */
snd_pcm_t* init_audio_device(uint32_t sample_rate, uint16_t channels) {
    int rc; snd_pcm_t *handle; snd_pcm_hw_params_t *params;
    unsigned int val = sample_rate; int dir = 0;

    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) return NULL;

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, channels);
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

    snd_pcm_uframes_t buffer_frames = BUFFER_SIZE * 4;
    snd_pcm_uframes_t period_frames = BUFFER_SIZE;
    snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_frames);
    snd_pcm_hw_params_set_period_size_near(handle, params, &period_frames, &dir);

    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) { snd_pcm_close(handle); return NULL; }
    return handle;
}

int main() {
    FILE *wav_file = NULL;
    WavHeader header;
    snd_pcm_t *audio_handle = NULL;
    int16_t audio_buffer[BUFFER_SIZE * 2];

    printf("=== Automotive Playlist Media Manager Engine ===\n");
    printf("Controls: [n] Next Track, [p] Prev Track, [+] Vol Up, [-] Vol Down, [q] Quit\n\n");

    set_conio_mode();

    // Force initial load of track index 0
    track_changed_flag = 1;

    while (1) {
        /* Step 1: Handle Track Lifecycle Reload Transitions */
        if (track_changed_flag) {
            track_changed_flag = 0;

            // Clear previous track infrastructure if currently allocated
            if (audio_handle) { snd_pcm_drain(audio_handle); snd_pcm_close(audio_handle); audio_handle = NULL; }
            if (wav_file) { fclose(wav_file); wav_file = NULL; }

            printf("\r[SYSTEM] Loading track %d/%ld: %s...\n", current_track_index + 1, TOTAL_TRACKS, playlist[current_track_index]);

            wav_file = fopen(playlist[current_track_index], "rb");
            if (!wav_file) {
                printf("[ERROR] Failed to open asset path file. Skipping to next...\n");
                track_changed_flag = 1;
                current_track_index = (current_track_index + 1) % TOTAL_TRACKS;
                continue;
            }

            if (fread(&header, sizeof(WavHeader), 1, wav_file) != 1) {
                track_changed_flag = 1; continue;
            }

            audio_handle = init_audio_device(header.sample_rate, header.num_channels);
            if (!audio_handle) {
                printf("[ERROR] Sound hardware reject configuration specs.\n");
                return 1;
            }
        }

        /* Step 2: Handle Non-Blocking Keyboard Handoffs */
        if (kbhit()) {
            char ch = getchar();
            if (ch == 'n' || ch == 'N') {
                current_track_index = (current_track_index + 1) % TOTAL_TRACKS; // Cycle forward wrap
                track_changed_flag = 1;
            } else if (ch == 'p' || ch == 'P') {
                current_track_index = (current_track_index - 1 + TOTAL_TRACKS) % TOTAL_TRACKS; // Cycle backward wrap
                track_changed_flag = 1;
            } else if (ch == '+' || ch == '=') {
                volume_scale += 0.05f; if (volume_scale > 1.0f) volume_scale = 1.0f;
                printf("\r[MIXER] Volume: %.0f%%   ", volume_scale * 100.0f); fflush(stdout);
            } else if (ch == '-') {
                volume_scale -= 0.05f; if (volume_scale < 0.0f) volume_scale = 0.0f;
                printf("\r[MIXER] Volume: %.0f%%   ", volume_scale * 100.0f); fflush(stdout);
            } else if (ch == 'q' || ch == 'Q') {
                break;
            }
        }

        /* Step 3: Stream Audio Blocks */
        size_t samples_to_read = BUFFER_SIZE * header.num_channels;
        size_t samples_read = fread(audio_buffer, sizeof(int16_t), samples_to_read, wav_file);

        // Auto-advance playlist when song ends naturally
        if (samples_read == 0) {
            printf("\n[MEDIA] Song complete. Auto-advancing playlist pipeline...\n");
            current_track_index = (current_track_index + 1) % TOTAL_TRACKS;
            track_changed_flag = 1;
            continue;
        }

        // DSP Volume Scalar pass
        for (size_t i = 0; i < samples_read; i++) {
            audio_buffer[i] = (int16_t)(audio_buffer[i] * volume_scale);
        }

        int frames_to_write = samples_read / header.num_channels;
        snd_pcm_sframes_t rc = snd_pcm_writei(audio_handle, audio_buffer, frames_to_write);

        if (rc == -EPIPE) {
            snd_pcm_prepare(audio_handle);
        } else if (rc < 0) {
            snd_pcm_prepare(audio_handle);
        }
    }

    if (audio_handle) { snd_pcm_drain(audio_handle); snd_pcm_close(audio_handle); }
    if (wav_file) { fclose(wav_file); }
    return 0;
}