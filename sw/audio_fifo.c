/**
 * audio_fifo.c - Implementation of shared FIFO functionality
 *
 * Implements the internal functions declared in audio_fifo.h that are
 * needed by both producer and consumer modules.
 */

#include "audio_fifo.h"
/* #include "pico/stdlib.h" */
#include <string.h>
#include <stdio.h>

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

// Current samples in fifo
uint32_t fifo_level(audio_fifo_t *fifo) {
    return fifo->samples_in_fifo;
}

// Initialize the FIFO (called by producer_init or directly by user)
void fifo_init(audio_fifo_t *fifo) {
    // Initialize FIFO structure
    memset(fifo, 0, sizeof(audio_fifo_t)); // Use the passed pointer
    fifo->state = FIFO_STATE_STOPPED;
    fifo->write_idx = 0;
    fifo->read_idx = 0;
    fifo->samples_in_fifo = 0;
}

// Reset the FIFO to empty state
void fifo_reset(audio_fifo_t *fifo) {
    fifo->state = FIFO_STATE_STOPPED;
    fifo->write_idx = fifo->read_idx;
    fifo->samples_in_fifo = 0;
}

// Add a sample to the FIFO (called by producer)
bool fifo_add_sample(audio_fifo_t *fifo, audio_sample_t sample) {
    if (fifo->samples_in_fifo == AUDIO_FIFO_SIZE) {
        return false;
    }
    fifo->buffer[fifo->write_idx] = sample;
    fifo->write_idx = (fifo->write_idx + 1) & (AUDIO_FIFO_BITS);
    fifo->samples_in_fifo++;
    if (fifo->samples_in_fifo >= AUDIO_FIFO_START_THRESHOLD) {
        fifo->state = FIFO_STATE_RUNNING;
    }
    return true;
}

// Add multiple samples to the FIFO
bool fifo_add_samples(audio_fifo_t *fifo, const audio_sample_t *samples_buffer, uint32_t num_samples_to_add) {
    // Check basic conditions
    if (num_samples_to_add == 0) {
        return true; // Nothing to add, operation considered successful
    }

    // Check if there's enough space in the FIFO
    uint32_t available_space = AUDIO_FIFO_SIZE - fifo->samples_in_fifo;
    if (num_samples_to_add > available_space) {
        return false; // Not enough space for all requested samples
    }

    // Determine how many samples can be written before wrap-around
    // AUDIO_FIFO_SIZE is the total capacity. write_idx is 0 to AUDIO_FIFO_SIZE-1.
    uint32_t samples_to_end = AUDIO_FIFO_SIZE - fifo->write_idx;

    if (num_samples_to_add <= samples_to_end) {
        // All samples fit without wrapping around the physical end of the buffer
        memcpy(&fifo->buffer[fifo->write_idx], samples_buffer, num_samples_to_add * sizeof(audio_sample_t));
    } else {
        // Samples will wrap around the physical end of the buffer
        // Part 1: Copy samples up to the end of the physical buffer
        memcpy(&fifo->buffer[fifo->write_idx], samples_buffer, samples_to_end * sizeof(audio_sample_t));

        // Part 2: Copy remaining samples from the beginning of the physical buffer
        uint32_t remaining_samples = num_samples_to_add - samples_to_end;
        memcpy(&fifo->buffer[0], samples_buffer + samples_to_end, remaining_samples * sizeof(audio_sample_t));
    }

    // Update FIFO state
    fifo->write_idx = (fifo->write_idx + num_samples_to_add) & (AUDIO_FIFO_BITS);
    fifo->samples_in_fifo += num_samples_to_add;
    if (fifo->samples_in_fifo >= AUDIO_FIFO_START_THRESHOLD) {
        fifo->state = FIFO_STATE_RUNNING;
    }
    return true;
}

// Take num_samples from the FIFO. Note that it is up to the consumer to actually
// access the bits in the FIFO directly via its instance of audio_fifo_t
uint32_t fifo_take_samples(audio_fifo_t *fifo, uint32_t num_samples) {
    if (fifo->state == FIFO_STATE_STOPPED) {
        return 0;
    }
    uint32_t samples_returned = MIN(fifo->samples_in_fifo, num_samples);
    fifo->read_idx = (fifo->read_idx + samples_returned) & (AUDIO_FIFO_BITS);
    fifo->samples_in_fifo -= samples_returned;
    // Only go back to stopped once fifo is fully exhausted
    if (fifo->samples_in_fifo == 0) {
        fifo->state = FIFO_STATE_STOPPED;
    }
    return samples_returned;
}
