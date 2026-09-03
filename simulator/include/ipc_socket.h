#ifndef IPC_SOCKET_H
#define IPC_SOCKET_H

#include "pv_system.h"

#define MAX_CLIENTS 10
#define DEFAULT_PORT 9001

int ipc_server_init(int port);
void ipc_server_cleanup(void);
void ipc_server_poll(SistemaFV *sys);
void ipc_server_broadcast(const char *msg);

#endif // IPC_SOCKET_H
