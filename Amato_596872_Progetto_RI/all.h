#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <dirent.h>
#include <libgen.h>

#define MAX_UTENTI 1000
#define SERVER_PORT 4242
#define MAX_CODA_LISTEN 10
#define MAX_LENGHT_RIGA 1000