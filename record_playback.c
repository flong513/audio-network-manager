// Using ALSA library to record and play audio

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <alsa/asoundlib.h>

#define SAMPLE_RATE 48000
#define CHANNELS 2
#define CAPTURE_PERIOD 256
#define CAPTURE_BUFFER 512
#define CHUNK_FRAMES 256

static double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static int recover_from_error(snd_pcm_t *handle, int err) {
    if (err == -EPIPE) {
        err = snd_pcm_prepare(handle);
        if (err < 0) {
            fprintf(stderr, "Cannot recover from underrun/overrun: %s\n", snd_strerror(err));
        }
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN) {
            sleep(1);
        }
        if (err < 0) {
            err = snd_pcm_prepare(handle);
        }
    }
    return err;
}

static int configure_capture(snd_pcm_t *handle) {
    snd_pcm_hw_params_t *hw_params;
    unsigned int rate = SAMPLE_RATE;
    int dir = 0;
    snd_pcm_uframes_t period = CAPTURE_PERIOD;
    snd_pcm_uframes_t buffer = CAPTURE_BUFFER;

    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(handle, hw_params);
    snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, hw_params, CHANNELS);
    snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, &dir);
    snd_pcm_hw_params_set_period_size_near(handle, hw_params, &period, &dir);
    snd_pcm_hw_params_set_buffer_size_near(handle, hw_params, &buffer);

    if (snd_pcm_hw_params(handle, hw_params) < 0) {
        fprintf(stderr, "Cannot set capture hardware params\n");
        return -1;
    }

    snd_pcm_hw_params_get_period_size(hw_params, &period, &dir);
    snd_pcm_hw_params_get_buffer_size(hw_params, &buffer);
    printf("Capture: rate=%u period=%lu buffer=%lu\n", rate, period, buffer);

    return snd_pcm_prepare(handle);
}

static int configure_playback(snd_pcm_t *handle) {
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
    unsigned int rate = SAMPLE_RATE;
    int dir = 0;
    snd_pcm_uframes_t period = 512;
    snd_pcm_uframes_t buffer = 2048;

    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(handle, hw_params);
    snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, hw_params, CHANNELS);
    snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, &dir);
    snd_pcm_hw_params_set_period_size_near(handle, hw_params, &period, &dir);
    snd_pcm_hw_params_set_buffer_size_near(handle, hw_params, &buffer);

    if (snd_pcm_hw_params(handle, hw_params) < 0) {
        fprintf(stderr, "Cannot set playback hardware params\n");
        return -1;
    }

    snd_pcm_hw_params_get_period_size(hw_params, &period, &dir);
    snd_pcm_hw_params_get_buffer_size(hw_params, &buffer);

    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(handle, sw_params);
    snd_pcm_sw_params_set_start_threshold(handle, sw_params, period);
    snd_pcm_sw_params(handle, sw_params);

    printf("Playback: rate=%u period=%lu buffer=%lu\n", rate, period, buffer);

    return snd_pcm_prepare(handle);
}

int main() {
    snd_pcm_t *capture_handle;
    snd_pcm_t *playback_handle;
    int16_t buffer[CHUNK_FRAMES * CHANNELS];

    const char *capture_device = "hw:0,0";
    const char *playback_device = "default";

    if (snd_pcm_open(&capture_handle, capture_device, SND_PCM_STREAM_CAPTURE, 0) < 0) {
        fprintf(stderr, "Cannot open capture device %s\n", capture_device);
        return 1;
    }
    if (snd_pcm_open(&playback_handle, playback_device, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf(stderr, "Cannot open playback device %s\n", playback_device);
        snd_pcm_close(capture_handle);
        return 1;
    }

    if (configure_capture(capture_handle) < 0 || configure_playback(playback_handle) < 0) {
        snd_pcm_close(capture_handle);
        snd_pcm_close(playback_handle);
        return 1;
    }

    printf("Starting loopback (%s -> %s). Press Ctrl+C to stop.\n",
           capture_device, playback_device);

    int frame_count = 0;
    double total_read_ms = 0, total_write_ms = 0, max_read_ms = 0, max_write_ms = 0;
    const int REPORT_EVERY = 200;

    while (1) {
        double t0 = now_ms();
        snd_pcm_sframes_t captured = snd_pcm_readi(capture_handle, buffer, CHUNK_FRAMES);
        double t1 = now_ms();

        if (captured < 0) {
            if (recover_from_error(capture_handle, (int)captured) < 0) {
                fprintf(stderr, "Error reading from capture device: %s\n",
                        snd_strerror((int)captured));
                break;
            }
            continue;
        }

        snd_pcm_sframes_t offset = 0;
        while (offset < captured) {
            snd_pcm_sframes_t written = snd_pcm_writei(
                playback_handle,
                buffer + offset * CHANNELS,
                (snd_pcm_uframes_t)(captured - offset));

            if (written < 0) {
                if (recover_from_error(playback_handle, (int)written) < 0) {
                    fprintf(stderr, "Error writing to playback device: %s\n",
                            snd_strerror((int)written));
                    goto cleanup;
                }
                continue;
            }
            offset += written;
        }

        double t2 = now_ms();
        double read_ms  = t1 - t0;
        double write_ms = t2 - t1;

        total_read_ms  += read_ms;
        total_write_ms += write_ms;
        if (read_ms  > max_read_ms)  max_read_ms  = read_ms;
        if (write_ms > max_write_ms) max_write_ms = write_ms;

        frame_count++;
        if (frame_count % REPORT_EVERY == 0) {
            double avg_read  = total_read_ms  / REPORT_EVERY;
            double avg_write = total_write_ms / REPORT_EVERY;
            printf("[frame %5d]  read: avg=%.2fms max=%.2fms  |  write: avg=%.2fms max=%.2fms  |  round-trip ~%.2fms\n",
                   frame_count, avg_read, max_read_ms, avg_write, max_write_ms,
                   avg_read + avg_write);
            total_read_ms = total_write_ms = 0;
            max_read_ms   = max_write_ms   = 0;
        }
    }

cleanup:
    snd_pcm_close(capture_handle);
    snd_pcm_close(playback_handle);

    return 0;
}
