#include "all.h"

/***************************************/
/*           STRUTTURE DATI            */
/***************************************/

// Struttura dati degli utenti
typedef struct utente {
    char username[50];
    int porta;
    int socket;
} utente_t;

// Funzioni di inizializzazione del device
int creaSocket();
void bindSocket(int sd, int porta, struct sockaddr_in* my_addr);
void listenSocket(int sd, int coda_listen);
void connectSocket(int client_socket, int porta, struct sockaddr_in* my_addr);
bool isSocketValid(int sd);
void homeDevice();
int initDevice(struct sockaddr_in* server_addr, utente_t utente[MAX_UTENTI], int message_socket[MAX_UTENTI]);
int initPeer(struct sockaddr_in* peer_addr, int dev_port);
void initUtenti(utente_t utente[MAX_UTENTI]);
void initMessageSocket(int message_socket[MAX_UTENTI]);

// Funzioni generali
void tockenizzaRichiesta(char* buffer, char** richiesta);
void resettaRichiesta(char** richiesta);
void riceviComando(char** richiesta, char* buffer, int dev_port, char mio_username[50], bool* controllo);
void inviaMessaggio(int sd, int len, uint16_t lmsg, char* messaggio);
void riceviMessaggio(int sd, int len, uint16_t lmsg, char* messaggio);
void homeUtente();
void rimuoviParole(char messaggio[1024], char parole_da_eliminare[100]);

// Funzioni per la gestione del signup
bool controlloSignup(char** richiesta);

// Funzioni per la gestione del login
void login(char* buffer, int dev_port, char mio_username[50]);
void successoLogin(char mio_username[50], char** richiesta);
bool controlloLogin(char** richiesta);

// Funzioni per la gestione del logout
bool controlloOut(char** richiesta);
void out(char* buffer, int dev_port, char mio_username[50]);
void successoLogout(char mio_username[50], int message_socket[MAX_UTENTI], utente_t utente[MAX_UTENTI]);

// Funzioni per la gestione della chat
void chat(char* buffer, char mio_username[50]);
void schermoChat();
void recuperaChat(char mio_username[50], char* dest_username);
void memorizzaMessaggioInviato(int dest_socket, utente_t utente[MAX_UTENTI], char mio_username[50], char* buffer);
void memorizzaMessaggioRicevuto(char** richiesta, utente_t utente[MAX_UTENTI], char mio_username[50], char* buffer);
void chatOnline(char** richiesta, int message_socket[MAX_UTENTI], int client_socket, utente_t utente[MAX_UTENTI], struct sockaddr_in server_addr, bool* inChat, char mio_username[50]);
void chatOffline(char** richiesta, char dest_username[50], bool* inChatServer, char mio_username[50]);
void utentiOnline(int client_socket, char buffer[1024]);
void aggiungiInChat(int client_socket, char dest_username[50], int message_socket[MAX_UTENTI], utente_t utente[MAX_UTENTI], int dev_port, char mio_username[50], struct sockaddr_in server_addr);
void aggiunto(utente_t utente[MAX_UTENTI], bool* inChat, int message_socket[MAX_UTENTI], struct sockaddr_in server_addr, char username_mittente[50], char porta_mittente[50]);
int socketUscente(utente_t utente[MAX_UTENTI], char** richiesta);
bool controlloChat(char** richiesta);

// Funzioni per la gestione della share
void share(char file_inviato[50], char mio_username[50], int message_socket[MAX_UTENTI]);
void riceviFile(int socket_pronto, char mio_username[50]);

// Funzioni per la gestione dell'hanging
bool controlloHanging(char** richiesta);
void hanging(char mio_username[50], char buffer[1024]);
void successoHanging(int client_socket);

// Funzioni per la gestione della show
bool controlloShow(char** richiesta);
void show(char mio_username[50], char buffer[1024]);
void successoShow(int client_socket);

/***************************************/
/*                MAIN                 */
/***************************************/

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Errore di sintassi.\n");
        printf("Per mandare in esecuzione un device bisogna specificarne la porta a lui associata:\n");
        printf("./dev <porta>\n");
        exit(-1);
    }

    int len, i, client_socket, peer_socket, connected_socked, ret, dev_port, fdmax, socket_pronto, socket_uscente;
    int message_socket[MAX_UTENTI];
    uint16_t lmsg;
    char buffer[1024];
    char comando[1024];
    char mio_username[50];
    char messaggio[1024];
    char tempo[100];
    char* richiesta[10];
    char dest_username[50];
    bool inChat = false;
    bool inChatServer = false;
    bool controllo = true;
    time_t current_timestamp;
    struct sockaddr_in server_addr;
    struct sockaddr_in peer_addr;
    struct sockaddr_in client_addr;
    utente_t utente[MAX_UTENTI];

    mio_username[0] = '\0';

    fd_set master;
    fd_set read_fds;
    
    dev_port = atoi(argv[1]);
    client_socket = initDevice(&server_addr, utente, message_socket);
    peer_socket = initPeer(&peer_addr, dev_port);

    FD_SET(0, &master);
    FD_SET(client_socket, &master);
    FD_SET(peer_socket, &master);
    fdmax = (client_socket > peer_socket) ? client_socket : peer_socket;

    while(1) {
        read_fds = master;
        select(fdmax + 1, &read_fds, NULL, NULL, NULL);

        for (socket_pronto = 0; socket_pronto <= fdmax; socket_pronto++) {
            if (FD_ISSET(socket_pronto, &read_fds)) {
                if (socket_pronto == 0) {
                    if (inChat) {                               /* invia messaggi agli altri device */
                        fgets(buffer, 1024, stdin);
                        if (buffer[strlen(buffer) - 1] == '\n') {
                            buffer[strlen(buffer) - 1] = '\0';
                        }

                        if (strlen(buffer) == 0) continue;
                        strcpy(comando, buffer);
                        tockenizzaRichiesta(comando, richiesta);
                        snprintf(messaggio, sizeof(messaggio), "[%s]: %s", mio_username, buffer);
                        len = strlen(messaggio) + 1;
                        lmsg = htons(len);
                        if (strcmp(buffer, "/q") == 0) {
                            inChat = false;
                            homeUtente();
                            for (i = 0; i < MAX_UTENTI; i++) {
                                if (message_socket[i] != 0) {
                                    inviaMessaggio(message_socket[i], len, lmsg, messaggio);
                                    message_socket[i] = 0;
                                }
                            }
                            resettaRichiesta(richiesta);
                            continue;
                        }
                        if (strcmp(buffer, "/u") == 0) {
                            if (FD_ISSET(client_socket, &master)) utentiOnline(client_socket, buffer);
                            else printf("server chiuso, questo comando non può essere effettuato\n");
                            continue;
                        }
                        if (strcmp(richiesta[0], "/a") == 0) {
                            for (i = 0; i < MAX_UTENTI; i++) {
                                if (message_socket[i] != 0) {
                                    inviaMessaggio(message_socket[i], len, lmsg, messaggio);
                                    memorizzaMessaggioInviato(message_socket[i], utente, mio_username, messaggio);
                                }
                            }
                            if (FD_ISSET(client_socket, &master)) aggiungiInChat(client_socket, richiesta[1], message_socket, utente, dev_port, mio_username, server_addr);
                            else printf("server chiuso, questo comando non può essere effettuato\n");
                            resettaRichiesta(richiesta);
                            continue;
                        }
                        if (strcmp(richiesta[0], "share") == 0) {
                            for (i = 0; i < MAX_UTENTI; i++) {
                                if (message_socket[i] != 0) {
                                    inviaMessaggio(message_socket[i], len, lmsg, messaggio);
                                    memorizzaMessaggioInviato(message_socket[i], utente, mio_username, messaggio);
                                }
                            }
                            share(richiesta[1], mio_username, message_socket);
                            continue;
                        }

                        for (i = 0; i < MAX_UTENTI; i++) {
                            if (message_socket[i] != 0) {
                                inviaMessaggio(message_socket[i], len, lmsg, messaggio);
                                memorizzaMessaggioInviato(message_socket[i], utente, mio_username, messaggio);
                            }
                        }
                        resettaRichiesta(richiesta);
                    }
                    else if (inChatServer) {                     /* invia messaggi al server */
                        fgets(buffer, 1024, stdin);
                        if (buffer[strlen(buffer) - 1] == '\n') {
                            buffer[strlen(buffer) - 1] = '\0';
                        }
                        strcpy(comando, buffer);
                        tockenizzaRichiesta(comando, richiesta);
                        if (strcmp(buffer, "/q") == 0) {
                            inChatServer = false;
                            homeUtente();
                            resettaRichiesta(richiesta);
                            continue;
                        }
                        current_timestamp = time(NULL);
                        strcpy(tempo, ctime(&current_timestamp));
                        if (tempo[strlen(tempo) - 1] == '\n') {
                            tempo[strlen(tempo) - 1] = '\0';
                        }
                        snprintf(messaggio, sizeof(messaggio), "chatServer %s [%s] %s : %s", dest_username, tempo, mio_username, buffer);
                        len = strlen(messaggio) + 1;
                        lmsg = htons(len);
                        inviaMessaggio(client_socket, len, lmsg, messaggio);
                        resettaRichiesta(richiesta);
                    }
                    else {                                       /* invia richieste al server */
                        fgets(buffer, 1024, stdin);
                        if (buffer[strlen(buffer) - 1] == '\n') {
                            buffer[strlen(buffer) - 1] = '\0';
                        }
                        if (strcmp(buffer, "\0") == 0) continue;    
                        strcpy(comando, buffer);
                        tockenizzaRichiesta(comando, richiesta);
                        riceviComando(richiesta, buffer, dev_port, mio_username, &controllo);
                        len = strlen(buffer) + 1;
                        lmsg = htons(len);

                        if (controllo == false) {
                            controllo = true;
                            resettaRichiesta(richiesta);
                            continue;
                        }

                        if (FD_ISSET(client_socket, &master)) inviaMessaggio(client_socket, len, lmsg, buffer);
                        else if (strcmp(richiesta[0], "out") == 0) successoLogout(mio_username, message_socket, utente);    // nel caso in cui il comando sia out, forzo l'uscita anche se il server è spento
                        else printf("server chiuso, questo comando non può essere effettuato\n");
                    }
                }
                else if (socket_pronto == peer_socket) {
                    len = sizeof(client_addr);
                    connected_socked = accept(peer_socket, (struct sockaddr*)&client_addr, &len);

                    FD_SET(connected_socked, &master);
                    if (connected_socked > fdmax) {
                        fdmax = connected_socked;
                    }
                }
                else if (socket_pronto == client_socket) {
                    bool socket_valido = isSocketValid(socket_pronto);
                    if(socket_valido == false) {
                        FD_CLR(socket_pronto, &master);
                        printf("chat attraverso il socket %d terminata in modo anomalo\n", socket_pronto);
                        continue;
                    }

                    riceviMessaggio(client_socket, len, lmsg, buffer);

                    if (strcmp(buffer, "successo login") == 0) successoLogin(mio_username, richiesta);
                    else if (strcmp(buffer, "successo logout") == 0) successoLogout(mio_username, message_socket, utente);
                    else if (strcmp(buffer, "online") == 0) chatOnline(richiesta, message_socket, client_socket, utente, server_addr, &inChat, mio_username);
                    else if (strcmp(buffer, "offline") == 0) chatOffline(richiesta, dest_username, &inChatServer, mio_username);
                    else if (strcmp(buffer, "successo hanging") == 0) successoHanging(client_socket);
                    else if (strcmp(buffer, "successo show") == 0) successoShow(client_socket);
                    else printf("%s\n", buffer);
                    resettaRichiesta(richiesta);
                }
                else {                                          /* gestione della comunicazione con gli altri peer */
                    bool socket_valido = isSocketValid(socket_pronto);
                    if(socket_valido == false) {
                        FD_CLR(socket_pronto, &master);
                        printf("chat attraverso il socket %d terminata in modo anomalo\n", socket_pronto);
                        continue;
                    }
                    riceviMessaggio(socket_pronto, len, lmsg, buffer);
                    strcpy(comando, buffer);
                    tockenizzaRichiesta(comando, richiesta);

                    if (strcmp(richiesta[1], "/q") == 0) {
                        socket_uscente = socketUscente(utente, richiesta);  
                        for (i = 0; i < MAX_UTENTI; i++) {              
                            if (message_socket[i] == socket_uscente) {
                                message_socket[i] = 0;
                            }
                        }

                        close(socket_pronto);
                        FD_CLR(socket_pronto, &master);
                        continue;
                    }
                    else if (strcmp(richiesta[1], "/a") == 0) {
                        if (FD_ISSET(client_socket, &master)) aggiungiInChat(client_socket, richiesta[2], message_socket, utente, dev_port, mio_username, server_addr);
                        else printf("server chiuso, questo comando non può essere effettuato\n");
                        continue;
                    }
                    else if (strcmp(richiesta[0], "aggiungiti") == 0) {
                        aggiunto(utente, &inChat, message_socket, server_addr, richiesta[1], richiesta[2]);
                        continue;
                    }
                    else if (strcmp(richiesta[1], "share") == 0) riceviFile(socket_pronto, mio_username);

                    memorizzaMessaggioRicevuto(richiesta, utente, mio_username, buffer);
                    printf("%s\n", buffer);
                    resettaRichiesta(richiesta);
                }
            }
        }
    }
    close(client_socket);
    close(peer_socket);
    return 0;
}

/***************************************/
/*          FUNZIONI GENERALI          */
/***************************************/

/*
    initDevice:
    - inizializza la struttura dati utente_t utente[MAX_UTENTI]
    - inizializza l'array dei socket che verranno usati per messaggiare con gli altri device
    - crea il proprio socket e lo connette al server
*/
int initDevice(struct sockaddr_in* server_addr, utente_t utente[MAX_UTENTI], int message_socket[MAX_UTENTI]) {
    int sd;
    initUtenti(utente);
    initMessageSocket(message_socket);
    sd = creaSocket();
    connectSocket(sd, SERVER_PORT, server_addr);
    homeDevice();
    return sd;
}

/*
    initPeer:
    - crea, aggancia e mette in ascolto il proprio socket per i peer
*/
int initPeer(struct sockaddr_in* peer_addr, int dev_port) {
    int sd;
    sd = creaSocket();
    bindSocket(sd, dev_port, peer_addr);
    listenSocket(sd, MAX_CODA_LISTEN);
    return sd;
}

void initUtenti(utente_t utente[MAX_UTENTI]) {
    int i;
    for (i = 0; i < MAX_UTENTI; i++) {
        strcpy(utente[i].username, "\0");
        utente[i].porta = 0;
        utente[i].socket = 0;
    }
    return;
}

void initMessageSocket(int message_socket[MAX_UTENTI]) {
    int i;
    for (i = 0; i < MAX_UTENTI; i++) {
        message_socket[i] = 0;
    }
    return;
}

int creaSocket() {
    int sd;
    sd = socket(AF_INET, SOCK_STREAM, 0);
    return sd;
}

void connectSocket(int client_socket, int porta, struct sockaddr_in* server_addr) {
    int ret;
    
    memset(server_addr, 0, sizeof(struct sockaddr_in));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(porta);
    inet_pton(AF_INET, "127.0.0.1", &server_addr->sin_addr);

    ret = connect(client_socket, (struct sockaddr*)server_addr, sizeof(struct sockaddr_in));
    if (ret == -1) {
        perror("Errore durante la connessione.\n");
        printf("Errore numero: %d\n", errno);
        exit(1);
    }
    return;
}

void bindSocket(int sd, int porta, struct sockaddr_in* my_addr) {
    int ret;

    memset(my_addr, 0, sizeof(struct sockaddr_in));
    my_addr->sin_family = AF_INET;
    my_addr->sin_port = htons(porta);
    inet_pton(AF_INET, "127.0.0.1", &my_addr->sin_addr);

    ret = bind(sd, (struct sockaddr*)my_addr, sizeof(struct sockaddr_in));
    if (ret == -1) {
        perror("Errore durante l'aggancio.\n");
        printf("Errore numero: %d\n", errno);
        exit(1);
    }
    return;
}

void listenSocket(int sd, int coda_listen) {
    int ret;
    ret = listen(sd, MAX_CODA_LISTEN);
    if (ret == -1) {
        perror("Errore durante l'ascolto.\n");
        printf("Errore numero: %d\n", errno);
        exit(1);
    }
    return;
}

/*
    isSocketValid:
    - restituisce 0 se il socket da cui si riceve il messaggio è stato chiuso
*/
bool isSocketValid(int sd) {
    int ret;
    char carattere_prova;
    ret = recv(sd, &carattere_prova, 1, MSG_PEEK);
    return ret; 
}

void homeDevice() {
    system("clear");
    printf("Menù iniziale:\n");
    printf("- signup [username] [password]\n");
    printf("- in [server_port] [username] [password]\n");
    printf("\n");
    return;
}

/*
    inviaMessaggio:
    - invia la dimensione del messaggio che si vuole inviare
    - invio il messaggio
*/
void inviaMessaggio(int sd, int len, uint16_t lmsg, char* messaggio) {
    int ret;

    ret = send(sd, (void*)&lmsg, sizeof(uint16_t), 0);
    if (ret == -1) {
        perror("Errore durante l'invio della dimensione.\n");
        printf("Errore numero: %d\n", errno);
        return;
    }
    
    ret = send(sd, (void*)messaggio, len, 0);
    if (ret == -1) {
        perror("Errore durante l'invio del messaggio.\n");
        printf("Errore numero: %d\n", errno);
        return;
    }
    return;
}

/*
    riceviMessaggio:
    - riceve la dimensione del messaggio che si deve ricevere
    - riceve il messaggio
*/
void riceviMessaggio(int sd, int len, uint16_t lmsg, char* messaggio) {
    int ret;

    ret = recv(sd, (void*)&lmsg, sizeof(uint16_t), 0);
    if (ret == -1) {
        perror("Errore durante la ricezione della dimensione del messaggio.\n");
        printf("Errore numero: %d\n", errno);
        return;
    }
    len = ntohs(lmsg);

    ret = recv(sd, (void*)messaggio, len, 0);
    if (ret == -1) {
        perror("Errore durante la ricezione del messaggio.\n");
        printf("Errore numero: %d\n", errno);
        return;
    }
    return;
}

/*
    tockenizzaRichiesta:
    - inserisce le parole del messaggio in un array
    - è usata per identificare la parola del messaggio che indica 
      il comando effettivo e i parametri da passare alle funzioni che lo soddisfano
    - è fondamentale in quanto per ogni comando c'è un formato diverso del messaggio
*/
void tockenizzaRichiesta(char* buffer, char** richiesta) {
    int numeroParole = 0;
    char* token;
    const char separatore[] = " ";

    token = strtok(buffer, separatore);
    while (token != NULL) {
        richiesta[numeroParole] = token;
        numeroParole++;
        token = strtok(NULL, separatore);
    }
    return;
}

void resettaRichiesta(char** richiesta) {
    int i = 0;
    while(richiesta[i] != NULL) {
        richiesta[i] = NULL;
        i++;
    }
    return;
}

/*
    riceviComando:
    riceve il comando scritto da tastiera e a seconda di quale è stato richiesto chiama una funzione che lo gestisce
*/
void riceviComando(char** richiesta, char* buffer, int dev_port, char mio_username[50], bool* controllo) {
    char* comando = richiesta[0];

    if (strcmp(comando, "signup") == 0) {
        if (controlloSignup(richiesta)) homeDevice();
        else {
            *controllo = false;
            printf("Qualcosa è andato storto. Verifica che il formato del comando sia corretto:\nsignup [username] [password]\n\n");
        }
        return;
    }
    if (strcmp(comando, "in") == 0) {
        if (controlloLogin(richiesta)) login(buffer, dev_port, mio_username);
        else {
            *controllo = false;
            printf("Qualcosa è andato storto. Verifica che il formato del comando sia corretto:\nin [serv_port] [username] [password]\n\n");
        }
        return;
    }
    if (strcmp(comando, "chat") == 0) {
        if (controlloChat(richiesta)) chat(buffer, mio_username);
        else {
            *controllo = false;
            printf("Qualcosa è andato storto. Verifica che il formato del comando sia corretto:\nchat [username]\n\n");
        }
        return;
    }
    if (strcmp(comando, "out") == 0) {
        if (controlloOut(richiesta)) out(buffer, dev_port, mio_username);
        else {
            *controllo = false;
            printf("Qualcosa è andato storto. Verifica che il formato del comando sia corretto:\nout\n\n");
            }
        return;
    }
    if (strcmp(comando, "hanging") == 0) {
        if (controlloHanging(richiesta)) hanging(mio_username, buffer);
        else {
            *controllo = false;
            printf("Qualcosa è andato storto. Verifica che il formato del comando sia corretto:\nhanging\n\n");
            }
        return;
    }
    if (strcmp(comando, "show") == 0) {
        if (controlloShow(richiesta)) show(mio_username, buffer);
        else {
            *controllo = false;
            printf("Qualcosa è andato storto. Verifica che il formato del comando sia corretto:\nshow [username]\n\n");
            }
        return;
    }
}

/*
    rimuoviParole:
    - trova la posizione della prima occorrenza delle parole da rimuovere
    - Calcola il numero di byte da spostare
    - Sposta i byte nel buffer in avanti
*/
void rimuoviParole(char messaggio[1024], char parole_da_eliminare[100]) {
    size_t lunghezza_parole = strlen(parole_da_eliminare);
    size_t lunghezza_messaggio = strlen(messaggio);

    char *posizione = strstr(messaggio, parole_da_eliminare);
    if (posizione != NULL) {
        size_t byte_da_muovere = lunghezza_messaggio - (posizione - messaggio) - lunghezza_parole + 1;
        memmove(posizione, posizione + lunghezza_parole, byte_da_muovere);
    }
    return;
}

/***************************************/
/*           COMANDI CLIENT            */
/***************************************/

void homeUtente() {
    system("clear");
    printf("Menù di ingresso:\n");
    printf("- hanging\n");
    printf("- show [username]\n");
    printf("- chat [username]\n");
    printf("- out\n");
    printf("\n");
    return;
}

/*
    COMANDO SIGNUP
    controlloSignup:
    - controlla che il fromato del messaggio sia corretto
*/
bool controlloSignup(char** richiesta) {
    if (richiesta[3] != NULL) return false;
    else return true;
}

/*
    COMANDO LOGIN
    login:
    - aggiunge al messaggio da inviare la propria porta e il proprio username
      per avere il formato del messaggio corretto
*/
void login(char* buffer, int dev_port, char mio_username[50]) {
    strcat(buffer, " "); 
    sprintf(buffer + strlen(buffer), "%d", dev_port);
    strcat(buffer, " ");
    strcat(buffer, mio_username);
    return;
}

/*
    controlloLogin:
    - controlla che il fromato del messaggio sia corretto
*/
bool controlloLogin(char** richiesta) {
    if (richiesta[4] != NULL) return false;
    else return true;
}

/*
    successoLogin:
    - riceve il messaggio con lo username e lo copia in mio_username
*/
void successoLogin(char mio_username[50], char** richiesta) {
    strcpy(mio_username, richiesta[2]);
    homeUtente();
    return;
}

/*
    COMANDO OUT
    controlloOut:
    - controlla che il fromato del messaggio sia corretto
*/
bool controlloOut(char** richiesta) {
    if (richiesta[1] != NULL) return false;
    else return true;
}

/*
    out:
    - aggiunge al messaggio da inviare la propria porta e il proprio username
      per avere il formato del messaggio corretto
*/
void out(char* buffer, int dev_port, char mio_username[50]) {
    strcat(buffer, " ");
    sprintf(buffer + strlen(buffer), "%d", dev_port);
    strcat(buffer, " ");
    strcat(buffer, mio_username);
    return;
}

/*
    successoLogout:
    - azzera l'array con il nome dell'utente che ha fatto logout
*/
void successoLogout(char mio_username[50], int message_socket[MAX_UTENTI], utente_t utente[MAX_UTENTI]) {
    int i;
    memset(mio_username, 0, sizeof(mio_username));
    for (i = 0; i < MAX_UTENTI; i++) {
        if (message_socket[i] != 0) close(message_socket[i]);
    }
    initMessageSocket(message_socket);
    initUtenti(utente);
    homeDevice();
    return;
}

/*
    COMANDO CHAT
    chat:
    - aggiunge al messaggio da inviare il proprio username per avere il formato del messaggio corretto
*/
void chat(char* buffer, char mio_username[50]) {
    strcat(buffer, " ");
    strcat(buffer, mio_username);
    return;
}

void schermoChat() {
    system("clear");
    printf("[IN CHAT]\n\n");
    return;
}

/*
    controlloChat:
    - controlla che il fromato del messaggio sia corretto
*/
bool controlloChat(char** richiesta) {
    if (richiesta[2] != NULL) return false;
    else return true;
}

/*
    memorizzaMessaggioInviato:
    - scorre la struttura degli utenti finchè non trova l'utente il cui socket è uguale a message_socket[i]
    - salva l'username
    - crea nella cartella mio_username un file chat_con_username (mio_username/chat_con_username)
    - appenda nel file il messaggio inviato
*/
void memorizzaMessaggioInviato(int dest_socket, utente_t utente[MAX_UTENTI], char mio_username[50], char* buffer) {
    int i;
    char username[50];
    char percorso_file[50];
    FILE* file;

    for (i = 0; i < MAX_UTENTI; i++) {
        if (utente[i].socket == dest_socket) {
            strcpy(username, utente[i].username);
        }
    }

    snprintf(percorso_file, sizeof(percorso_file), "%s/chat_con_%s.txt", mio_username, username);
    file = fopen(percorso_file, "a");
    fprintf(file, "%s\n", buffer);
    fclose(file);
    return;
}

/*
    memorizzaMessaggioRicevuto:
    - scorre la struttura degli utenti finchè non trova utente il cui socket è uguale a socket_pronto
    - salva l'username
    - crea nella cartella mio_username un file chat_con_username (mio_username/chat_con_username)
    - appende nel file il messaggio ricevuto
*/
void memorizzaMessaggioRicevuto(char** richiesta, utente_t utente[MAX_UTENTI], char mio_username[50], char* buffer) {
    int i;
    char username[50];
    char percorso_file[50];
    char parole_da_eliminare[50];
    FILE* file;

    strcpy(username, richiesta[0]);
    strcpy(parole_da_eliminare, "[");
    rimuoviParole(username, parole_da_eliminare);
    strcpy(parole_da_eliminare, "]:");
    rimuoviParole(username, parole_da_eliminare);

    snprintf(percorso_file, sizeof(percorso_file), "%s/chat_con_%s.txt", mio_username, username);
    file = fopen(percorso_file, "a");
    fprintf(file, "%s\n", buffer);
    fclose(file);
    return;
}

/*
    chatOnline:
    - recupera la cronologia della chat
    - riceve il messaggio con la porta dell'utente con cui si vuole chattare
    - richiede una connessione con il destinatario
    - salva nella struttura dati degli utenti username e porta dell'utente con cui si vuole chattare
*/
void chatOnline(char** richiesta, int message_socket[MAX_UTENTI], int client_socket, utente_t utente[MAX_UTENTI], struct sockaddr_in server_addr, bool* inChat, char mio_username[50]) {
    schermoChat();
    recuperaChat(mio_username, richiesta[1]);
    
    int len, i, j;
    uint16_t lmsg;
    char dest_port[20];

    riceviMessaggio(client_socket, len, lmsg, dest_port);
    for (i = 0; i < MAX_UTENTI; i++) {
        if (message_socket[i] == 0) {
            message_socket[i] = creaSocket();
            connectSocket(message_socket[i], atoi(dest_port), &server_addr);
            *inChat = true;
            break;
        }
    }

    for (j = 0; j < MAX_UTENTI; j++) {

        if (strcmp(utente[j].username, richiesta[1]) == 0) {   
            utente[j].socket = message_socket[i];
            break;
        }

        if (strcmp(utente[j].username, "\0") == 0) {
            strcpy(utente[j].username, richiesta[1]);
            utente[j].porta = atoi(dest_port);
            utente[j].socket = message_socket[i];
            break;
        }
    }
    return;
}

/*
    utentiOnline:
    - invia al server il messaggio di richiesta di sapere chi sono gli utenti online
*/
void utentiOnline(int client_socket, char buffer[1024]) {
    int len;
    uint16_t lmsg;
    
    len = strlen(buffer) + 1;
    lmsg = htons(len);
    inviaMessaggio(client_socket, len, lmsg, buffer);
    return;
}

/*
    aggiungiInChat:
    - connette il socket dell'utente che vuole chattare all'utente con cui si vuole chattare
        -> invia un messaggio al server con l'username dell'utente con cui si vuole chattare
        -> riceve un messaggio dal server con la porta dell'utente con cui si vuole chattare
        -> salva nella struttura dati degli utenti username e porta dell'utente con cui si vuole chattare
        -> effettua la connessione con l'utente con cui si vuole chattare
    - connette l'utente con cui si vuole chattare all'utente che ha fatto la richiesta di chat
        -> invia all'utente con cui si vuole chattare un ack "aggiunto"
        -> invio al'utente con cui si vuole chattare la porta dell'utente che ha fatto la richiesta
*/
void aggiungiInChat(int client_socket, char dest_username[50], int message_socket[MAX_UTENTI], utente_t utente[MAX_UTENTI], int dev_port, char mio_username[50], struct sockaddr_in server_addr) {
    int len, i, j;
    uint16_t lmsg;
    char buffer[1024];
    char messaggio[1024];
    char dest_port[20];
    char mia_porta[50];
    char ack[20];
    strcpy(ack, "aggiungiti");
    strcpy(buffer, "aggiungi ");
    strcat(buffer, dest_username);

    if (strcmp(dest_username, "\0") == 0) {
        printf("username dell'utente da aggiungere non inserito\n");
        return;
    }

    len = strlen(buffer) + 1;
    lmsg = htons(len);
    inviaMessaggio(client_socket, len, lmsg, buffer);
    riceviMessaggio(client_socket, len, lmsg, dest_port);

    for (i = 0; i < MAX_UTENTI; i++) {
        if (message_socket[i] == 0) {
            message_socket[i] = creaSocket();
            connectSocket(message_socket[i], atoi(dest_port), &server_addr);
            break;
        }
    }

    for (j = 0; j < MAX_UTENTI; j++) {     
        if (strcmp(utente[j].username, dest_username) == 0) {   
            utente[j].socket = message_socket[i];
            break;
        }

        if (strcmp(utente[j].username, "\0") == 0) {
            strcpy(utente[j].username, dest_username);
            utente[j].porta = atoi(dest_port);
            utente[j].socket = message_socket[i];
            break;
        }
    }

    sprintf(mia_porta, "%d", dev_port);
    strcpy(messaggio, ack);
    strcat(messaggio, " ");
    strcat(messaggio, mio_username);
    strcat(messaggio, " ");
    strcat(messaggio, mia_porta);
    len = strlen(messaggio) + 1;
    lmsg = htons(len);
    inviaMessaggio(message_socket[i], len, lmsg, messaggio);
    return;
}

/*
    aggiunto:
    - riceva la porta e lo username dell'utente con cui ci si deve connettere
    - salva nella struttura dati degli utenti username e porta
    - effettua la connessione con l'utente
*/
void aggiunto(utente_t utente[MAX_UTENTI], bool* inChat, int message_socket[MAX_UTENTI], struct sockaddr_in server_addr, char username_mittente[50], char porta_mittente[50]) {
    int len, i, j;
    uint16_t lmsg;

    for (i = 0; i < MAX_UTENTI; i++) {
        if (message_socket[i] == 0) {
            message_socket[i] = creaSocket();
            connectSocket(message_socket[i], atoi(porta_mittente), &server_addr);
            break;
        }
    }

    for (j = 0; j < MAX_UTENTI; j++) {
        if (strcmp(utente[j].username, username_mittente) == 0) {   
            utente[j].socket = message_socket[i];
            break;
        }

        if (strcmp(utente[j].username, "\0") == 0) {                
            strcpy(utente[j].username, username_mittente);
            utente[j].porta = atoi(porta_mittente);
            utente[j].socket = message_socket[i];
            break;
        }
    }

    *inChat = true;
    schermoChat();
    return;
}

/*
    chatOffline:
    - recupera la cronologia della chat
    - apre una chat con il server
*/
void chatOffline(char** richiesta, char dest_username[50], bool* inChatServer, char mio_username[50]) {
    strcpy(dest_username, richiesta[1]);
    schermoChat();
    recuperaChat(mio_username, richiesta[1]);

    *inChatServer = true;
    printf("[UTENTE %s OFFLINE]\n", richiesta[1]);
    return;
}

/*
    recuperaChat:
    - apre in lettura il file mio_username/chat_con_dest_username
    - stampa su terminale ogni riga del file
*/
void recuperaChat(char mio_username[50], char* dest_username) {
    char riga[MAX_LENGHT_RIGA];
    char percorso_file[50];
    FILE* file;

    snprintf(percorso_file, sizeof(percorso_file), "%s/chat_con_%s.txt", mio_username, dest_username);    
    file = fopen(percorso_file, "r");

    if (file != NULL) {
        while (fgets(riga, sizeof(riga), file) != NULL) {
            strcat(riga, "\0");
            printf("%s", riga);
        }
    }

    if (file != NULL) fclose(file);
    return;
}

/*
    socketUscente:
    - pulisce l'username dell'utente che esce dalla chat
    - trova l'utente nella struttura degli utenti
    - trova il socket dell'utente
    - restituisce il socket
*/
int socketUscente(utente_t utente[MAX_UTENTI], char** richiesta) {
    int i, socket_uscente;
    char username[50];
    char parole_da_eliminare[50];
    socket_uscente = -1;

    strcpy(username, richiesta[0]);
    strcpy(parole_da_eliminare, "[");
    rimuoviParole(username, parole_da_eliminare);
    strcpy(parole_da_eliminare, "]:");
    rimuoviParole(username, parole_da_eliminare);

    for (i = 0; i < MAX_UTENTI; i++) {
        if (strcmp(utente[i].username, username) == 0) {
            socket_uscente = utente[i].socket;
            utente[i].socket = 0;
            break;
        }
    }

    return socket_uscente;
}

/*
    COMANDO HANGING
    controlloHanging:
    - verifica che il formato del messaggio sia corretto
*/
bool controlloHanging(char** richiesta) {
    if (richiesta[1] != NULL) return false;
    else return true;
}

/*
    hanging:
    - aggiunge al messaggio da inviare il proprio username per avere il formato corretto
*/
void hanging(char mio_username[50], char buffer[1024]) {
    strcat(buffer, " ");
    strcat(buffer, mio_username);
    return;
}

/*
    successoHanging:
    - riceve il messaggio con i nomi degli utenti che avevano inviato messaggi mentre questo utente era offline dal server
    - stampa il messaggio sul terminale
*/
void successoHanging(int client_socket) {
    int len, i;
    char nomi_file[1024];
    uint16_t lmsg;

    riceviMessaggio(client_socket, len, lmsg, nomi_file);
    printf("%s\n", nomi_file);
    return;
}

/*
    COMANDO SHOW
    controlloShow:
    - verifica che il formato del messaggio sia corretto
*/
bool controlloShow(char** richiesta) {
    if (richiesta[1] == NULL || richiesta[2] != NULL) return false;
    else return true;
}

/*
    show:
    - aggiunge al messaggio da inviare il proprio username e il buffer per avere il formato corretto
*/
void show(char mio_username[50], char buffer[1024]) {
    strcat(buffer, " ");
    strcat(buffer, mio_username);
    return;
}

/*
    successoShow:
    - riceve dal server i messaggi ricevuti mentre si era offline da uno specifico utente
    - stampa sul terminale i messaggi
*/
void successoShow(int client_socket) {
    int len, i;
    char messaggi_ricevuti[1024];
    uint16_t lmsg;

    riceviMessaggio(client_socket, len, lmsg, messaggi_ricevuti);
    printf("%s\n", messaggi_ricevuti);
    return;
}

/*
    COMANDO SHARE
    share:
    - apra il file nome_file nella cartella mio_username (mio_username/nome_file)
    - copia in un buffer il contenuto del file aperto
    - invia il buffer al destinatario (tramite message_socket[i]). il buffer contiene come prima parola nome_file
*/
void share(char file_inviato[50], char mio_username[50], int message_socket[MAX_UTENTI]) {
    int len, i;
    uint16_t lmsg;
    char riga[MAX_LENGHT_RIGA];
    char nome_file[50];
    char buffer[1024];
    char percorso_file[100];
    FILE* file;

    snprintf(nome_file, sizeof(nome_file), "%s", file_inviato);
    snprintf(percorso_file, sizeof(percorso_file), "%s/%s.txt", mio_username, nome_file);
    file = fopen(percorso_file, "r");
    if (file == NULL) {
        printf("Il file selezionato non esiste\n");
        return;
    }

    snprintf(buffer, sizeof(buffer), "%s ", nome_file);
    while (fgets(riga, sizeof(riga), file) != NULL) {
        strcat(buffer, riga);
    }
    fclose(file);

    len = strlen(buffer) + 1;
    lmsg = htons(len);
    for (i = 0; message_socket[i] != 0; i++) {
        inviaMessaggio(message_socket[i], len, lmsg, buffer);
    }
    return;
}

/*
    riceviFile:
    - riceve il buffer contenete il contenuto del file
    - crea un file nome_file nella cartella mio_username (mio_username/nome_file)
    - elimina la parola nome_file dal buffer
    - copia il buffer nel file creato
*/
void riceviFile(int socket_pronto, char mio_username[50]) {
    int len, i;
    uint16_t lmsg;
    char parole_da_eliminare[100];
    char nome_file[50];
    char buffer[1024];
    char temp_buffer[1024];
    char* parole[1024];
    char percorso_file[100];
    FILE* file;

    riceviMessaggio(socket_pronto, len, lmsg, buffer);
    strcpy(temp_buffer, buffer);
    tockenizzaRichiesta(temp_buffer, parole);

    strcpy(nome_file, parole[0]);
    strcpy(parole_da_eliminare, nome_file);
    strcat(parole_da_eliminare, " ");
    rimuoviParole(buffer, parole_da_eliminare);

    snprintf(percorso_file, sizeof(percorso_file), "%s/%s.txt", mio_username, nome_file);
    file = fopen(percorso_file, "w");
    if (file == NULL) {
        perror("Errore nell'apertura del file");
        return;
    }

    fprintf(file, "%s", buffer);
    fclose(file);
    return;
}