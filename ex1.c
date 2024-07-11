#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "utils/dlist.h"
#include "utils/utils.h"
#include "wavelib/wave.h"
#include <alsa/asoundlib.h>

static const snd_pcm_sframes_t period_size = 64;

static void print_samples(uint8_t *buffer, snd_pcm_sframes_t nframes, int channels)
{
	int16_t *p = (uint16_t *)buffer;
	for (snd_pcm_sframes_t i = 0; i < nframes; p += channels)
	{
		printf("%d", *p);
		if (channels == 2)
			printf("|%d", *(p + 1));
		putchar(' ');
		if ((++i % 16) == 0)
			putchar('\n');
	}
	putchar('\n');
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <file_samples>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	Wave *wave = wave_create();
	if (wave == NULL)
	{
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}
	wave_set_number_of_channels(wave, 1);
	wave_set_sample_rate(wave, 44100);
	wave_set_bits_per_sample(wave, 16);

	snd_pcm_t *handle;
	int result = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);
	if (result < 0)
	{
		fprintf(stderr, "cannot open audio device %s (%s)\n",
				"default",
				snd_strerror(result));
		exit(EXIT_FAILURE);
	}
	result = snd_pcm_set_params(handle,
								SND_PCM_FORMAT_S16_LE,
								SND_PCM_ACCESS_RW_INTERLEAVED,
								wave_get_number_of_channels(wave),
								wave_get_sample_rate(wave),
								1,
								500000); /* 0.5 sec */
	if (result < 0)
	{
		fprintf(stderr, "snd_pcm_set_params: %s\n",
				snd_strerror(result));
		exit(EXIT_FAILURE);
	}
	result = snd_pcm_prepare(handle);
	if (result < 0)
	{
		fprintf(stderr, "cannot prepare audio interface for use (%s)\n",
				snd_strerror(result));
		exit(EXIT_FAILURE);
	}
	int frame_size = snd_pcm_frames_to_bytes(handle, 1);
	uint8_t buffer[period_size * frame_size];
	snd_pcm_sframes_t read_frames;
	int ten_seconds = 2 * wave_get_sample_rate(wave);
	for (int frame_index = 0; frame_index < ten_seconds; frame_index += read_frames)
	{
		read_frames = snd_pcm_readi(handle, buffer, period_size);
		if (read_frames < 0)
		{
			fprintf(stderr, "read from audio interface failed (%s)\n",
					snd_strerror(read_frames));
			exit(EXIT_FAILURE);
		}
		wave_append_samples(wave, buffer, read_frames);
		print_samples(buffer, read_frames, wave_get_number_of_channels(wave));
	}
	wave_store(wave, argv[1]);
	snd_pcm_close(handle);
}
