#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define AUDIO_PID 0x1100
#define TS_SIZE 188

static volatile sig_atomic_t stop_requested;
static int audio_pipe = -1;
static pid_t audio_writer = -1;
static uint8_t pending_byte;
static int have_pending;
static unsigned long long audio_bytes;
static unsigned long long dropped_bytes;
static unsigned long audio_packets;

static void on_signal(int sig)
{
    (void)sig;
    stop_requested = 1;
}

static void write_all(int fd, const uint8_t *data, size_t len)
{
    while (len > 0) {
        ssize_t written = write(fd, data, len);
        if (written > 0) {
            data += written;
            len -= (size_t)written;
            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        break;
    }
}

static void writer_child(const char *fifo, int input)
{
    uint8_t buf[4096];
    int output = open(fifo, O_WRONLY);
    if (output < 0)
        _exit(1);
    while (!stop_requested) {
        ssize_t got = read(input, buf, sizeof(buf));
        if (got == 0)
            break;
        if (got < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        write_all(output, buf, (size_t)got);
    }
    close(output);
    _exit(0);
}

static void start_writer(const char *fifo)
{
    int fds[2];
    if (pipe(fds) < 0) {
        perror("pipe");
        exit(1);
    }
    if (fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | O_NONBLOCK) < 0) {
        perror("pipe nonblock");
        exit(1);
    }
    audio_writer = fork();
    if (audio_writer < 0) {
        perror("fork");
        exit(1);
    }
    if (audio_writer == 0) {
        close(fds[1]);
        writer_child(fifo, fds[0]);
    }
    close(fds[0]);
    audio_pipe = fds[1];
}

static void queue_lpcm_le(const uint8_t *data, size_t len)
{
    uint8_t converted[TS_SIZE];
    size_t in = 0;
    size_t out = 0;
    if (have_pending && len > 0) {
        converted[out++] = data[in++];
        converted[out++] = pending_byte;
        have_pending = 0;
    }
    while (in + 1 < len) {
        converted[out++] = data[in + 1];
        converted[out++] = data[in];
        in += 2;
    }
    if (in < len) {
        pending_byte = data[in];
        have_pending = 1;
    }
    if (out > 0) {
        ssize_t written = write(audio_pipe, converted, out);
        if (written > 0) {
            audio_bytes += (unsigned long long)written;
            dropped_bytes += (unsigned long long)(out - (size_t)written);
        } else {
            dropped_bytes += (unsigned long long)out;
        }
    }
}

static void process_ts(const uint8_t *ts)
{
    int pid;
    int afc;
    int off;
    int payload_start;
    if (ts[0] != 0x47)
        return;
    pid = ((ts[1] & 0x1f) << 8) | ts[2];
    if (pid != AUDIO_PID)
        return;
    afc = (ts[3] >> 4) & 3;
    if (!(afc & 1))
        return;
    off = 4;
    if (afc & 2)
        off += 1 + ts[4];
    if (off >= TS_SIZE)
        return;
    payload_start = ts[1] & 0x40;
    if (payload_start) {
        if (TS_SIZE - off < 20 || ts[off] != 0 || ts[off + 1] != 0 || ts[off + 2] != 1)
            return;
        off += 20;
    }
    if (off < TS_SIZE) {
        audio_packets++;
        queue_lpcm_le(ts + off, TS_SIZE - off);
    }
}

static void process_rtp(const uint8_t *data, size_t len)
{
    size_t off;
    size_t i;
    if (len < 12 || (data[0] >> 6) != 2)
        return;
    off = 12 + (size_t)(data[0] & 15) * 4;
    if (data[0] & 0x10) {
        if (off + 4 > len)
            return;
        off += 4 + (size_t)((data[off + 2] << 8) | data[off + 3]) * 4;
    }
    if (off >= len)
        return;
    for (i = off; i + TS_SIZE <= len; i += TS_SIZE)
        process_ts(data + i);
}

int main(int argc, char **argv)
{
    int fd;
    int ifindex;
    struct ifreq ifr;
    struct sockaddr_ll bind_addr;
    uint8_t frame[2048];

    if (argc != 3) {
        fprintf(stderr, "usage: %s <iface> <lpcm-fifo>\n", argv[0]);
        return 2;
    }
    start_writer(argv[2]);
    fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if (fd < 0) {
        perror("AF_PACKET");
        return 1;
    }
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, argv[1], IFNAMSIZ - 1);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("SIOCGIFINDEX");
        return 1;
    }
    ifindex = ifr.ifr_ifindex;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sll_family = AF_PACKET;
    bind_addr.sll_protocol = htons(ETH_P_IP);
    bind_addr.sll_ifindex = ifindex;
    if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        perror("bind");
        return 1;
    }
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);
    while (!stop_requested) {
        ssize_t got = recv(fd, frame, sizeof(frame), 0);
        size_t ip;
        size_t udp;
        size_t ihl;
        if (got < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (got < 42 || frame[12] != 0x08 || frame[13] != 0x00)
            continue;
        ip = 14;
        if ((frame[ip] >> 4) != 4 || frame[ip + 9] != 17)
            continue;
        ihl = (size_t)(frame[ip] & 15) * 4;
        udp = ip + ihl;
        if (udp + 8 > (size_t)got)
            continue;
        if (frame[udp + 2] != 0x04 || frame[udp + 3] != 0x04)
            continue;
        process_rtp(frame + udp + 8, (size_t)got - udp - 8);
    }
    fprintf(stderr, "audio_ts_packets=%lu lpcm_bytes=%llu lpcm_dropped=%llu\n",
            audio_packets, audio_bytes, dropped_bytes);
    if (audio_pipe >= 0)
        close(audio_pipe);
    if (audio_writer > 0) {
        kill(audio_writer, SIGTERM);
        waitpid(audio_writer, NULL, 0);
    }
    close(fd);
    return 0;
}
