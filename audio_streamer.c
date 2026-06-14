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

/* 1. Master Playlist Array Matrix */
const char *playlist[] = {
    "assets/test_song.wav",
    "assets/test_song2.wav",
    "assets/test_song3.wav"
};
#define TOTAL_TRACKS (sizeof(playlist) / sizeof(playlist[0]))

/* 2. Global System Mixer States */
int current_track_index = 0;
int track_changed_flag = 1;   // Force initial system configuration reload
float volume_scale = 0.5f;     // Master volume (0.0 to 1.0)
float balance_panning = 0.0f;  // Spatial balance (-1.0 Full Left, 0.0 Center, 1.0 Full Right)

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

/* Linux Terminal Non-Blocking Configuration Matrix */
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

/* Dynamic Hardware Provisioning Handler */
// Pulse code modulation = digital audio format that represents audio as a stream of bits
snd_pcm_t* init_audio_device(uint32_t sample_rate, uint16_t channels) {
    int rc; snd_pcm_t *handle; snd_pcm_hw_params_t *params;
    unsigned int val = sample_rate; int dir = 0;

    // open the pcm device for playback
    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) return NULL;

    // allocate hardware parameters struct
    snd_pcm_hw_params_alloca(&params);
    // initialize the parameters with default values
    snd_pcm_hw_params_any(handle, params);
    // configure the pcm device for interleaved access, eg stereo audio
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    // configure the pcm device for 16-bit little endian format
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    // configure the pcm device for the number of channels
    snd_pcm_hw_params_set_channels(handle, params, channels);
    // set the sample rate
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

    snd_pcm_uframes_t buffer_frames = BUFFER_SIZE * 4;
    snd_pcm_uframes_t period_frames = BUFFER_SIZE;

    // set the buffer size
    snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_frames);
    // set the period size
    snd_pcm_hw_params_set_period_size_near(handle, params, &period_frames, &dir);

    // set the hardware parameters
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) { snd_pcm_close(handle); return NULL; }
    return handle;
}

int main() {
    FILE *wav_file = NULL;
    WavHeader header;
    snd_pcm_t *audio_handle = NULL;
    int16_t audio_buffer[BUFFER_SIZE * 2];

    printf("=== Combined Infotainment Mixer Core Engine ===\n");
    printf("Controls:  [ n ] Next Track    [ p ] Prev Track\n");
    printf("           [ [ ] Pan Left      [ ] ] Pan Right     [ \\ ] Reset Center\n");
    printf("           [ + ] Vol Up        [ - ] Vol Down      [ q ] Quit System\n\n");

    set_conio_mode();

    while (1) {
        /* Part A: Handle Dynamic Track Transitions Lifecycle */
        if (track_changed_flag) {
            track_changed_flag = 0;

            // Safely deallocate current active hardware handles
            if (audio_handle) { snd_pcm_drain(audio_handle); snd_pcm_close(audio_handle); audio_handle = NULL; }
            if (wav_file) { fclose(wav_file); wav_file = NULL; }

            printf("\r[SYSTEM] Loading Index %d/%ld: %s...\n", current_track_index + 1, TOTAL_TRACKS, playlist[current_track_index]);

            wav_file = fopen(playlist[current_track_index], "rb");
            if (!wav_file) {
                printf("[WARN] Missing file asset. Skipping to adjacent index...\n");
                current_track_index = (current_track_index + 1) % TOTAL_TRACKS;
                track_changed_flag = 1;
                continue;
            }

            if (fread(&header, sizeof(WavHeader), 1, wav_file) != 1) {
                track_changed_flag = 1; continue;
            }

            audio_handle = init_audio_device(header.sample_rate, header.num_channels);
            if (!audio_handle) {
                printf("[FATAL] Driver layer allocation rejection.\n");
                return 1;
            }
        }

        /* Part B: Capture Non-Blocking Multi-Input Matrix */
        if (kbhit()) {
            char ch = getchar();
            if (ch == 'n' || ch == 'N') {
                current_track_index = (current_track_index + 1) % TOTAL_TRACKS;
                track_changed_flag = 1;
            } else if (ch == 'p' || ch == 'P') {
                current_track_index = (current_track_index - 1 + TOTAL_TRACKS) % TOTAL_TRACKS;
                track_changed_flag = 1;
            } else if (ch == '[') {
                balance_panning -= 0.1f; if (balance_panning < -1.0f) balance_panning = -1.0f;
                printf("\r[MIXER] Panning: %.1f (Left)   | Vol: %.0f%%   ", balance_panning, volume_scale * 100.0f); fflush(stdout);
            } else if (ch == ']') {
                balance_panning += 0.1f; if (balance_panning > 1.0f) balance_panning = 1.0f;
                printf("\r[MIXER] Panning: %.1f (Right)  | Vol: %.0f%%   ", balance_panning, volume_scale * 100.0f); fflush(stdout);
            } else if (ch == '\\') {
                balance_panning = 0.0f;
                printf("\r[MIXER] Panning: 0.0 (Center)  | Vol: %.0f%%   ", volume_scale * 100.0f); fflush(stdout);
            } else if (ch == '+' || ch == '=') {
                volume_scale += 0.05f; if (volume_scale > 1.0f) volume_scale = 1.0f;
                printf("\r[MIXER] Panning: %.1f           | Vol: %.0f%%   ", balance_panning, volume_scale * 100.0f); fflush(stdout);
            } else if (ch == '-') {
                volume_scale -= 0.05f; if (volume_scale < 0.0f) volume_scale = 0.0f;
                printf("\r[MIXER] Panning: %.1f           | Vol: %.0f%%   ", balance_panning, volume_scale * 100.0f); fflush(stdout);
            } else if (ch == 'q' || ch == 'Q') {
                break;
            }
        }

        /* Part C: Real-Time Dual-Channel Audio Streaming & DSP */
        size_t samples_to_read = BUFFER_SIZE * header.num_channels;

        size_t samples_read = fread(audio_buffer, sizeof(int16_t), samples_to_read, wav_file);

        // Track completion -> auto advance pipeline sequence
        if (samples_read == 0) {
            current_track_index = (current_track_index + 1) % TOTAL_TRACKS;
            track_changed_flag = 1;
            continue;
        }

        /* --- INTEGRATED MULTI-CHANNEL DSP MATRIX --- */
        float left_gain  = (balance_panning <= 0.0f) ? 1.0f : (1.0f - balance_panning);
        float right_gain = (balance_panning >= 0.0f) ? 1.0f : (1.0f + balance_panning);

        float final_left_scaler  = volume_scale * left_gain;
        float final_right_scaler = volume_scale * right_gain;

        if (header.num_channels == 2) {
            for (size_t i = 0; i < samples_read; i += 2) {
                audio_buffer[i]     = (int16_t)(audio_buffer[i]     * final_left_scaler);  // Left Channel
                audio_buffer[i + 1] = (int16_t)(audio_buffer[i + 1] * final_right_scaler); // Right Channel
            }
        } else {
            // Fallback attenuation loop context for mono structures
            for (size_t i = 0; i < samples_read; i++) {
                audio_buffer[i] = (int16_t)(audio_buffer[i] * volume_scale);
            }
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