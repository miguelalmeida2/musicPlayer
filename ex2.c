#include <alsa/asoundlib.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "utils/dlist.h"
#include "utils/utils.h"
#include "wavelib/wave.h"

static Node *commands, *wave_files, *wave_records, *wave_records_names, *queue;
static pthread_t pthrd1;
static const snd_pcm_sframes_t period_size = 64;
volatile int running = 1;

typedef struct command
{
    char letter;
    char *args;
    char *description;
    void (*action)(char *);
} Command;

Command *new_command(char letter, char *args, char *description, void (*action)(char *))
{
    Command *cmd = malloc(sizeof *cmd);
    if (cmd == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        exit(-1);
    }
    cmd->letter = letter;
    cmd->args = args;
    cmd->description = description;
    cmd->action = action;
    return cmd;
}

void sorted_insert_if_matches(const char *it, void *context)
{
    if (string_match(context, it))
    {
        char *text = malloc(strlen(it) + 1);
        strcpy(text, it);
        if (wave_files->next == wave_files)
        {
            // List is empty
            list_insert_front(wave_files, (void *)text);
            return;
        }
        for (Node *p = wave_files->next; p != wave_files; p = p->next)
        {
            if (strcmp(it, p->data) <= 0)
            {
                list_insert_front(p->prev, (void *)text);
                return;
            }
        }
        // Insert at end
        list_insert_rear(wave_files, (void *)text);
    }
}

void free_nodes_and_data(Node *node)
{
    Node *next;
    for (Node *p = node->next; p != node; p = next)
    {
        next = p->next;
        free(p->data);
        free(p);
    }
    free(node);
}

void show_wave_files()
{
    printf("\nWave files found:\n");
    int idx = 1;
    for (Node *p = wave_files->next; p != wave_files; p = p->next, idx++)
    {
        printf(" %d. '%s'\n", idx, (char *)p->data);
    }
    printf("\n");
}

void add_to_queue(char *wave_index)
{
    if (wave_index == NULL)
    {
        printf("\nInvalid\n\n");
        return;
    }

    int idx;
    sscanf(wave_index, "%d", &idx);
    if (idx <= 0)
    {
        printf("\nInvalid\n\n");
        return;
    }

    // Find wave to add to queue
    Node *node = wave_files;
    for (int i = 0; i < idx; i++)
    {
        node = node->next;
        if (node == wave_files)
        {
            printf("\nInvalid\n\n");
            return;
        }
    }

    // Add wave to queue
    char *file_path = malloc(strlen(node->data) + 1);
    if (file_path == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        exit(-1);
    }
    strcpy(file_path, node->data);
    list_insert_rear(queue, file_path);
}

void clear_queue(char *unused)
{
    free_nodes_and_data(queue);
    queue = list_create();
}

void show_queue(char *unused)
{
    if (queue == queue->next)
    {
        printf("\nQueue is empty\n");
    }
    else
    {
        printf("\nQueue:\n");
        int idx = 1;
        for (Node *p = queue->next; p != queue; p = p->next, idx++)
        {
            printf(" %d. '%s'\n", idx, (char *)p->data);
        }
    }
    printf("\n");
}

void play_queue(char *unused)
{
    printf("\n");
    for (Node *p = queue->next; p != queue; p = p->next)
    {
        const char *file_path = (char *)p->data;
        Wave *wave = wave_load(file_path);
        printf("Playing '%s'...\n", file_path);
        wave_play(wave);
        wave_destroy(wave);
    }
}

Wave *wave_record(Wave *wave)
{
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
    int four_seconds = 4 * wave_get_sample_rate(wave);
    // running == 1
    // frame_index < four_seconds
    for (int frame_index = 0; running == 1; frame_index += read_frames)
    {
        printf(".");
        read_frames = snd_pcm_readi(handle, buffer, period_size);
        if (read_frames < 0)
        {
            fprintf(stderr, "read from audio interface failed (%s)\n",
                    snd_strerror(read_frames));
            exit(EXIT_FAILURE);
        }
        wave_append_samples(wave, buffer, read_frames);
    }
    snd_pcm_close(handle);
    char capture_name[25] = {"wave_capture.wav"};
    list_insert_rear(wave_records, wave);
    list_insert_rear(wave_records_names, &capture_name);
    // pthread_exit(wave);
    return wave;
};

void start_record()
{
    Wave *wave = wave_create();
    running = 1;
    pthread_create(&pthrd1, NULL, wave_record, wave);
}

void stop_record()
{
    running = 0;
    int res = 0;
    printf("\n\tSTOPPING RECORDING THREAD\n");
    pthread_join(pthrd1, res);
    printf("\n\n\tRECORDING STOPPED!\n\n");
    // list_insert_rear(wave_records, result_wave);
    // list_insert_rear(wave_records_names, list_size(wave_records));
}

void list_records()
{
    if (wave_records_names == wave_records_names->next)
    {
        printf("\nRecords List is empty\n");
    }
    else
    {
        printf("\nWave Recordings In List:\n");
        int idx = 1;
        for (Node *p = wave_records_names->next; p != wave_records_names; p = p->next, idx++)
        {
            printf(" %d. '%s'\n", idx, (char *)p->data);
        }
        printf("\n");
    }
}

int save_record(const unsigned char idx)
{
    Node *wave_node = wave_records;
    int wave_records_size = list_size(wave_records);
    for (int i = 0; i < wave_records_size; i++)
    {
        wave_node = wave_node->next;
    }

    // Save Wave Recording in File

    char filename[100] = {"wave_capture.wav"};
    printf("> ");
    fgets(filename, sizeof filename, stdin);

    // Storing File With Name Given By User
    int res = wave_store((Wave *)(wave_node->data), filename);
    if (res == -1)
    {
        printf("ERROR: Could Not Save The Current Recording!");
        exit(-1);
    }
    return 0;
}

void help(char *unused)
{
    printf("\nHelp:\n");
    for (Node *p = commands->next; p != commands; p = p->next)
    {
        Command *cmd = p->data;
        printf(" %c", cmd->letter);
        if (cmd->args != NULL)
        {
            printf(" %s", cmd->args);
        }
        printf(" %s\n", cmd->description);
    }
    printf("\n");
}

void leave_program(char *unused)
{
    if (running == 1)
    {
        running = 0;
        pthread_join(pthrd1, NULL);
    }
    free_nodes_and_data(commands);
    free_nodes_and_data(wave_files);
    free_nodes_and_data(wave_records);
    free_nodes_and_data(wave_records_names);
    free_nodes_and_data(queue);
    exit(0);
}

void command_execute(char c, char *param)
{
    for (Node *p = commands->next; p != commands; p = p->next)
    {
        Command *cmd = p->data;
        if (cmd->letter == c)
        {
            cmd->action(param);
            return;
        }
    }
}

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <directory>\n", argv[0]);
        return -1;
    }

    commands = list_create();
    wave_files = list_create();
    wave_records_names = list_create();
    wave_records = list_create();
    queue = list_create();

    file_tree_foreach(argv[1], &sorted_insert_if_matches, "*.wav");
    show_wave_files();

    Command *cmd;
    cmd = new_command('a', "<wave_index>", "Add to Audio File To Queue to Play", &add_to_queue);
    list_insert_rear(commands, cmd);
    cmd = new_command('q', NULL, "Show Queue of Playable Files", &show_queue);
    list_insert_rear(commands, cmd);
    cmd = new_command('z', NULL, "Play Queue of Playable Files", &play_queue);
    list_insert_rear(commands, cmd);
    cmd = new_command('c', NULL, "Clear queue of Playable Files", &clear_queue);
    list_insert_rear(commands, cmd);
    cmd = new_command('r', NULL, "Start Recording new Wave File", &start_record);
    list_insert_rear(commands, cmd);
    cmd = new_command('p', NULL, "Stop the Recording of Wave File", &stop_record);
    list_insert_rear(commands, cmd);
    cmd = new_command('l', NULL, "Show List of Recorded Files", &list_records);
    list_insert_rear(commands, cmd);
    cmd = new_command('s', NULL, "Save File from Recordings List", &save_record);
    list_insert_rear(commands, cmd);
    cmd = new_command('h', NULL, "Help", &help);
    list_insert_rear(commands, cmd);
    cmd = new_command('e', NULL, "Exit Application", &leave_program);
    list_insert_rear(commands, cmd);

    char line[100];
    while (1)
    {
        printf("> ");
        fgets(line, sizeof line, stdin);
        char *command = strtok(line, " \n");
        char *arg = strtok(NULL, " \n");
        if (command != NULL)
            command_execute(*command, arg);
    }
    return 0;
}
