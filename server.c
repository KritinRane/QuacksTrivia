/*
 * Kritin Rane
 * I pledge my honor that I have abided by the Stevens Honor System.
 *
 * server.c - Trivia Game Server
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
 
 #define MAX_PLAYERS 2
 #define MAX_QUESTIONS 50
 #define BACKLOG 3
 
 struct Entry {
     char prompt[1024];
     char options[3][50];
     int answer_idx;
 };
 
 struct Player {
     int fd;
     int score;
     char name[128];
 };
 
 int read_questions(struct Entry *arr, char *filename) {
     FILE *fp = fopen(filename, "r");
     if (!fp) {
         perror("fopen");
         return 0;
     }
 
     int count = 0;
     char line[1024];
 
     while (count < MAX_QUESTIONS) {
         /* Read question prompt - skip blank lines */
         char prompt[1024] = {0};
         int found = 0;
         while (fgets(line, sizeof(line), fp)) {
             /* strip newline */
             line[strcspn(line, "\n")] = '\0';
             if (strlen(line) > 0) {
                 strncpy(prompt, line, sizeof(prompt) - 1);
                 found = 1;
                 break;
             }
         }
         if (!found) break;
 
         /* Read options line */
         if (!fgets(line, sizeof(line), fp)) break;
         line[strcspn(line, "\n")] = '\0';
 
         char *token;
         char opts_copy[256];
         strncpy(opts_copy, line, sizeof(opts_copy) - 1);
         token = strtok(opts_copy, " ");
         for (int i = 0; i < 3 && token; i++) {
             strncpy(arr[count].options[i], token, 49);
             token = strtok(NULL, " ");
         }
 
         /* Read answer line */
         if (!fgets(line, sizeof(line), fp)) break;
         line[strcspn(line, "\n")] = '\0';
 
         /* Find answer index */
         arr[count].answer_idx = -1;
         for (int i = 0; i < 3; i++) {
             if (strcmp(arr[count].options[i], line) == 0) {
                 arr[count].answer_idx = i;
                 break;
             }
         }
 
         strncpy(arr[count].prompt, prompt, sizeof(arr[count].prompt) - 1);
         count++;
     }
 
     fclose(fp);
     return count;
 }
 
 int main(int argc, char **argv) {
     char *question_file = "qshort.txt";
     char *ip_address = "127.0.0.1";
     int port_number = 25555;
 
     int opt;
     while ((opt = getopt(argc, argv, "f:i:p:h")) != -1) {
         switch (opt) {
             case 'f':
                 question_file = optarg;
                 break;
             case 'i':
                 ip_address = optarg;
                 break;
             case 'p':
                 port_number = atoi(optarg);
                 break;
             case 'h':
                 printf("Usage: %s [-f question_file] [-i IP_address] [-p port_number] [-h]\n\n", argv[0]);
                 printf("-f question_file   Default to \"qshort.txt\";\n");
                 printf("-i IP_address      Default to \"127.0.0.1\";\n");
                 printf("-p port_number     Default to 25555;\n");
                 printf("-h                 Display this help info.\n");
                 return 0;
             default:
                 fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
                 return EXIT_FAILURE;
         }
     }
 
     /* Create server socket */
     int server_fd = socket(AF_INET, SOCK_STREAM, 0);
     if (server_fd < 0) {
         perror("socket");
         return EXIT_FAILURE;
     }
 
     int reuse = 1;
     if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
         perror("setsockopt");
         close(server_fd);
         return EXIT_FAILURE;
     }
 
     struct sockaddr_in server_addr;
     memset(&server_addr, 0, sizeof(server_addr));
     server_addr.sin_family = AF_INET;
     server_addr.sin_port = htons(port_number);
     if (inet_pton(AF_INET, ip_address, &server_addr.sin_addr) <= 0) {
         fprintf(stderr, "Invalid IP address: %s\n", ip_address);
         close(server_fd);
         return EXIT_FAILURE;
     }
 
     if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
         perror("bind");
         close(server_fd);
         return EXIT_FAILURE;
     }
 
     if (listen(server_fd, BACKLOG) < 0) {
         perror("listen");
         close(server_fd);
         return EXIT_FAILURE;
     }
 
     printf("Welcome to 392 Trivia!\n");
 
     /* Read questions */
     struct Entry questions[MAX_QUESTIONS];
     int num_questions = read_questions(questions, question_file);
 
     /* Accept players */
     struct Player players[MAX_PLAYERS];
     int num_players = 0;
 
     /* Track which players have sent their name yet */
     int pending_fds[MAX_PLAYERS];
     int num_pending = 0;
     for (int i = 0; i < MAX_PLAYERS; i++) pending_fds[i] = -1;
 
     while (num_players < MAX_PLAYERS) {
         fd_set read_fds;
         FD_ZERO(&read_fds);
         FD_SET(server_fd, &read_fds);
         int max_fd = server_fd;
 
         /* Watch pending clients for their name */
         for (int i = 0; i < num_pending; i++) {
             if (pending_fds[i] != -1) {
                 FD_SET(pending_fds[i], &read_fds);
                 if (pending_fds[i] > max_fd) max_fd = pending_fds[i];
             }
         }
 
         int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
         if (activity < 0) { perror("select"); break; }
 
         /* New connection */
         if (FD_ISSET(server_fd, &read_fds)) {
             struct sockaddr_in client_addr;
             socklen_t client_len = sizeof(client_addr);
             int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
             if (client_fd < 0) { perror("accept"); continue; }
 
             if (num_players + num_pending >= MAX_PLAYERS) {
                 printf("Max connection reached!\n");
                 close(client_fd);
             } else {
                 printf("New connection detected!\n");
                 const char *name_prompt = "Please type your name: ";
                 send(client_fd, name_prompt, strlen(name_prompt), 0);
                 pending_fds[num_pending++] = client_fd;
             }
         }
 
         /* Check pending clients for their name */
         for (int i = 0; i < num_pending; i++) {
             if (pending_fds[i] == -1) continue;
             if (!FD_ISSET(pending_fds[i], &read_fds)) continue;
 
             char name_buf[128] = {0};
             int bytes = recv(pending_fds[i], name_buf, sizeof(name_buf) - 1, 0);
             if (bytes <= 0) {
                 printf("Lost connection!\n");
                 close(pending_fds[i]);
                 close(server_fd);
                 return EXIT_FAILURE;
             }
             name_buf[strcspn(name_buf, "\r\n")] = '\0';
 
             players[num_players].fd = pending_fds[i];
             players[num_players].score = 0;
             strncpy(players[num_players].name, name_buf, sizeof(players[num_players].name) - 1);
             num_players++;
             pending_fds[i] = -1;
             num_pending--;
 
             printf("Hi %s!\n", name_buf);
         }
     }
 
     printf("The game starts now!\n");
 
     /* Game loop */
     for (int q = 0; q < num_questions; q++) {
         struct Entry *e = &questions[q];
 
         /* Print question to server screen */
         printf("Question %d: %s\n", q + 1, e->prompt);
         printf("1: %s\n", e->options[0]);
         printf("2: %s\n", e->options[1]);
         printf("3: %s\n", e->options[2]);
 
         /* Send question to each player */
         char msg[2048];
         snprintf(msg, sizeof(msg),
                  "Question %d: %s\nPress 1: %s\nPress 2: %s\nPress 3: %s\n",
                  q + 1, e->prompt,
                  e->options[0], e->options[1], e->options[2]);
 
         for (int p = 0; p < MAX_PLAYERS; p++) {
             send(players[p].fd, msg, strlen(msg), 0);
         }
 
         /* Use select to wait for first answer */
         fd_set read_fds;
         FD_ZERO(&read_fds);
         int max_fd = -1;
         for (int p = 0; p < MAX_PLAYERS; p++) {
             FD_SET(players[p].fd, &read_fds);
             if (players[p].fd > max_fd) max_fd = players[p].fd;
         }
 
         int answered = 0;
         /* Keep selecting until at least one player answers */
         while (!answered) {
             fd_set tmp = read_fds;
             int activity = select(max_fd + 1, &tmp, NULL, NULL, NULL);
             if (activity < 0) {
                 perror("select");
                 break;
             }
 
             for (int p = 0; p < MAX_PLAYERS; p++) {
                 if (FD_ISSET(players[p].fd, &tmp)) {
                     char ans_buf[16] = {0};
                     int bytes = recv(players[p].fd, ans_buf, sizeof(ans_buf) - 1, 0);
                     if (bytes <= 0) {
                         printf("Lost connection!\n");
                         for (int i = 0; i < MAX_PLAYERS; i++) close(players[i].fd);
                         close(server_fd);
                         return EXIT_FAILURE;
                     }
                     ans_buf[strcspn(ans_buf, "\r\n")] = '\0';
 
                     int choice = atoi(ans_buf);
                     if (choice >= 1 && choice <= 3) {
                         int chosen_idx = choice - 1;
                         if (chosen_idx == e->answer_idx) {
                             players[p].score += 1;
                             printf("%s answered first -- CORRECT!\n", players[p].name);
                         } else {
                             players[p].score -= 1;
                             printf("%s answered first -- WRONG!\n", players[p].name);
                         }
                         answered = 1;
                     }
                 }
             }
         }
 
         /* Broadcast correct answer */
         char answer_msg[256];
         snprintf(answer_msg, sizeof(answer_msg),
                  "The correct answer is: %s\n", e->options[e->answer_idx]);
         printf("%s", answer_msg);
         for (int p = 0; p < MAX_PLAYERS; p++) {
             send(players[p].fd, answer_msg, strlen(answer_msg), 0);
         }
         sleep(1);
     }
 
     /* Find winner(s) */
     int max_score = players[0].score;
     for (int p = 1; p < MAX_PLAYERS; p++) {
         if (players[p].score > max_score) max_score = players[p].score;
     }
 
     for (int p = 0; p < MAX_PLAYERS; p++) {
         if (players[p].score == max_score) {
             printf("Congrats, %s!\n", players[p].name);
         }
     }
 
     /* Close all connections */
     for (int p = 0; p < MAX_PLAYERS; p++) {
         close(players[p].fd);
     }
     close(server_fd);
 
     return 0;
 }