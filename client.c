/*
 * Kritin Rane
 * I pledge my honor that I have abided by the Stevens Honor System.
 *
 * client.c - Trivia Game Client
 * CS 392 Spring 2026
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

void parse_connect(int argc, char **argv, int *server_fd) {
    char *ip_address = "127.0.0.1";
    int port_number = 25555;

    int opt;
    while ((opt = getopt(argc, argv, "i:p:h")) != -1) {
        switch (opt) {
            case 'i':
                ip_address = optarg;
                break;
            case 'p':
                port_number = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [-i IP_address] [-p port_number] [-h]\n\n", argv[0]);
                printf("-i IP_address      Default to \"127.0.0.1\";\n");
                printf("-p port_number     Default to 25555;\n");
                printf("-h                 Display this help info.\n");
                exit(0);
            default:
                fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
                exit(EXIT_FAILURE);
        }
    }

    *server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    if (inet_pton(AF_INET, ip_address, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", ip_address);
        close(*server_fd);
        exit(EXIT_FAILURE);
    }

    if (connect(*server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(*server_fd);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv) {
    int server_fd;
    parse_connect(argc, argv, &server_fd);

    char buf[4096];

    /* Main game loop: multiplex stdin and server_fd */
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd > STDIN_FILENO ? server_fd : STDIN_FILENO;

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select");
            break;
        }

        /* Data from server */
        if (FD_ISSET(server_fd, &read_fds)) {
            memset(buf, 0, sizeof(buf));
            int bytes = recv(server_fd, buf, sizeof(buf) - 1, 0);
            if (bytes <= 0) {
                /* Server closed connection - game over */
                break;
            }
            printf("%s", buf);
            fflush(stdout);
        }

        /* Data from stdin (user typed something) */
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            memset(buf, 0, sizeof(buf));
            if (fgets(buf, sizeof(buf), stdin) == NULL) {
                break;
            }
            /* Send what the user typed to server */
            send(server_fd, buf, strlen(buf), 0);
        }
    }

    close(server_fd);
    return 0;
}
