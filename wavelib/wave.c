#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <alsa/asoundlib.h>
#include "../utils/dlist.h"
#include "wave.h"

#define DATA_OFFSET 44
#define SOUND_DEVICE "default"
#define BYTE_SIZE 8

Wave *wave_create()
{
    Wave *wave = malloc(sizeof *wave);
    if (wave == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        exit(-1);
    }
    // Subchunk2Size == NumSamples * NumChannels * BitsPerSample/8
    // Node *node_data_list = list_create();
    wave->wave_data_list = list_create();
    wave->sub_chunk_2_size = 0;
    return wave;
}

int wave_store(Wave *wave, char *filename)
{
    char buffer[DATA_OFFSET] = {0};
    FILE *ptr = fopen(filename, "wb");
    if (ptr == NULL)
    {
        printf("Error opening file\n");
        return -1;
    }
    // TODO  Ffalta escrever o header do ficheiro Wave como deve ser para depois escrever a datalist

    // Escrita do Header no ficheiro
    write_wave_header_info(wave, buffer);
    int res = fwrite(buffer, DATA_OFFSET, 1, ptr); // The fwrite() function returns the number of members successfully written
    if (res != 1)
    {
        fprintf(stderr, "Couldnn't write file!\n");
        return -1;
    }

    const unsigned int bytes_per_sample = wave_get_bits_per_sample(wave) / 8;
    const unsigned int frame_size = bytes_per_sample * wave_get_number_of_channels(wave);
    unsigned int frame_count = 0;

    Node *list = wave->wave_data_list;
    // Escrita dos blocos de frames no ficheiro
    for (Node *p = list->next; p != list; p = p->next)
    {
        int result = fwrite(((DataBuffer *)p->data)->heap_data, frame_size, (((DataBuffer *)p->data)->data_frame_count), ptr);
        if (result != 0)
        {
            frame_count += (((DataBuffer *)&p->data)->data_frame_count);
        }
        else
        {
            printf("Frames Written before error %d\n", frame_count);
            printf("Error writing on file %s\n", filename);
            return -1;
        }
    }

    void *aux(void *p)
    {
        int result = fwrite(((DataBuffer *)p->data)->heap_data, frame_size, (((DataBuffer *)p->data)->data_frame_count), ptr);
        if (result != 0)
        {
            frame_count += (((DataBuffer *)&p->data)->data_frame_count);
        }
        else
        {
            printf("Frames Written before error %d\n", frame_count);
            printf("Error writing on file %s\n", filename);
            exit(-1);
        }
    }

    list_foreach(list, aux, list);

    fclose(ptr);

    // TODO Atualizar o tamanho do ficheiro no header depois da escrita

    return 0;
}

// writes static wave header content
void write_wave_header_info(Wave *wave, char *buffer)
{
    int32_t temp = (int32_t)(wave->sub_chunk_2_size + DATA_OFFSET - BYTE_SIZE);
    memcpy(buffer, RIFF_HEADER, 4);               // "RIFF"
    memcpy(buffer + 4, &temp, 4);                 // Size of the wav portion of the file, which follows the first 8 bytes. File size - 8
    memcpy(buffer + 8, WAVE_HEADER, 4);           // "WAVE"
    memcpy(buffer + 12, FMT_HEADER, 4);           // "fmt "
    buffer[16] = (int32_t)CHUNK_SIZE_PCM;         // should be 16 for PCM
    buffer[20] = (int16_t)AUDIO_FORMAT_PCM;       // Should be 1 for PCM.
    buffer[22] = (int16_t)wave->num_channels;     // num_channels
    memcpy(buffer + 24, &(wave->sample_rate), 4); // sample_rate
    temp = (int32_t)(wave->sample_rate * wave->num_channels * wave->bits_per_sample / BYTE_SIZE);
    memcpy(buffer + 28, &temp, 4);                                                  // byte_rate
    buffer[32] = (int16_t)(wave->num_channels * wave->bits_per_sample / BYTE_SIZE); // sample alignment
    buffer[34] = (int16_t)(wave->bits_per_sample);                                  // number of bits per sample;
    memcpy(buffer + 36, DATA_HEADER, 4);                                            // "data"
    memcpy(buffer + 40, &(wave->sub_chunk_2_size), 4);                              // Number of bytes in data. Number of samples * num_channels * sample byte size
}

Wave *wave_load(const char *filename)
{
    FILE *ptr = fopen(filename, "rb");
    if (ptr == NULL)
    {
        printf("Error opening file\n");
        exit(1);
    }

    Wave *wave = malloc(sizeof *wave);
    if (wave == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        exit(-1);
    }

    wave->file = ptr;
    fread((void *)&wave->chunk_id, 1, sizeof *wave - sizeof wave->file, ptr);

    return wave;
}

void wave_destroy(Wave *wave)
{
    Node *list = wave->wave_data_list;
    for (Node *p = list->prev; list->prev != list; p = p->prev)
    {
        p->prev->next = p->next;
        p->next->prev = p->prev;
        free(p->data);
        free(p);
    }
    free(wave);
}

void wave_set_bits_per_sample(Wave *wave, int bits_per_sample)
{
    wave->bits_per_sample = bits_per_sample;
}

int wave_get_bits_per_sample(Wave *wave)
{
    return wave->bits_per_sample;
}

void wave_set_number_of_channels(Wave *wave, int number_of_channels)
{
    wave->num_channels = number_of_channels;
}

int wave_get_number_of_channels(Wave *wave)
{
    return wave->num_channels;
}

void wave_set_sample_rate(Wave *wave, int sample_rate)
{
    wave->sample_rate = sample_rate;
}

int wave_get_sample_rate(Wave *wave)
{
    return wave->sample_rate;
}

size_t wave_append_samples(Wave *wave, uint8_t *buffer, size_t frame_count)
{
    const unsigned int bytes_per_sample = wave_get_bits_per_sample(wave) / 8;
    const unsigned int frame_size = wave_get_bits_per_sample(wave) * wave_get_number_of_channels(wave);

    DataBuffer *data_buffer = malloc(sizeof *data_buffer);
    if (NULL == data_buffer)
    {
        fprintf(stderr, "Out of memory\n");
        exit(-1);
    }
    unsigned char *data = malloc(frame_size * frame_count);
    if (NULL == data)
    {
        fprintf(stderr, "Out of memory\n");
        exit(-1);
    }
    data_buffer->heap_data = data;
    memcpy(data, buffer, frame_size * frame_count);

    data_buffer->data_frame_count = frame_count;
    list_insert_rear(wave->wave_data_list, data_buffer);
    wave->sub_chunk_2_size += frame_count * wave_get_number_of_channels(wave) * bytes_per_sample;

    return frame_count;
}

size_t wave_get_samples(Wave *wave, size_t frame_index,
                        char *buffer, size_t frame_count)
{
    const unsigned int bytes_per_sample = wave_get_bits_per_sample(wave) / 8;
    const unsigned int frame_size = bytes_per_sample * wave->num_channels;
    const int result = fseek(wave->file, DATA_OFFSET + (frame_index * frame_size), SEEK_SET);
    if (result != 0)
    {
        printf("fseek error\n");
        exit(1);
    }
    return fread(buffer, frame_size, frame_count, wave->file);
}

void wave_play(Wave *wave)
{
    snd_pcm_t *handle = NULL;
    int result = snd_pcm_open(&handle, SOUND_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
    if (result < 0)
    {
        printf("snd_pcm_open(&handle, %s, SND_PCM_STREAM_PLAYBACK, 0): %s\n",
               SOUND_DEVICE, snd_strerror(result));
        exit(EXIT_FAILURE);
    }

    snd_config_update_free_global();

    result = snd_pcm_set_params(handle,
                                SND_PCM_FORMAT_S16_LE,
                                SND_PCM_ACCESS_RW_INTERLEAVED,
                                wave_get_number_of_channels(wave),
                                wave_get_sample_rate(wave),
                                1,
                                500000);
    if (result < 0)
    {
        fprintf(stderr, "Playback open error: %s\n", snd_strerror(result));
        exit(EXIT_FAILURE);
    }

    const snd_pcm_sframes_t period_size = 64;
    int frame_size = snd_pcm_frames_to_bytes(handle, 1);

    uint8_t buffer[period_size * frame_size];
    size_t frame_index = 0;

    size_t read_frames = wave_get_samples(wave, frame_index, (char *)buffer, period_size);

    while (read_frames > 0)
    {
        snd_pcm_sframes_t wrote_frames = snd_pcm_writei(handle, buffer, read_frames);
        if (wrote_frames < 0)
            wrote_frames = snd_pcm_recover(handle, wrote_frames, 0);
        if (wrote_frames < 0)
        {
            printf("snd_pcm_writei failed: %s\n", snd_strerror(wrote_frames));
            break;
        }

        if (wrote_frames < read_frames)
            fprintf(stderr, "Short write (expected %li, wrote %li)\n",
                    read_frames, wrote_frames);

        frame_index += period_size;
        read_frames = wave_get_samples(wave, frame_index, (char *)buffer, period_size);
    }
    /* pass the remaining samples, otherwise they're dropped in close */
    result = snd_pcm_drain(handle);
    if (result < 0)
        printf("snd_pcm_drain failed: %s\n", snd_strerror(result));

    snd_pcm_close(handle);
    snd_config_update_free_global();
}
