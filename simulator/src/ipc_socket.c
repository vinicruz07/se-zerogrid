#include "../include/ipc_socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET(s) closesocket(s)
    #define IS_INVALID_SOCKET(s) ((s) == INVALID_SOCKET)
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <fcntl.h>
    #include <errno.h>
    typedef int socket_t;
    #define CLOSE_SOCKET(s) close(s)
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
    #define IS_INVALID_SOCKET(s) ((s) < 0)
#endif

static socket_t server_fd = INVALID_SOCKET;
static socket_t clients[MAX_CLIENTS];
static char recv_buffers[MAX_CLIENTS][1024];
static int recv_lens[MAX_CLIENTS];

static void set_nonblocking(socket_t sock) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

static void process_command(SistemaFV *sys, const char *cmd) {
    int string_id, state;
    double val;

    char mode_buf[32];
    int int_val;

    if (sscanf(cmd, "SET_STRING %d %d", &string_id, &state) == 2) {
        pv_system_set_string(sys, string_id, state);
    } else if (sscanf(cmd, "SET_LOAD %lf", &val) == 1) {
        pv_system_set_load(sys, val);
    } else if (sscanf(cmd, "SET_IRRADIANCE %lf", &val) == 1) {
        pv_system_set_irradiance(sys, val);
    } else if (sscanf(cmd, "SET_MODE %31s", mode_buf) == 1) {
        if (strcmp(mode_buf, "AUTO") == 0 || strcmp(mode_buf, "1") == 0) {
            pv_system_set_mode(sys, 1);
        } else if (strcmp(mode_buf, "MANUAL") == 0 || strcmp(mode_buf, "0") == 0) {
            pv_system_set_mode(sys, 0);
        }
    } else if (sscanf(cmd, "SET_NUM_STRINGS %d", &int_val) == 1) {
        pv_system_set_num_strings(sys, int_val);
    } else if (strstr(cmd, "\"cmd\":\"SET_STRING\"") != NULL) {
        // Fallback para JSON simples
        char *id_ptr = strstr(cmd, "\"id\":");
        char *state_ptr = strstr(cmd, "\"state\":");
        if (id_ptr && state_ptr) {
            string_id = atoi(id_ptr + 5);
            state = atoi(state_ptr + 8);
            pv_system_set_string(sys, string_id, state);
        }
    } else if (strstr(cmd, "\"cmd\":\"SET_LOAD\"") != NULL) {
        char *val_ptr = strstr(cmd, "\"value\":");
        if (val_ptr) {
            val = atof(val_ptr + 8);
            pv_system_set_load(sys, val);
        }
    } else if (strstr(cmd, "\"cmd\":\"SET_IRRADIANCE\"") != NULL) {
        char *val_ptr = strstr(cmd, "\"value\":");
        if (val_ptr) {
            val = atof(val_ptr + 8);
            pv_system_set_irradiance(sys, val);
        }
    } else if (strstr(cmd, "\"cmd\":\"SET_MODE\"") != NULL) {
        char *mode_ptr = strstr(cmd, "\"mode\":");
        if (mode_ptr) {
            if (strstr(mode_ptr, "MANUAL")) {
                pv_system_set_mode(sys, 0);
            } else {
                pv_system_set_mode(sys, 1);
            }
        }
    } else if (strstr(cmd, "\"cmd\":\"SET_NUM_STRINGS\"") != NULL) {
        char *n_ptr = strstr(cmd, "\"num\":");
        if (n_ptr) {
            pv_system_set_num_strings(sys, atoi(n_ptr + 6));
        }
    }
}

int ipc_server_init(int port) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "[IPC] Falha ao inicializar Winsock.\n");
        return -1;
    }
#endif

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = INVALID_SOCKET;
        recv_lens[i] = 0;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (IS_INVALID_SOCKET(server_fd)) {
        fprintf(stderr, "[IPC] Falha ao criar socket do servidor.\n");
        return -1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    set_nonblocking(server_fd);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons((unsigned short)port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        fprintf(stderr, "[IPC] Falha ao associar porta %d (porta em uso?).\n", port);
        CLOSE_SOCKET(server_fd);
        server_fd = INVALID_SOCKET;
        return -1;
    }

    if (listen(server_fd, MAX_CLIENTS) == SOCKET_ERROR) {
        fprintf(stderr, "[IPC] Falha no listen da porta %d.\n", port);
        CLOSE_SOCKET(server_fd);
        server_fd = INVALID_SOCKET;
        return -1;
    }

    printf("[IPC] Servidor TCP escutando na porta %d...\n", port);
    return 0;
}

void ipc_server_cleanup(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!IS_INVALID_SOCKET(clients[i])) {
            CLOSE_SOCKET(clients[i]);
            clients[i] = INVALID_SOCKET;
        }
    }
    if (!IS_INVALID_SOCKET(server_fd)) {
        CLOSE_SOCKET(server_fd);
        server_fd = INVALID_SOCKET;
    }
#ifdef _WIN32
    WSACleanup();
#endif
    printf("[IPC] Servidor TCP finalizado.\n");
}

void ipc_server_poll(SistemaFV *sys) {
    if (IS_INVALID_SOCKET(server_fd)) return;

    // Aceita novos clientes
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    socket_t new_sock = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (!IS_INVALID_SOCKET(new_sock)) {
        set_nonblocking(new_sock);
        int accepted = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (IS_INVALID_SOCKET(clients[i])) {
                clients[i] = new_sock;
                recv_lens[i] = 0;
                printf("[IPC] Novo cliente conectado no slot %d.\n", i);
                accepted = 1;
                break;
            }
        }
        if (!accepted) {
            printf("[IPC] Maximo de clientes atingido. Desconectando novo socket.\n");
            CLOSE_SOCKET(new_sock);
        }
    }

    // Lê dados dos clientes conectados
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (IS_INVALID_SOCKET(clients[i])) continue;

        char buf[512];
        int bytes = recv(clients[i], buf, sizeof(buf) - 1, 0);
        if (bytes > 0) {
            buf[bytes] = '\0';
            // Concatena no buffer do cliente
            int space_left = (int)sizeof(recv_buffers[i]) - 1 - recv_lens[i];
            if (bytes > space_left) bytes = space_left;
            memcpy(recv_buffers[i] + recv_lens[i], buf, bytes);
            recv_lens[i] += bytes;
            recv_buffers[i][recv_lens[i]] = '\0';

            // Processa linhas completas terminadas em '\n'
            char *newline = NULL;
            while ((newline = strchr(recv_buffers[i], '\n')) != NULL) {
                *newline = '\0';
                if (newline > recv_buffers[i] && *(newline - 1) == '\r') {
                    *(newline - 1) = '\0';
                }
                if (strlen(recv_buffers[i]) > 0) {
                    process_command(sys, recv_buffers[i]);
                }
                int processed_len = (int)(newline - recv_buffers[i]) + 1;
                int remaining = recv_lens[i] - processed_len;
                memmove(recv_buffers[i], newline + 1, remaining);
                recv_lens[i] = remaining;
                recv_buffers[i][recv_lens[i]] = '\0';
            }
        } else if (bytes == 0) {
            // Cliente desconectou normalmente
            printf("[IPC] Cliente no slot %d desconectou.\n", i);
            CLOSE_SOCKET(clients[i]);
            clients[i] = INVALID_SOCKET;
            recv_lens[i] = 0;
        } else {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                printf("[IPC] Erro no slot %d (código %d). Desconectando.\n", i, err);
                CLOSE_SOCKET(clients[i]);
                clients[i] = INVALID_SOCKET;
                recv_lens[i] = 0;
            }
#else
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                CLOSE_SOCKET(clients[i]);
                clients[i] = INVALID_SOCKET;
                recv_lens[i] = 0;
            }
#endif
        }
    }
}

void ipc_server_broadcast(const char *msg) {
    if (!msg) return;
    int len = (int)strlen(msg);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!IS_INVALID_SOCKET(clients[i])) {
            int sent = send(clients[i], msg, len, 0);
            if (sent == SOCKET_ERROR) {
#ifdef _WIN32
                int err = WSAGetLastError();
                if (err != WSAEWOULDBLOCK) {
                    CLOSE_SOCKET(clients[i]);
                    clients[i] = INVALID_SOCKET;
                }
#else
                CLOSE_SOCKET(clients[i]);
                clients[i] = INVALID_SOCKET;
#endif
            }
        }
    }
}
