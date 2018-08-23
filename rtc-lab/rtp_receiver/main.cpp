#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
//#include <dnet.h>
#include <opus.h>
#include <arpa/inet.h>

#define UDP_PORT 63629
#define BUFFER_SIZE 1500
#define OPUS_RTP_PT 111
#define OUTPUT_FILE "./out.2ch.16000.s16le"
#define MAX_PENDING_BUFFER 50
#define CHANNELS 2
#define MAX_DECODE_FRAME_SIZE 960 // 48000Hz / 1000ms * 20ms

struct Buffer {
    unsigned char *data;
    int size;
};

bool last_played_rtp_seq_set = false;
uint16_t last_played_rtp_seq = 0;
std::map<int, Buffer*> pending_buffers;
FILE* out_file = nullptr;
OpusDecoder *decoder;
bool running = true;

void init_opus_decoder() {
    int err;
    decoder = opus_decoder_create(16000, 2, &err);
    if (err != OPUS_OK) {
        fprintf(stderr, "could not init opus encoder\n");
        exit(-1);
    }
}

void close_opus_decoder() {
    opus_decoder_destroy(decoder);
}

void opus_decoder_decode(uint8_t *data, int size) {
    uint8_t buffer[MAX_DECODE_FRAME_SIZE*CHANNELS*2];
    int frame_size = opus_decode(decoder, data, size, (int16_t*)buffer,
            MAX_DECODE_FRAME_SIZE, 0);
    fwrite(buffer, sizeof(int16_t), frame_size*CHANNELS, out_file);
}

void demux_rtp(unsigned char *data, int size) {
    // see RFC3550 for rtp
    bool rtp_extensions_present = (data[0]>>4&1) == 1;
    int rtp_contributing_sources = data[0]&0xf;
    int rtp_header_size = 12 + rtp_contributing_sources*4;

    uint8_t rtp_pt = data[1]&0x7f;
    uint16_t rtp_seq = data[2]<<8 | data[3];

    if (rtp_pt != OPUS_RTP_PT) {
        fprintf(stdout, "non opus packet ignored\n");
        return;
    }

    int rtp_extensions_size = 0;
    if (rtp_extensions_present) { // see RFC5285 for rtp header extensions
        rtp_extensions_size = data[rtp_header_size+2] << 8
                | data[rtp_header_size+3];
        rtp_extensions_size = rtp_extensions_size*4 + 4;
    }

    uint8_t *rtp_payload = data+rtp_header_size+rtp_extensions_size;
    int rtp_payload_size = size-rtp_header_size-rtp_extensions_size;

    // code below should be replace by a jitter buffer,
    // which will be overwhelming for this demo,
    // see https://github.com/search?q=jitter+buffer for jitter buffers

    // code below handles rtp packet in following steps:
    // - play it, if rtp_seq is continuous,
    // - queue it, if gap between last played is smaller than MAX_PENDING_BUFFER
    // - otherwise, drop it
    // - try to play continuous packets in buffer
    // - drop oldest packets if buffer is full
    if (!last_played_rtp_seq_set || rtp_seq == last_played_rtp_seq+1) {
        fprintf(stdout, "continuous, play %d\n", rtp_seq);
        last_played_rtp_seq = rtp_seq;
        opus_decoder_decode(rtp_payload, rtp_payload_size);
    } else if (rtp_seq-last_played_rtp_seq <= MAX_PENDING_BUFFER) {
        fprintf(stdout, "discontinuous, queue %d\n", rtp_seq);
        Buffer *pending_buffer = new Buffer();
        pending_buffer->data = new uint8_t[rtp_payload_size];
        pending_buffer->size = rtp_payload_size;
        pending_buffers.emplace(rtp_seq, pending_buffer);
    } else {
        fprintf(stdout, "discontinuous, drop %d\n", rtp_seq);
        return;
    }

    while (true) {
        auto next = pending_buffers.find(last_played_rtp_seq+1);
        if (next == pending_buffers.end()) {
            break;
        }

        fprintf(stdout, "discontinuous -> continuous, play %d\n",
                next->first);
        rtp_payload = next->second->data;
        rtp_payload_size = next->second->size;
        opus_decoder_decode(rtp_payload, rtp_payload_size);
        last_played_rtp_seq = next->first;
        delete next->second;
        pending_buffers.erase(next);
    }

    while (pending_buffers.size() > MAX_PENDING_BUFFER) {
        // note: last_played_rtp_seq is a uint16_t,
        //       it will wrap to 0 automatically
        auto next = pending_buffers.find(last_played_rtp_seq+1);
        if (next == pending_buffers.end()) {
            break;
        }
        fprintf(stdout, "purging pending buffer, play %d\n", next->first);
        last_played_rtp_seq = next->first;
        delete next->second;
        pending_buffers.erase(next);
    }
}

int read_udp_port(int port) {
    fprintf(stdout, "opening udp port %d...\n", port);
    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        fprintf(stdout, "failed to open udp socket\n");
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
        fprintf(stdout, "failed to set udp socket read timeout\n");
        return -1;
    }


    sockaddr_in sa;
    sa.sin_family = AF_INET;
    inet_aton("127.0.0.1", &sa.sin_addr);
    sa.sin_port = htons(port);
    int bind_err = bind(sockfd, (const sockaddr*)&sa, sizeof(sa));
    if (bind_err) {
        fprintf(stdout, "failed to bind to port %d\n", port);
        return -1;
    }

    fprintf(stdout, "reading from udp port %d...\n", port);

    uint8_t buffer[BUFFER_SIZE];
    while (running) {
        int b_recv = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, 0);
        if (b_recv < 0) {
            continue;
        }

        demux_rtp(buffer, b_recv);
    }

    close(sockfd);
    return 0;
}

void sig_handler(int sig) {
    running = false;
}

int main() {

    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_handler = sig_handler;
    sigaction(SIGINT,  &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    out_file = fopen(OUTPUT_FILE, "w");
    if (out_file == NULL) {
        fprintf(stderr, "could not open output %s\n", OUTPUT_FILE);
        exit(-1);
    }

    init_opus_decoder();

    read_udp_port(UDP_PORT);

    close_opus_decoder();

    fclose(out_file);

    return 0;
}
