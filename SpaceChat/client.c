#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int errno;
#define BUFFER 1024

int port;

struct mesaj {
    int id_sursa;          
    int id_dest;           
    char mesaje[BUFFER];   
};

int main(int argc, char *argv[]) {
    int sd; //descriptorul de socket
    struct sockaddr_in server; //structura pt adersa server-ului
    struct mesaj mesaj_trimis, mesaj_primit;
    int client_id;

    if (argc != 3) {
        printf("Usage: %s <server_address> <port>\n", argv[0]);
        return -1;
    }

    //setam port-ul
    port = atoi(argv[2]);

    //cream socket-ul
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket.\n");
        return errno;
    }

    //structura server-ului
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    //conectarea la server
    if (connect(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("[client] Connection error.\n");
        return errno;
    }

    //citim id-ul clientului din server
    if (read(sd, &client_id, sizeof(client_id)) <= 0) {
        perror("[client] Error receiving client ID from server.\n");
        close(sd);
        return errno;
    }

    printf("[client] Assigned client ID: %d\n", client_id);

    //scriem mesajul si destinatia 
    while (1) {
        printf("Enter your message: ");
        if (fgets(mesaj_trimis.mesaje, BUFFER, stdin) == NULL) {
            perror("Error reading message.\n");
            continue;
        }
        mesaj_trimis.mesaje[strcspn(mesaj_trimis.mesaje, "\n")] = '\0'; 

        printf("Enter destination client ID: ");
        if (scanf("%d", &mesaj_trimis.id_dest) != 1) {
            perror("Error reading destination ID.\n");
            while (getchar() != '\n'); //scapam de orice exces de input invalid 
            continue;
        }
        getchar();
        //dam id-ul 
        mesaj_trimis.id_sursa = client_id;

        //trimitem mesajul la server
        if (write(sd, &mesaj_trimis, sizeof(mesaj_trimis)) <= 0) {
            perror("[client] Error sending message to server.\n");
            break;
        }
           printf("astept raspuns: ");
        //aici primim raspunsul de la server
        if (read(sd, &mesaj_primit, sizeof(mesaj_primit)) <= 0) {
            perror("[client] Error receiving response from server.\n");
            break;
        }

        printf("[client] Response from server:\n");
        printf("From: %d, To: %d, Message: %s\n",
               mesaj_primit.id_sursa, mesaj_primit.id_dest, mesaj_primit.mesaje);
    }

    //gata cu conectarea 
    close(sd);
    return 0;
}