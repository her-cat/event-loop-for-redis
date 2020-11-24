#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "until.h"
#include "server.h"

void loadServerConfig(const char *filename) {
    FILE *fp;
    char *delimiterPos;
    char buffer[CONFIG_MAX_LINE];

    fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("can't open config file");
        return;
    }

    while (fgets(buffer, CONFIG_MAX_LINE, fp)) {
        delimiterPos = strstr(buffer, CONFIG_DELIMITER);
        if (!delimiterPos) {
            continue;
        }

        trim(buffer);

        if (strncasecmp("port", buffer, 4) == 0) {
            server.port = atoi(delimiterPos + 1);
        } else if (strncasecmp("tcp_backlog", buffer, 11) == 0) {
            server.tcp_backlog = atoi(delimiterPos + 1);
        } else if (strncasecmp("document_root", buffer, 13) == 0) {
            server.document_root = delimiterPos + 1;
        }
    }
}
