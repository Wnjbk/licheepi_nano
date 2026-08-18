/*
 * Minimal lazycast d2.py C port for F1C200S experiments.
 *
 * Scope:
 * - Connect to the Miracast source RTSP server on TCP 7236.
 * - Follow lazycast d2.py M1..M7 WFD negotiation.
 * - Receive RTP/MPEG-TS on UDP port 1028.
 * - Extract video PES payload from TS PID 0x1011 and write H.264 ES to stdout.
 *
 * This intentionally does not play video. Pipe stdout to a recorder, parser,
 * or Cedar playback tool.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define RTSP_PORT 7236
#define RTP_PORT 1028
#define RTP_MAX 2048
#define TS_SIZE 188
#define VIDEO_PID 0x1011
#define RTSP_BUF_MAX 32768

static volatile sig_atomic_t g_stop;
static unsigned long long g_h264_bytes;
static unsigned long g_ts_packets;
static unsigned long g_video_ts_packets;

struct rtsp_reader {
    char data[RTSP_BUF_MAX];
    size_t len;
};

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static int set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static ssize_t send_all(int fd, const char *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, buf + off, len - off, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        off += (size_t)n;
    }
    return (ssize_t)off;
}

static void sendf(int fd, const char *fmt, ...)
{
    char buf[8192];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= sizeof(buf))
        die("RTSP message too large");
    fprintf(stderr, "<--------\n%.*s", n, buf);
    if (send_all(fd, buf, (size_t)n) < 0)
        die("send failed: %s", strerror(errno));
}

static int content_length_of(const char *msg)
{
    const char *p = strstr(msg, "Content-Length:");
    if (!p)
        return 0;
    p += strlen("Content-Length:");
    while (*p == ' ' || *p == '\t')
        p++;
    return atoi(p);
}

static int read_rtsp_message(int fd, struct rtsp_reader *r, char *out,
                             size_t out_cap, const char *tag)
{
    for (;;) {
        char *hdr_end = NULL;
        for (size_t i = 0; i + 3 < r->len; i++) {
            if (r->data[i] == '\r' && r->data[i + 1] == '\n' &&
                r->data[i + 2] == '\r' && r->data[i + 3] == '\n') {
                hdr_end = r->data + i + 4;
                break;
            }
        }

        if (hdr_end) {
            size_t header_len = (size_t)(hdr_end - r->data);
            char header[4096];
            size_t hn = header_len < sizeof(header) - 1 ? header_len : sizeof(header) - 1;
            memcpy(header, r->data, hn);
            header[hn] = '\0';

            int body_len = content_length_of(header);
            if (body_len < 0)
                body_len = 0;
            size_t msg_len = header_len + (size_t)body_len;
            if (r->len >= msg_len) {
                if (msg_len + 1 > out_cap)
                    die("RTSP message too large for output buffer");
                memcpy(out, r->data, msg_len);
                out[msg_len] = '\0';
                memmove(r->data, r->data + msg_len, r->len - msg_len);
                r->len -= msg_len;
                fprintf(stderr, "%s\n%.*s", tag, (int)msg_len, out);
                return (int)msg_len;
            }
        }

        if (r->len == sizeof(r->data))
            die("RTSP receive buffer overflow");

        ssize_t n = recv(fd, r->data + r->len, sizeof(r->data) - r->len, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            die("recv %s failed: %s", tag, strerror(errno));
        }
        if (n == 0)
            die("recv %s: peer closed", tag);
        r->len += (size_t)n;
    }
}

static int pop_rtsp_message(struct rtsp_reader *r, char *out, size_t out_cap,
                            const char *tag)
{
    char *hdr_end = NULL;
    for (size_t i = 0; i + 3 < r->len; i++) {
        if (r->data[i] == '\r' && r->data[i + 1] == '\n' &&
            r->data[i + 2] == '\r' && r->data[i + 3] == '\n') {
            hdr_end = r->data + i + 4;
            break;
        }
    }
    if (!hdr_end)
        return 0;

    size_t header_len = (size_t)(hdr_end - r->data);
    char header[4096];
    size_t hn = header_len < sizeof(header) - 1 ? header_len : sizeof(header) - 1;
    memcpy(header, r->data, hn);
    header[hn] = '\0';

    int body_len = content_length_of(header);
    if (body_len < 0)
        body_len = 0;
    size_t msg_len = header_len + (size_t)body_len;
    if (r->len < msg_len)
        return 0;
    if (msg_len + 1 > out_cap)
        die("RTSP message too large for output buffer");

    memcpy(out, r->data, msg_len);
    out[msg_len] = '\0';
    memmove(r->data, r->data + msg_len, r->len - msg_len);
    r->len -= msg_len;
    fprintf(stderr, "%s\n%.*s", tag, (int)msg_len, out);
    return 1;
}

static int recv_rtsp_available(int fd, struct rtsp_reader *r)
{
    for (;;) {
        if (r->len == sizeof(r->data))
            die("RTSP receive buffer overflow");
        ssize_t n = recv(fd, r->data + r->len, sizeof(r->data) - r->len, 0);
        if (n > 0) {
            r->len += (size_t)n;
            continue;
        }
        if (n == 0)
            return -1;
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        die("recv RTSP failed: %s", strerror(errno));
    }
}

static int find_cseq(const char *msg, char *out, size_t out_sz)
{
    const char *p = strstr(msg, "CSeq:");
    if (!p)
        return -1;
    const char *e = strstr(p, "\r\n");
    if (!e)
        e = p + strlen(p);
    size_t n = (size_t)(e - p);
    if (n + 1 > out_sz)
        n = out_sz - 1;
    memcpy(out, p, n);
    out[n] = '\0';
    return 0;
}

static int parse_session(const char *msg, char *session, size_t session_sz)
{
    const char *p = strstr(msg, "Session:");
    if (!p)
        return -1;
    p += strlen("Session:");
    while (*p == ' ' || *p == '\t')
        p++;
    const char *e = p;
    while (*e && *e != '\r' && *e != '\n' && *e != ';' && *e != ' ')
        e++;
    size_t n = (size_t)(e - p);
    if (n == 0)
        return -1;
    if (n >= session_sz)
        n = session_sz - 1;
    memcpy(session, p, n);
    session[n] = '\0';
    return 0;
}

static void rtsp_ok_same_cseq(int fd, const char *msg)
{
    if (!strncmp(msg, "RTSP/1.0", 8))
        return;

    char cseq[64];
    if (find_cseq(msg, cseq, sizeof(cseq)) == 0)
        sendf(fd, "RTSP/1.0 200 OK\r\n%s\r\n\r\n", cseq);
}

static void rtsp_public_ok_same_cseq(int fd, const char *msg)
{
    char cseq[64];
    if (find_cseq(msg, cseq, sizeof(cseq)) < 0)
        die("request without CSeq");

    sendf(fd, "RTSP/1.0 200 OK\r\n"
              "%s\r\n"
              "Public: org.wfa.wfd1.0, SET_PARAMETER, GET_PARAMETER\r\n\r\n",
          cseq);
}

static void rtsp_parameters_ok_same_cseq(int fd, const char *req,
                                         const char *body)
{
    char cseq[64];
    if (find_cseq(req, cseq, sizeof(cseq)) < 0)
        die("request without CSeq");

    sendf(fd, "RTSP/1.0 200 OK\r\n"
              "%s\r\n"
              "Content-Type: text/parameters\r\n"
              "Content-Length: %zu\r\n\r\n%s",
          cseq, strlen(body), body);
}

static void send_idr_request(int fd, int *cseq)
{
    const char *body = "wfd_idr_request\r\n";
    (*cseq)++;
    sendf(fd, "SET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n"
              "Content-Length: %zu\r\n"
              "Content-Type: text/parameters\r\n"
              "CSeq: %d\r\n\r\n%s",
          strlen(body), *cseq, body);
}

static int rtp_payload_offset(const uint8_t *pkt, int len)
{
    if (len < 12)
        return -1;
    int cc = pkt[0] & 0x0f;
    int off = 12 + cc * 4;
    if (off > len)
        return -1;
    if (pkt[0] & 0x10) {
        if (off + 4 > len)
            return -1;
        int ext_len_words = ((int)pkt[off + 2] << 8) | pkt[off + 3];
        off += 4 + ext_len_words * 4;
        if (off > len)
            return -1;
    }
    return off;
}

static void write_h264_from_ts_payload(const uint8_t *payload, int payload_len,
                                       int payload_start)
{
    if (payload_len <= 0)
        return;

    int off = 0;
    if (payload_start) {
        if (payload_len < 9)
            return;
        if (!(payload[0] == 0x00 && payload[1] == 0x00 && payload[2] == 0x01))
            return;
        int pes_header_len = payload[8];
        off = 9 + pes_header_len;
        if (off > payload_len)
            return;
    }

    if (payload_len > off) {
        size_t want = (size_t)(payload_len - off);
        size_t wrote = fwrite(payload + off, 1, want, stdout);
        g_h264_bytes += (unsigned long long)wrote;
        if (wrote != want)
            g_stop = 1;
    }
}

static void process_ts_packet(const uint8_t *ts)
{
    if (ts[0] != 0x47)
        return;
    g_ts_packets++;

    int payload_start = !!(ts[1] & 0x40);
    int pid = ((ts[1] & 0x1f) << 8) | ts[2];
    if (pid != VIDEO_PID)
        return;
    g_video_ts_packets++;

    int afc = (ts[3] >> 4) & 0x03;
    if (!(afc & 0x01))
        return;

    int off = 4;
    if (afc & 0x02) {
        int ad_len = ts[4];
        off += 1 + ad_len;
    }
    if (off >= TS_SIZE)
        return;

    write_h264_from_ts_payload(ts + off, TS_SIZE - off, payload_start);
}

static void process_rtp_packet(const uint8_t *pkt, int len)
{
    int off = rtp_payload_offset(pkt, len);
    if (off < 0)
        return;

    int payload_len = len - off;
    if (payload_len < TS_SIZE)
        return;

    const uint8_t *p = pkt + off;
    int ts_count = payload_len / TS_SIZE;
    for (int i = 0; i < ts_count; i++)
        process_ts_packet(p + i * TS_SIZE);
}

static int bind_rtp_socket(const char *bind_ip)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        die("udp socket failed: %s", strerror(errno));

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(RTP_PORT);
    if (inet_pton(AF_INET, bind_ip, &addr.sin_addr) != 1)
        die("bad bind ip: %s", bind_ip);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        die("bind udp %s:%d failed: %s", bind_ip, RTP_PORT, strerror(errno));
    return fd;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <source-ip> [h264-output-file]\n", argv[0]);
        return 2;
    }

    const char *source_ip = argv[1];
    if (argc >= 3) {
        FILE *f = freopen(argv[2], "wb", stdout);
        if (!f)
            die("open output failed: %s", strerror(errno));
    }
    setvbuf(stdout, NULL, _IONBF, 0);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);

    int rtsp = socket(AF_INET, SOCK_STREAM, 0);
    if (rtsp < 0)
        die("tcp socket failed: %s", strerror(errno));

    int one = 1;
    setsockopt(rtsp, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    setsockopt(rtsp, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in src;
    memset(&src, 0, sizeof(src));
    src.sin_family = AF_INET;
    src.sin_port = htons(RTSP_PORT);
    if (inet_pton(AF_INET, source_ip, &src.sin_addr) != 1)
        die("bad source ip: %s", source_ip);

    if (connect(rtsp, (struct sockaddr *)&src, sizeof(src)) < 0)
        die("connect %s:%d failed: %s", source_ip, RTSP_PORT, strerror(errno));

    struct sockaddr_in local;
    socklen_t local_len = sizeof(local);
    if (getsockname(rtsp, (struct sockaddr *)&local, &local_len) < 0)
        die("getsockname failed: %s", strerror(errno));
    char local_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local.sin_addr, local_ip, sizeof(local_ip));
    fprintf(stderr, "local sink ip: %s\n", local_ip);

    int rtp = bind_rtp_socket(local_ip);

    char buf[8192];
    struct rtsp_reader rr;
    memset(&rr, 0, sizeof(rr));

    read_rtsp_message(rtsp, &rr, buf, sizeof(buf), "---M1--->");
    rtsp_public_ok_same_cseq(rtsp, buf);

    sendf(rtsp, "OPTIONS * RTSP/1.0\r\n"
                "CSeq: 1\r\n"
                "Require: org.wfa.wfd1.0\r\n\r\n");
    read_rtsp_message(rtsp, &rr, buf, sizeof(buf), "-------->");

    read_rtsp_message(rtsp, &rr, buf, sizeof(buf), "---M3--->");
    char m3_body[2048];
    snprintf(m3_body, sizeof(m3_body),
        "wfd_client_rtp_ports: RTP/AVP/UDP;unicast 1028 0 mode=play\r\n"
        "wfd_audio_codecs: LPCM 00000002 00\r\n"
        "wfd_video_formats: 00 00 02 04 00000001 00000000 00000000 00 0000 0000 00 none none\r\n"
        "wfd_3d_video_formats: none\r\n"
        "wfd_coupled_sink: none\r\n"
        "wfd_connector_type: 05\r\n"
        "wfd_uibc_capability: input_category_list=GENERIC, HIDC;generic_cap_list=Keyboard, Mouse;hidc_cap_list=Keyboard/USB, Mouse/USB;port=none\r\n"
        "wfd_standby_resume_capability: none\r\n"
        "wfd_content_protection: none\r\n"
        "wfd2_audio_codecs: LPCM 000001ff 00\r\n"
        "wfd2_video_formats: 40 01 04 0080 000001ffbdeb 000155557fff 000000000fff 10 0000 001f 11, 01 01 0080 000001ffbdeb 0001555557ff 000000000fff 10 0000 001f 11 00\r\n"
        "wfd2_video_stream_control: 0f 0f\r\n"
        "wfd2_rotation_capability: supported\r\n%s",
        strstr(buf, "wfd_idr_request_capability") ?
            "wfd_idr_request_capability: 1\r\n" : "");
    rtsp_parameters_ok_same_cseq(rtsp, buf, m3_body);

    read_rtsp_message(rtsp, &rr, buf, sizeof(buf), "---M4--->");
    rtsp_ok_same_cseq(rtsp, buf);

    read_rtsp_message(rtsp, &rr, buf, sizeof(buf), "---M5--->");
    rtsp_ok_same_cseq(rtsp, buf);

    sendf(rtsp, "SETUP rtsp://%s/wfd1.0/streamid=0 RTSP/1.0\r\n"
                "CSeq: 5\r\n"
                "Transport: RTP/AVP/UDP;unicast;client_port=1028\r\n\r\n",
          source_ip);
    read_rtsp_message(rtsp, &rr, buf, sizeof(buf), "-------->");

    char session[128];
    if (parse_session(buf, session, sizeof(session)) < 0)
        die("no Session in SETUP response");
    fprintf(stderr, "session: %s\n", session);

    sendf(rtsp, "PLAY rtsp://%s/wfd1.0/streamid=0 RTSP/1.0\r\n"
                "CSeq: 6\r\n"
                "Session: %s\r\n\r\n",
          source_ip, session);
    read_rtsp_message(rtsp, &rr, buf, sizeof(buf), "-------->");
    fprintf(stderr, "---- Negotiation successful ----\n");

    if (set_nonblock(rtsp) < 0 || set_nonblock(rtp) < 0)
        die("nonblock failed: %s", strerror(errno));

    uint8_t pkt[RTP_MAX];
    time_t last_packet = time(NULL);
    time_t last_idle_log = 0;
    time_t rtp_stall_since = 0;
    unsigned long rtp_packets = 0;
    unsigned long last_rtp_packets = 0;
    unsigned long long rtp_bytes = 0;
    int local_cseq = 102;
    int initial_idr_sent = 0;
    while (!g_stop) {
        while (pop_rtsp_message(&rr, buf, sizeof(buf), "<---RTSP---")) {
            if (strstr(buf, "wfd_trigger_method: TEARDOWN")) {
                g_stop = 1;
                break;
            }
            rtsp_ok_same_cseq(rtsp, buf);
        }
        if (g_stop)
            break;

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(rtsp, &rfds);
        FD_SET(rtp, &rfds);
        int maxfd = rtsp > rtp ? rtsp : rtp;

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int rc = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            die("select failed: %s", strerror(errno));
        }

        if (rc == 0) {
            time_t now = time(NULL);
            if (rtp_packets > 0 && rtp_packets == last_rtp_packets) {
                if (rtp_stall_since == 0)
                    rtp_stall_since = now;
                if (now - rtp_stall_since == 30) {
                    fprintf(stderr,
                            "RTP packet counter stalled, keep RTSP session alive: packets=%lu bytes=%llu stall=%ld\n",
                            rtp_packets, rtp_bytes,
                            (long)(now - rtp_stall_since));
                }
            } else {
                last_rtp_packets = rtp_packets;
                rtp_stall_since = 0;
            }
            if (now - last_packet > 10 && now - last_idle_log >= 10) {
                fprintf(stderr,
                        "waiting for RTP packets... packets=%lu bytes=%llu idle=%ld\n",
                        rtp_packets, rtp_bytes, (long)(now - last_packet));
                last_idle_log = now;
            }
            continue;
        }

        if (FD_ISSET(rtsp, &rfds)) {
            if (recv_rtsp_available(rtsp, &rr) < 0)
                break;
            while (pop_rtsp_message(&rr, buf, sizeof(buf), "<---RTSP---")) {
                if (strstr(buf, "wfd_trigger_method: TEARDOWN")) {
                    g_stop = 1;
                    break;
                }
                rtsp_ok_same_cseq(rtsp, buf);
            }
        }

        if (FD_ISSET(rtp, &rfds)) {
            ssize_t n = recv(rtp, pkt, sizeof(pkt), 0);
            if (n > 0) {
                process_rtp_packet(pkt, (int)n);
                last_packet = time(NULL);
                rtp_packets++;
                rtp_bytes += (unsigned long long)n;
                if (!initial_idr_sent && rtp_packets >= 12) {
                    fprintf(stderr, "request initial IDR after %lu RTP packets\n",
                            rtp_packets);
                    send_idr_request(rtsp, &local_cseq);
                    initial_idr_sent = 1;
                }
            }
        }
    }

    fflush(stdout);
    fprintf(stderr,
            "final stats: rtp_packets=%lu rtp_bytes=%llu ts_packets=%lu video_ts_packets=%lu h264_bytes=%llu\n",
            rtp_packets, rtp_bytes, g_ts_packets, g_video_ts_packets,
            g_h264_bytes);
    close(rtp);
    close(rtsp);
    return 0;
}
