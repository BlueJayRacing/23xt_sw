#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pb_encode.h>
#include "proto/channel_sample.pb.h"

#define PORT 5000
#define SAMPLES_PER_BATCH 50
#define PACKETS         1000   
#define PACKETS_PER_SEC  20  

// dummy base stamp and increment values
#define BASE_TIMESTAMP_US  1723000000000000ULL
#define SAMPLE_INTERVAL_US (1000000 / (PACKETS_PER_SEC * SAMPLES_PER_BATCH))

// channel map
static const uint8_t CHANNELS[] = {6, 8, 10, 12, 14, 7, 9, 11, 13, 15,   // analog
                                   16, 17, 18, 19, 20, 21};              // digital
#define N_CHANNELS (sizeof(CHANNELS) / sizeof(CHANNELS[0]))
#define FIRST_DIGITAL 16
#define N_DIGITAL      6

// dummy analog values 
#define UINT24_MAX   0xFFFFFF
#define ANALOG_STEP  200000

int main(void)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket dead");
        return 1;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // counts samples sent so timestamps increase
    uint64_t sample_index = 0;

    // digital channels report a running total
    uint32_t digital_counters[N_DIGITAL] = {0};

    for (int p = 0; p < PACKETS; p++) {
        ChannelSampleBatch batch = ChannelSampleBatch_init_default;
        for (int i = 0; i < SAMPLES_PER_BATCH; i++) {
            uint8_t channel = CHANNELS[i % N_CHANNELS];
            batch.samples[i].timestamp           = BASE_TIMESTAMP_US + sample_index * SAMPLE_INTERVAL_US;
            batch.samples[i].internal_channel_id = channel;
            if (channel >= FIRST_DIGITAL) {
                digital_counters[channel - FIRST_DIGITAL]++;
                batch.samples[i].value = digital_counters[channel - FIRST_DIGITAL];
            } else {
                batch.samples[i].value = (channel * 100000 + i + p * ANALOG_STEP) & UINT24_MAX;
            }
            sample_index++;
        }
        batch.samples_count = SAMPLES_PER_BATCH;

        uint8_t buffer[ChannelSampleBatch_size];
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        if (!pb_encode(&stream, ChannelSampleBatch_fields, &batch)) {
            printf("encode kaboomed: %s\n", PB_GET_ERROR(&stream));
            return 1;
        }
        sendto(sockfd, buffer, stream.bytes_written, 0, (struct sockaddr*) &servaddr, sizeof(servaddr));

        printf("sent packet %d: %zu bytes in %d samples\n",
               p + 1, stream.bytes_written, SAMPLES_PER_BATCH);

        usleep(1000000 / PACKETS_PER_SEC);
    }

    close(sockfd);
    return 0;
}
