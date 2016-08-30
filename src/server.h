#ifndef SERVER_H
#define SERVER_H

#define PORT "6373"
#define MAX_DATA_SIZE 10485760
#define MAX_EVENTS 100
#define CONNECTION_END -9
#define PERSISTENCE_LOG "/tmp/shibui/"

#include "map.h"
#include "partition.h"

void start_server(const char *);
int process_command(partition **, char *, int);

#endif
