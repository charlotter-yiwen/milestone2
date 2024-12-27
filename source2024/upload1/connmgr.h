// connmgr.h

#ifndef CONNMGR_H
#define CONNMGR_H

#include "sbuffer.h"

/**
 * Starts the connection manager, listens for incoming sensor node connections.
 *
 * @param port The port number to listen on.
 * @param max_connections The maximum number of clients the server will handle.
 * @param buffer Pointer to the shared buffer where sensor data will be stored.
 */

void connmgr_listen(int port, int max_connections, sbuffer_t *buffer);



//void connmgr_listen(int port,  sbuffer_t *buffer);
#endif // CONNMGR_H
