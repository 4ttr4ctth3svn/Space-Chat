#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>

//portul folosit
#define PORT 8899
#define BUFFER 1024
#define MAX_RELEE 10
#define INF 999999
#define MAX_IP 10
//codul de eroare returnat de anumite apeluri
extern int errno;

const char *ip_permise[MAX_IP] = {
    "127.0.0.1",   
    "192.168.4.20", 
    "192.168.10.14"  
};

struct date_thread{
    int client_thread;
    int id_client;
};

struct mesaj
{
    int id_sursa;
    int id_dest;
    int ttl;                
    char mesaje[BUFFER];
};

struct coada_mesaje{
    struct mesaj mesaje[BUFFER];
    int fata, spate;
    pthread_mutex_t blocare;
};
struct coada_mesaje coada[MAX_RELEE];

int ip_permis(const char *ip) {
    for (int i = 0; i < MAX_IP; i++) {
        if (ip_permise[i] != NULL && strcmp(ip, ip_permise[i]) == 0) {
            return 1; 
        }
    }
    return 0; 
}

void init_coada(struct coada_mesaje *coada)
{
    coada->fata=0;
    coada->spate=0;
    pthread_mutex_init(&coada->blocare,NULL);
}

int adaugare_in_coada(struct coada_mesaje *coada, struct mesaj mesaj_adugat)
{
    pthread_mutex_lock(&coada->blocare);
    if((coada->spate+1)%BUFFER==coada->fata)
    {
        pthread_mutex_unlock(&coada->blocare);
        return -1;//plina
    }
    coada->mesaje[coada->spate]=mesaj_adugat;
    coada->spate=(coada->spate+1)%BUFFER;
    pthread_mutex_unlock(&coada->blocare);
    return 0;
}

int eliminare_din_coada(struct coada_mesaje *coada, struct mesaj *mesaj_scos) 
{
    pthread_mutex_lock(&coada->blocare);
    if (coada->fata == coada->spate) {
        pthread_mutex_unlock(&coada->blocare);
        return -1; //gol
    }
    *mesaj_scos = coada->mesaje[coada->fata];
    coada->fata = (coada->fata + 1) % BUFFER;
    pthread_mutex_unlock(&coada->blocare);
    return 0;
}

struct client_infos
{
  int socket;
  int id_nod;
  struct client_infos *next;
};

struct client_infos *clienti=NULL;

pthread_mutex_t clienti_mutex = PTHREAD_MUTEX_INITIALIZER;

int relee_graf[MAX_RELEE][MAX_RELEE];

void clienti_noi(int client_socket, int id_nod)
{
    pthread_mutex_lock(&clienti_mutex);
    struct client_infos *client_nou =(struct client_infos *)malloc(sizeof(struct client_infos));
    client_nou->socket=client_socket;
    client_nou->id_nod=id_nod;
    client_nou->next=clienti;
    clienti=client_nou;
    pthread_mutex_unlock(&clienti_mutex);
}

struct client_infos *gasire_client(int id_nod)
{
    pthread_mutex_lock(&clienti_mutex);
    struct client_infos *client_curent = clienti;
    while(client_curent!=NULL)
    {
        if(client_curent->id_nod==id_nod)
        {
            pthread_mutex_unlock(&clienti_mutex);
            return client_curent;
        }
        client_curent=client_curent->next;
    }
    pthread_mutex_unlock(&clienti_mutex);
    return NULL;
}

void eliminare_client(int id_nod)
{
    pthread_mutex_lock(&clienti_mutex);
    struct client_infos **client_curent = &clienti;
    while(*client_curent!=NULL)
    {
        if((*client_curent)->id_nod==id_nod)
        {
            struct client_infos *client_eliminat = *client_curent;
            *client_curent=(*client_curent)->next;
            free(client_eliminat);
            break;
        }
        client_curent=&((*client_curent)->next);
    }
    pthread_mutex_unlock(&clienti_mutex);
}

int nevizitat_total(int s[MAX_RELEE])
{
    for(int i=0;i<MAX_RELEE;i++)
    {
        if(s[i]==0)
            return 1;
    }
    return 0;
}

int minim_i(int d[],int s[])
{
    int minim=INF +1;
    int min_i=-1;
    for(int i=0;i<MAX_RELEE;i++)
    {
        if(minim>d[i]&&s[i]==0)
        {
            minim=d[i];
            min_i=i;
        }
    }
    return min_i;
}

void dijkstra(int graf[MAX_RELEE][MAX_RELEE], int sursa, int dest, int *drum, int *lung_drum, int *cost_total)
{
    int dist[MAX_RELEE];
    int vizitate[MAX_RELEE];
    int anterior[MAX_RELEE];
    int i,j,min,min_i;

    for(i=0;i<MAX_RELEE;i++)
    {
        dist[i]=INF;
        vizitate[i]=0;
        anterior[i]=-1;
    }
    for(i=0;i<MAX_RELEE;i++)
    {
        if(graf[sursa][i]!=0)
        {
            dist[i]=graf[sursa][i];
            anterior[i]=sursa;
        }   
    }
    vizitate[sursa]=1;
    while(nevizitat_total(vizitate))
    {
        i=minim_i(dist,vizitate);
        vizitate[i]= 1;
        for(j=0;j<MAX_RELEE;j++)
        {
            if(dist[j]>dist[i]+graf[i][j])
            {
                dist[j]=dist[i]+graf[i][j];
                anterior[j]=i;
            }
        }
    }

    *lung_drum=0;
    *cost_total=dist[dest];
    for(i=dest; i!=sursa; i=anterior[i])
    {
        drum[(*lung_drum)++]=i;
    }
    for(i=0;i<*lung_drum/2;i++)
    {
        int aux=drum[i];
        drum[i]=drum[*lung_drum-i-1];
        drum[*lung_drum-i-1]=aux;
    }


}

void *handle_client(void *arg) 
{
    struct date_thread *date = (struct date_thread *)arg;
    int client = date->client_thread;
    int client_id = date->id_client;
    free(date);

    printf("[server] Thread started for client ID %d on socket %d.\n", client_id, client);

    struct mesaj mesaj_primit;
    int drum[MAX_RELEE], lung_drum, cost_total;

    while (1) {
        if (read(client, &mesaj_primit, sizeof(mesaj_primit)) <= 0) {
            printf("[server] Client ID %d disconnected.\n", client_id);
            eliminare_client(client_id);
            close(client);
            return NULL;
        }

        printf("[server] Message received from client ID %d for destination ID %d: %s\n",
               mesaj_primit.id_sursa, mesaj_primit.id_dest, mesaj_primit.mesaje);

        dijkstra(relee_graf, mesaj_primit.id_sursa, mesaj_primit.id_dest, drum, &lung_drum, &cost_total);

        printf("[server] Route calculated for client ID %d: ", client_id);
        for (int i = 0; i < lung_drum; i++) {
            printf("%d ", drum[i]);
        }
        printf("\n");
        //prelucrare ttl(scaderea costului min din ttl)
        //simulam delay-ul bazat pe costul total
        sleep(cost_total);

        
        struct client_infos *client_dest = gasire_client(mesaj_primit.id_dest);
        if (client_dest != NULL) {
            write(client_dest->socket, &mesaj_primit, sizeof(mesaj_primit));
        } else {
            printf("[server] Destination client ID %d is not connected.\n", mesaj_primit.id_dest);
        }
    }

    return NULL;
}

int main()
{
    struct sockaddr_in server; // structura folosita de server
    struct sockaddr_in from;
    int sd; // descriptorul de socket
    int client;
    socklen_t length = sizeof(from);

    int client_id_counter = 0;
    pthread_mutex_t id_mutex = PTHREAD_MUTEX_INITIALIZER;

    for(int i=0;i<MAX_RELEE;i++)
    {
        for(int j=0;j<MAX_RELEE;j++)
        {
            relee_graf[i][j]=(i==j)?0:INF;
        }
        init_coada(&coada[i]);
    }

    relee_graf[0][1]=1;
    relee_graf[1][0]=1;
    relee_graf[0][3]=3;
    relee_graf[3][0]=3;
    relee_graf[0][4]=4;
    relee_graf[4][0]=4;
    relee_graf[1][6]=2;
    relee_graf[6][1]=2;
    relee_graf[1][2]=2;
    relee_graf[2][1]=2;
    relee_graf[3][5]=3;
    relee_graf[5][3]=3;

    //crearea unui socket
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server]Eroare la socket().\n");
        return errno;
    }

    //pregatirea structurilor de date
    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    //umplem structura folosita de server
    //stabilirea familiei de socket-uri
    server.sin_family = AF_INET;
    //acceptam orice adresa
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    //utilizam un port utilizator 
    server.sin_port = htons(PORT);

    //atasam socket-ul
    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server]Eroare la bind().\n");
        return errno;
    }

    //punem serverul sa asculte daca vin clienti sa se conecteze
    if (listen(sd, 5) == -1)
    {
        perror("[server]Eroare la listen().\n");
        return errno;
    }

    //servim in mod concurent clientii
    while (1)
    {
        client = accept(sd, (struct sockaddr *)&from, &length);
        if (client < 0) {
            perror("[server] Eroare la accept.\n");
            continue;
        }

       
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("[server] Incercare de conexiune de la IP-ul: %s\n", client_ip);

        
        if (!ip_permis(client_ip)) {
            printf("[server] Conexiune respinsa pentru IP-ul: %s\n", client_ip);
            close(client);
            continue;
        }

        
        pthread_mutex_lock(&id_mutex);
        int client_id = client_id_counter++;
        pthread_mutex_unlock(&id_mutex);

        if (write(client, &client_id, sizeof(client_id)) <= 0) 
        {
            perror("[server] Error sending client ID.\n");
            close(client);
            continue;
        }

        printf("[server] Assignat ID-ul %d clientului cu IP-ul: %s\n", client_id, client_ip);

        //adaugam clientul in lista
        clienti_noi(client, client_id);

        //aloc mem pt thread-uri
        struct date_thread *date = (struct date_thread *)malloc(sizeof(struct date_thread));
        if (!date) {
            perror("[server]Eroare alocare memorie.\n");
            close(client);
            continue;
        }
        date->client_thread = client;
        date->id_client = client_id;

        //cream thread-uri pt gestionarea clientilor
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, date) != 0) {
            perror("[server] Eroare creare thread.\n");
            free(date);
            close(client);
            continue;
        }

        pthread_detach(thread_id);
        printf("[server] Thread creat pentru ID-ul %d.\n", client_id);
    } //main 

    close(sd);
    return 0;
}