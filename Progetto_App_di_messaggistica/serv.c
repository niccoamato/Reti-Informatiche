#include "all.h"

/***************************************/
/*           STRUTTURE DATI            */
/***************************************/

// Struttura dati degli utenti
typedef struct utente {
    char username[50];
    int porta;
    int socket;
    time_t timestamp_login;
    time_t timestamp_logout;
} utente_t;

// Comandi del server
void serverHelp();
void serverList(utente_t utente[MAX_UTENTI]);
void serverEsc(int server_socket, fd_set* master, int fdmax);

// Funzioni di inizializzazione del server
void initStampa();
void stampaHomeServer();
void fdSetInit(fd_set* master, fd_set* read_fds);
void initUtenti(utente_t utente[MAX_UTENTI]);
int creaSocket();
void bindSocket(int sd, int porta, struct sockaddr_in* my_addr);
void listenSocket(int sd, int coda_listen);
void connectSocket(int client_socket, int porta, struct sockaddr_in* server_addr);
bool isSocketValid(int sd);
int initServer(struct sockaddr_in* server_addr, fd_set* master, fd_set* read_fds, utente_t utente[MAX_UTENTI]);

// Funzioni generali
void riceviComando(int socket_pronto, utente_t utente[MAX_UTENTI], int server_socket, fd_set* master, int fdmax);
void erroreComando(int socket_pronto);
void inviaMessaggio(int sd, int len, uint16_t lmsg, char* messaggio);
void riceviMessaggio(int sd, int len, uint16_t lmsg, char* messaggio);
void riceviRichiesta(int sd, char* buffer);
void tockenizzaRichiesta(char buffer[1024], char** richiesta);
bool controllaRichiesta(char** richiesta, int socket_pronto, utente_t utente[MAX_UTENTI]);
void eseguiRichiesta(int socket_pronto, char** richiesta, utente_t utente[MAX_UTENTI], struct sockaddr_in peer_addr, fd_set* master, char buffer[1024]);
void resettaRichiesta(char** richiesta);
char* traduciTimestamp(time_t timestamp);
bool cercaFile(char* nome_cartella, const char* nome_file);
void rimuoviParole(char messaggio[1024], char parole_da_eliminare[100]);

// Funzioni per la gestione del signup
void signup(char** richiesta, int socket_pronto);
bool controlloSignup(char username[20], char password[20], int socket_pronto);
void salvaDatiUtente(char** richiesta);
void aggiungiUsername(char** richiesta);

// Funzioni per la gestione del login
bool controlloLogin(int serv_port, char username[20], char password[20], int dev_port, char mio_username[50], int socket_pronto, utente_t utente[MAX_UTENTI]);
void login(char username[20], char password[20], int dev_port, int socket_pronto, utente_t utente[MAX_UTENTI], struct sockaddr_in peer_addr);

// Funzione per la gestione del logout
void out(int dev_port, utente_t utente[MAX_UTENTI], int socket_utente, char mio_username[50]);

// Funzioni per la gestione della chat
bool controlloChat(char username[20], char mio_username[50], int socket_pronto, char** richiesta);
bool controllaRubrica(char percorso_file[50], char username[50], int socket_pronto);
void chat(char dest_username[20], utente_t utente[MAX_UTENTI], int socket_pronto);
void chatServer(char dest_username[50], char mittente_username[50], char messaggio[1024]);
void memorizzaMessaggio(char mittente_username[50], char dest_username[50], char messaggio[1024]);
void utentiOnline(int socket_pronto, utente_t utente[MAX_UTENTI]);
void aggiungiUtente(int socket_pronto, utente_t utente[MAX_UTENTI], char buffer[1024]);

// Funzioni per la gestione dell'hanging
void hanging(int socket_pronto, char mio_username[50]);
int contaFile(char nome_cartella[50], char nomi_file[1024]);
int contaMessaggiPendenti(char nome_file[100], char nome_cartella[50], char timestamp_ultimo_messaggio[100]);

// Funzioni per la gestione della show
bool controllaShow(char nome_file[100], char nome_cartella[50], int socket_pronto);
void show(char dest_username[50], char mio_username[50], int socket_pronto, utente_t utente[MAX_UTENTI]);

/***************************************/
/*                MAIN                 */
/***************************************/

int main(int argc, char** argv) {
    int sd, new_sd, socket_pronto, fdmax;
	socklen_t len;
    char buffer[1024];
    char comando[1024];
    char* richiesta[1024];
    struct sockaddr_in client_addr;
    struct sockaddr_in server_addr;
    struct sockaddr_in peer_addr;
    utente_t utente[MAX_UTENTI];

    fd_set master;
    fd_set read_fds;

    sd = initServer(&server_addr, &master, &read_fds, utente);

    FD_SET(sd, &master);
    fdmax = sd;
    
    while(1) {
        read_fds = master;
        select(fdmax + 1, &read_fds, NULL, NULL, NULL);

        for (socket_pronto = 0; socket_pronto <= fdmax; socket_pronto++) {
            if (FD_ISSET(socket_pronto, &read_fds)) {
                if (socket_pronto == 0) {
                    riceviComando(socket_pronto, utente, sd, &master, fdmax);
                    stampaHomeServer();
                }
                else if (socket_pronto == sd) {
                    len = sizeof(client_addr);
                    new_sd = accept(sd, (struct sockaddr*)&client_addr, &len);
                    printf("Connessione accettata.\n");
                    printf("Socket di connessione: %d\n", new_sd);
                    printf("\n");

                    FD_SET(new_sd, &master);
                    if (new_sd > fdmax) {
                        fdmax = new_sd;
                    }
                }
                else {
                    bool socket_valido = isSocketValid(socket_pronto);
                    if(socket_valido == false) {
                        FD_CLR(socket_pronto, &master);
                        printf("socket eliminato: %d\n", socket_pronto);
                        continue;
                    }
                    riceviRichiesta(socket_pronto, buffer);
                    strcpy(comando, buffer);
                    tockenizzaRichiesta(comando, richiesta);
                    if (controllaRichiesta(richiesta, socket_pronto, utente) == false) {
                        resettaRichiesta(richiesta);
                        continue;
                    }
                    eseguiRichiesta(socket_pronto, richiesta, utente, peer_addr, &master, buffer);
                    resettaRichiesta(richiesta);
                }
            }
        }
    }
    close(sd);
    return 0;
}

/***************************************/
/*          FUNZIONI GENERALI          */
/***************************************/

/*
    initServer:
    - inizializza i set dei socket
    - inizializza la struttura dati utente_t utente[MAX_UTENTI]
    - crea, aggancia e mette in ascolto il proprio socket
*/
int initServer(struct sockaddr_in* server_addr, fd_set* master, fd_set* read_fds, utente_t utente[MAX_UTENTI]) {
    int sd;
    fdSetInit(master, read_fds);
    initUtenti(utente);
    initStampa();
    stampaHomeServer();
    sd = creaSocket();
    bindSocket(sd, SERVER_PORT, server_addr);
    listenSocket(sd, MAX_CODA_LISTEN);
    return sd;
}

void fdSetInit(fd_set* master, fd_set* read_fds) {
    FD_ZERO(master);
    FD_ZERO(read_fds);
    FD_SET(0, master);
    return;
}

void initUtenti(utente_t utente[MAX_UTENTI]) {
    int i;
    for (i = 0; i < MAX_UTENTI; i++) {
        strcpy(utente[i].username, "\0");
        utente[i].porta = 0;
        utente[i].timestamp_login = 0;
        utente[i].timestamp_logout = 0;
    }
    return;
}

void initStampa() {
    system("clear");
    printf("***************************** SERVER STARTED *********************************\n");
    return;
}

void stampaHomeServer() {
    printf("Digita comando:\n"
            "\n"
            "1) help --> mostra i dettagli dei comandi\n"
            "2) list --> mostra un elenco degli utenti connessi\n"
            "3) esc  --> chiude il server\n"
            "\n");
    return;
}

int creaSocket() {
    int sd;
    sd = socket(AF_INET, SOCK_STREAM, 0);
    return sd;
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

/*
    riceviComando:
    riceve il comando scritto da tastiera e a seconda di quale è stato richiesto chiama una funzione che lo gestisce
*/
void riceviComando(int socket_pronto, utente_t utente[MAX_UTENTI], int server_socket, fd_set* master, int fdmax) {
    char buffer[1024];
    scanf("%s", buffer);
    if (strcmp(buffer, "help") == 0) {
        serverHelp();
        return;
    }
    if (strcmp(buffer, "list") == 0) {
        serverList(utente);
        return;
    }
    if (strcmp(buffer, "esc") == 0) {
        serverEsc(server_socket, master, fdmax);
        return;
    }
    else {
        erroreComando(socket_pronto);
        return;
    }
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
    riceviRichiesta:
    - riceve il messaggio contenente la richiesta da soddisfare
    - è usata per ricevere dai device i messaggi
*/
void riceviRichiesta(int sd, char* buffer) {
    int len;
    uint16_t lmsg;
    riceviMessaggio(sd, len, lmsg, buffer);
    return;
}

/*
    tockenizzaRichiesta:
    - inserisce le parole del messaggio in un array
    - è usata per identificare la parola del messaggio che indica 
      il comando effettivo e i parametri da passare alle funzioni che lo soddisfano
    - è fondamentale in quanto per ogni comando c'è un formato diverso del messaggio
*/
void tockenizzaRichiesta(char buffer[1024], char** richiesta) {
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

/*
    controllaRichiesta:
    - a seconda di quale sia prima parola del messaggio inviato dall'utente, riconosce la richiesta da soddisfare
      e controlla se può essere effettivamente soddisfatta (ogni richiesta ha condizioni diverse)
    - è fondamentale aver precedentemente tockenizzato il messaggio
*/
bool controllaRichiesta(char** richiesta, int socket_pronto, utente_t utente[MAX_UTENTI]) {
    char* comando = richiesta[0];
    int serv_port, dev_port;

    if (strcmp(comando, "signup") == 0) {
        return controlloSignup(richiesta[1], richiesta[2], socket_pronto);
    }
    else if (strcmp(comando, "in") == 0) {
        if (strcmp(richiesta[1], "4242") != 0) serv_port = 0;   
        else serv_port = SERVER_PORT;
        if (richiesta[4] != NULL) dev_port = atoi(richiesta[4]);
        else dev_port = 0;
        return controlloLogin(serv_port, richiesta[2], richiesta[3], dev_port, richiesta[5], socket_pronto, utente);
    }
    else if (strcmp(comando, "out") == 0) {
        if (richiesta[2] == NULL) return false;
        else return true;
    }
    else if (strcmp(comando, "show") == 0) {
        return controllaShow(richiesta[1], richiesta[2], socket_pronto);
    }
    else if (strcmp(comando, "chat") == 0) {
        return controlloChat(richiesta[1], richiesta[2], socket_pronto, richiesta);
    }
    else if (strcmp(comando, "/u") == 0) {
        return true;
    }
    else if (strcmp(comando, "chatServer") == 0) {
        return true;
    }
    else if (strcmp(comando, "aggiungi") == 0) {
        return true;
    }
    else if (strcmp(comando, "hanging") == 0) {
        return true;
    }
    else {
        erroreComando(socket_pronto);
        return false;
    }
}

/*
    eseguiRichiesta:
    - a seconda di quale sia prima parola del messaggio inviato dall'utente, riconosce la richiesta da soddisfare,
      questa funzione è chiamata dopo aver controllato che la richiesta possa essere effettivamente soddisfatta
    - è fondamentale aver precedentemente tockenizzato il messaggio
    - è fondamentale aver precedentemente invocato la controllaRichiesta()
*/
void eseguiRichiesta(int socket_pronto, char** richiesta, utente_t utente[MAX_UTENTI], struct sockaddr_in peer_addr, fd_set* master, char buffer[1024]) {
    char* comando = richiesta[0];
    int dev_port;

    if (strcmp(comando, "signup") == 0) {
        signup(richiesta, socket_pronto);
    }
    else if (strcmp(comando, "in") == 0) {
        dev_port = atoi(richiesta[4]);
        login(richiesta[2], richiesta[3], dev_port, socket_pronto, utente, peer_addr);
    }
    else if (strcmp(comando, "out") == 0) {
        dev_port = atoi(richiesta[1]);
        out(dev_port, utente, socket_pronto, richiesta[2]);
    }
    else if (strcmp(comando, "show") == 0) {
        show(richiesta[1], richiesta[2], socket_pronto, utente);
    }
    else if (strcmp(comando, "chat") == 0) {
        chat(richiesta[1], utente, socket_pronto);
    }
    else if (strcmp(comando, "/u") == 0) {
        utentiOnline(socket_pronto, utente);
    }
    else if (strcmp(comando, "chatServer") == 0) {
        chatServer(richiesta[1], richiesta[7], buffer);
    }
    else if (strcmp(comando, "aggiungi") == 0) {
        aggiungiUtente(socket_pronto, utente, buffer);
    }
    else if (strcmp(comando, "hanging") == 0) {
        hanging(socket_pronto, richiesta[1]);
    }
}

/*
    resettaRichiesta:
    mette a NULL ogni elemento della variabile richiesta così che non ci sia overflow con le richieste successive
*/
void resettaRichiesta(char** richiesta) {
    int i = 0;
    while(richiesta[i] != NULL) {
        richiesta[i] = NULL;
        i++;
    }
    return;
}

/*
    erroreComando:
    notifica che c'è stato un errore nel richiedere un determinato comando
*/
void erroreComando(int socket_pronto) {
    if (socket_pronto == 0) {
        printf("\n");
        printf("Qualcosa è andato storto...");
        printf("Verifica che il comando selezionato sia corretto, o che i parametri inviati siano corretti.\n");
        printf("\n");
    }
    else {
        int len;
        uint16_t lmsg;
        char* messaggio = "\nQualcosa è andato storto...\nVerifica che il comando selezionato sia corretto, o che i parametri inviati siano corretti.\n\n";

        len = strlen(messaggio) + 1;
        lmsg = htons(len);
        inviaMessaggio(socket_pronto, len, lmsg, messaggio);
    }
    return;
}

/*
    traduciTimestamp:
    traduce il timestamp dal tipo time_t al tipo char*
*/
char* traduciTimestamp(time_t timestamp) {
    char* orario;
    orario = ctime(&timestamp);
    if (orario[strlen(orario) - 1] == '\n') {
        orario[strlen(orario) - 1] = '\0';
    }
    return orario;
}

/*
    cercaFile:
    - cerca nella directory "nome_cartella" il file "nome_file"
    - restituisce true se lo trova, false altrimenti
*/
bool cercaFile(char* nome_cartella, const char* nome_file) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(nome_cartella);
    if (dir == NULL) {
        perror("Errore nell'apertura della directory");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, nome_file) == 0) {
            closedir(dir);
            return true;
        }
    }
    closedir(dir);
    return false;
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
/*           COMANDI SERVER            */
/***************************************/

/*
    COMANDO: HELP
    serverHelp:
    stampa sul terminale una breve descrizione per ciascun comando utilizzabile
*/
void serverHelp() {
    printf("\n"
           "Comandi dal Server:\n"
           "-) help --> mostra i dettagli dei comandi.\n"
           "-) list --> mostra un elenco degli utenti connessi.\n"
           "-) esc  --> chiude il server.\n"
           "\n"
           "Comandi dagli utenti:\n"
           "-) signup  --> crea un account sul server, caratterizzato da username e password.\n"
           "-) in      --> login di un utente, va specificata la porta del server, username e password.\n"
           "-) hanging --> mostra un elenco degli utenti che hanno inviato dei messaggi pendenti.\n"
           "-) show    --> mostra i messaggi pendenti inviato da un utente specificato.\n"
           "-) chat    --> avvia una chat con un utente specificato\n"
           "-) share   --> invia un file al device su cui è connesso l'utente.\n"
           "-) out     --> l'utente si disconnette\n"
           "\n");
    return;
}

/*
    COMANDO: LIST
    serverList:
    stampa sul terminale gli utenti online nel formato: "username * timestamp * porta"
*/
void serverList(utente_t utente[MAX_UTENTI]) {
    int i;
    char timestamp[50];

    printf("Utenti online:\n");
    for (i = 0; i < MAX_UTENTI; i++) {
        if (strcmp(utente[i].username, "\0") != 0 && utente[i].timestamp_logout == 0) {
            strcpy(timestamp, traduciTimestamp(utente[i].timestamp_login));
            printf("-) %s * %s * %d\n", utente[i].username, timestamp, utente[i].porta);
        }
    }
    printf("\n");
    return;
}

/*
    COMANDO: ESC
    serverEsc:
    - toglie tutti i socket dalla lista master e li chiude
    - chiude il proprio socket
*/
void serverEsc(int server_socket, fd_set* master, int fdmax) {
    int i;

    for (i = 1; i <= fdmax; i++) {
        if (FD_ISSET(i, master)) {
            FD_CLR(i, master);
            close(i);
        }
    }

    printf("Server in spegnimento\n");
    close(server_socket);
    exit(0);
    return;
}

/***************************************/
/*           COMANDI CLIENT            */
/***************************************/

/*
    COMANDO SIGNUP
    controlloSignup:
    - verifica che il messaggio inviato dall'utente abbia il formato corretto
    - verifica se l'username che il nuovo utente vuole utilizzare non sia già in uso
*/ 
bool controlloSignup(char username[20], char password[20], int socket_pronto) {
    int len, i;
    uint16_t lmsg;
    char riga[MAX_LENGHT_RIGA];
    char* messaggio;
    FILE* file = fopen("users.txt", "r");

    if (username == NULL || password == NULL) {
        messaggio = "Errore nel signup. Verifica di aver inviato il messaggio nel formato corretto:\nsignup [username] [password]\n\n";
        len = strlen(messaggio) + 1;
        lmsg = htons(len);
        inviaMessaggio(socket_pronto, len, lmsg, messaggio);
        if (file != NULL) {
            fclose(file);
        }
        return false;
    }

    if (file != NULL) {
        while (fgets(riga, sizeof(riga), file) != NULL) {
            riga[strcspn(riga, "\n")] = '\0';
            if (strcmp(riga, username) == 0) {
                messaggio = "Username già in uso\n\n";
                len = strlen(messaggio) + 1;
                lmsg = htons(len);
                inviaMessaggio(socket_pronto, len, lmsg, messaggio);
                fclose(file);
                return false;
            }
        }
    }

    if (file != NULL) fclose(file);
    return true;
}

/*
    signup:
    - aggiunge lo username del nuovo utente iscritto nel file "users.txt"
    - salva i dati del nuovo utente nel file "utenti_iscritti.txt"
    - invia all'utente un ack di avvenuta registrazione
*/
void signup(char** richiesta, int socket_pronto) {
    int len;
    uint16_t lmsg;
    char* ack = "Registrazione avvenuta con successo.\n\n";

    aggiungiUsername(richiesta);
    salvaDatiUtente(richiesta);
    len = strlen(ack) + 1;
    lmsg = htons(len);
    inviaMessaggio(socket_pronto, len, lmsg, ack);
    return;
}

/*
    salvaDatiUtente:
    - salva nel file "utenti_iscritti.txt" username e password del nuovo utente iscritto
    - crea la cartella dell'utente (conterrà i file.txt con le chat con i vari utenti o altri file)
    - crea, all'interno della cartella dell'utente, la cartella "messaggi_pendenti" che conterrà 
      i file.txt con i messaggi inviati dagli altri utenti mentre questo era offline
*/
void salvaDatiUtente(char** richiesta) {
    int ret;
    char* nome_cartella = richiesta[1];
    FILE* file = fopen("utenti_iscritti.txt", "a");
    if (file == NULL) {
        perror("Errore nell'apertura del file per il salvataggio degli utenti.\n");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "Username: %s Password: %s\n", richiesta[1], richiesta[2]);
    fclose(file);

    ret = mkdir(nome_cartella);
    strcat(nome_cartella, "/messaggi_pendenti");
    ret = mkdir(nome_cartella);
    return;
}

/*
    aggiungiUsername:
    - aggiunge lo username del nuovo utente iscritto nel file "users.txt"
*/
void aggiungiUsername(char** richiesta) {
    FILE* file = fopen("users.txt", "a");
    if (file == NULL) {
        perror("Errore nell'apertura del file per il salvataggio degli utenti.\n");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "%s\n", richiesta[1]);
    fclose(file);
    return;
}

/*
    COMANDO LOGIN
    controlloLogin:
    - controlla che l'utente che richiede il comando non sia già online
    - verifica che il messaggio inviato dall'utente abbia il formato corretto
    - verifica che la porta del server specificata sia corretta
    - verifica che l'username specificato dall'utente esista e che la password specificata coincida con quella salvata
    - verifica che l'utente che sta richiedendo il login non faccia richiesta per conto di un utente che è già online
*/
bool controlloLogin(int serv_port, char username[20], char password[20], int dev_port, char mio_username[50], int socket_pronto, utente_t utente[MAX_UTENTI]) {    
    int len, i;
    uint16_t lmsg;
    char riga[MAX_LENGHT_RIGA];
    char riga_temp[20];
    char* messaggio;
    char* rigaTockenizzata[20];
    FILE* file = fopen("utenti_iscritti.txt", "r");

    if (mio_username != NULL) {
        for (i = 0; i < MAX_UTENTI; i++) {
            if (strcmp(utente[i].username, mio_username) == 0 && utente[i].timestamp_logout == 0) {
                messaggio = "Errore nel login:\nnon puoi eseguire un login se sei già online\n\n";
                len = strlen(messaggio) + 1;
                lmsg = htons(len);
                inviaMessaggio(socket_pronto, len, lmsg, messaggio);
                if (file != NULL) fclose(file);
                return false;
                } 
        }
    }

    if (username == NULL || password == NULL || dev_port == 0) {
        messaggio = "Errore nel login:\nverifica di aver inviato il messaggio nel formato corretto:\nin [serv_port] [username] [password]\n\n";
        len = strlen(messaggio) + 1;
        lmsg = htons(len);
        inviaMessaggio(socket_pronto, len, lmsg, messaggio);
        if (file != NULL) fclose(file);
        return false;
    }

    if (serv_port != SERVER_PORT) {
        messaggio = "Errore nel login:\nla porta del server è sbagliata.\n";
        len = strlen(messaggio) + 1;
        lmsg = htons(len);
        inviaMessaggio(socket_pronto, len, lmsg, messaggio);
        if (file != NULL) fclose(file);
        return false;
    }

    if (file == NULL) {
        messaggio = "Errore nel login:\nl'utente non esiste.\n";
        len = strlen(messaggio) + 1;
        lmsg = htons(len);
        inviaMessaggio(socket_pronto, len, lmsg, messaggio);
        return false;
    }

    if (file != NULL) {
        while (fgets(riga, sizeof(riga), file) != NULL) {
            riga[strcspn(riga, "\n")] = '\0';
            strcpy(riga_temp, riga);
            tockenizzaRichiesta(riga_temp, rigaTockenizzata);
            if (strcmp(rigaTockenizzata[1], username) == 0 && strcmp(rigaTockenizzata[3], password) != 0) {
                messaggio = "Errore nel login:\nla password non coincide.\n";
                len = strlen(messaggio) + 1;
                lmsg = htons(len);
                inviaMessaggio(socket_pronto, len, lmsg, messaggio);
                fclose(file);
                return false;
            }
            else if (strcmp(rigaTockenizzata[1], username) == 0 && strcmp(rigaTockenizzata[3], password) == 0) {
                break;
            }
        }
        if (strcmp(rigaTockenizzata[1], username) != 0) {
            messaggio = "Errore nel login:\nl'utente non esiste.\n";
            len = strlen(messaggio) + 1;
            lmsg = htons(len);
            inviaMessaggio(socket_pronto, len, lmsg, messaggio);
            fclose(file);
            return false;
        }
    }
    
    for(i = 0; i < MAX_UTENTI; i++) {
        if (strcmp(utente[i].username, username) == 0 && utente[i].timestamp_logout == 0) {
            messaggio = "Errore nel login:\nl'utente è già online.\n";
            len = strlen(messaggio) + 1;
            lmsg = htons(len);
            inviaMessaggio(socket_pronto, len, lmsg, messaggio);
            fclose(file);
            return false;
        }
    }

    fclose(file);
    return true;
}

/*
    login:
    - se è la prima volta che l'utente fa login inizializza la struttura in una nuova entry altrimenti aggiorna 
      l'entry dell'utente azzerando il timestamp del logout e aggiornando il login
    - invia un ack di login avvenuto all'utente
    - controlla se nella cartella nome_utente è presente un file "messaggi_inviati"
      se esiste invia le notifiche all'utente ed elimina il file
*/
void login(char username[20], char password[20], int dev_port, int socket_pronto, utente_t utente[MAX_UTENTI], struct sockaddr_in peer_addr) {
    int i, socket_connesso, len, lmsg;
    const char* nome_file = "messaggi_inviati.txt";
    char percorso_file[50];
    char riga[MAX_LENGHT_RIGA];
    char* ack_login = "successo login";
    FILE* file;
    time_t tempo_corrente = time(NULL);

    for(i = 0; i < MAX_UTENTI; i++) {
        if (strcmp(utente[i].username, "\0") == 0) {
            strcpy(utente[i].username, username);
            utente[i].porta = dev_port;
            utente[i].timestamp_login = tempo_corrente;
            utente[i].timestamp_logout = 0;
            utente[i].socket = socket_pronto;
            break;
        }
        else if (strcmp(utente[i].username, username) == 0) {
            utente[i].timestamp_login = tempo_corrente;
            utente[i].timestamp_logout = 0;
            utente[i].porta = dev_port;
            utente[i].socket = socket_pronto;
            break;
        }
    }

    len = strlen(ack_login) + 1;
    lmsg = htons(len);
    inviaMessaggio(socket_pronto, len, lmsg, ack_login);

    if (cercaFile(username, nome_file)) {
        snprintf(percorso_file, sizeof(percorso_file), "%s/%s", username, nome_file);
        file = fopen(percorso_file, "r+");
        if (file == NULL) {
            perror("Errore nell'apertura del file");
            return;
        }

        while (fgets(riga, sizeof(riga), file) != NULL) {
            strcat(riga, "\0");
            len = strlen(riga) + 1;
            lmsg = htons(len);
            inviaMessaggio(socket_pronto, len, lmsg, riga);
        }
        fclose(file);

        if (remove(percorso_file) == 0) {
            printf("Il file %s è stato eliminato con successo.\n", nome_file);
        }
        else {
            perror("Errore nell'eliminazione del file");
        }
    }
    return;
}

/*
    COMANDO OUT
    out:
    - verifica che il comando sia stato chiamato da un utente online
    - azzera i dati dell'utente precedentemente salvati nella struttura dati
    - invia un ack di avvenuto logout
*/
void out(int dev_port, utente_t utente[MAX_UTENTI], int socket_utente, char mio_username[50]) {
    int i, len, lmsg;
    char* ack_logout = "successo logout";
    time_t tempo_corrente = time(NULL);

    if (mio_username == NULL) {
        erroreComando(socket_utente);
        return;
    }

    for (i = 0; i < MAX_UTENTI; i++) {
        if (utente[i].porta == dev_port) {
            utente[i].porta = 0;
            utente[i].socket = 0;
            utente[i].timestamp_login = 0;
            utente[i].timestamp_logout = tempo_corrente;
        }
    }

    len = strlen(ack_logout) + 1;
    lmsg = htons(len);
    inviaMessaggio(socket_utente, len, lmsg, ack_logout);
    return;
}

/*
    COMANDO HANGING
    hanging:
    - controlla che l'utente che richiede il servizio sia online
    - conta quanti file ci sono nella cartella mio_username/messaggi_pendenti e ne scrive i nomi in un buffer
    - invia l'ack di successo dell'hanging all'utente che ha richiesto il servizio
    - invia il messaggio con i nomi dei file all'utente che ha richiesto il servizio
*/
void hanging(int socket_pronto, char mio_username[50]) {
    int i, len, numero_file;
    uint16_t lmsg;
    char nomi_file[1024];
    char nome_cartella[50];
    char* ack = "successo hanging";
    char messaggio[1024];

    nomi_file[0] = '\0';

    if (mio_username == NULL) {
        erroreComando(socket_pronto);
        return;
    }

    strcpy(nome_cartella, mio_username);
    strcat(nome_cartella, "/messaggi_pendenti");
    numero_file = contaFile(nome_cartella, nomi_file);

    len = strlen(ack) + 1;
    lmsg = htons(len);
    inviaMessaggio(socket_pronto, len, lmsg, ack);

    snprintf(messaggio, sizeof(messaggio), "%d utenti ti hanno inviato messaggi mentre eri offline:\n", numero_file);
    strcat(messaggio, nomi_file);
    len = strlen(messaggio) + 1;
    lmsg = htons(len);
    inviaMessaggio(socket_pronto, len, lmsg, messaggio);
    return;
}

/*
    contaFile:
    - conta quanti file ci sono nella cartella nome_cartella
    - memorizza i nomi dei file in un buffer
*/
int contaFile(char nome_cartella[50], char nomi_file[1024]) {
    DIR *dir;
    struct dirent *entry;
    char* posizione_punto;
    char nome_file[100];
    char timestamp_ultimo_messaggio[100];
    char n_mex[100];
    int numero_file = 0;
    int numero_messaggi = 0;

    dir = opendir(nome_cartella);
    if (dir == NULL) {
        perror("Errore durante l'apertura della cartella");
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {

        if (entry->d_type == DT_REG) {                  // Controlla se è un file regolare
            strcpy(nome_file, entry->d_name);
            posizione_punto = strchr(nome_file, '.');   // Trova la posizione del punto nell'estensione
            if (posizione_punto != NULL) {
                *posizione_punto = '\0';                // Termina la stringa al punto per rimuovere l'estensione
            }

            numero_messaggi = contaMessaggiPendenti(entry->d_name, nome_cartella, timestamp_ultimo_messaggio);
            sprintf(n_mex, "%d", numero_messaggi);

            strcat(nomi_file, "-) ");
            strcat(nomi_file, nome_file);
            strcat(nomi_file, " num_messaggi: ");
            strcat(nomi_file, n_mex);
            strcat(nomi_file, "; ultimo_messaggio: ");
            strcat(nomi_file, timestamp_ultimo_messaggio);
            strcat(nomi_file, "\n");
            numero_file++;
        }
    }
    closedir(dir);
    return numero_file;
}

/*
    contaMessaggiPendenti:
    - conta il numero di righe presenti nel file nome_cartella/nome_file
    - memorizza il timestamp dell'ultima riga (ovvero il timestamp dell'ultimo messaggio pendente ricevuto)
*/
int contaMessaggiPendenti(char nome_file[100], char nome_cartella[50], char timestamp_ultimo_messaggio[100]) {
    int numero_righe = 0;
    char riga[MAX_LENGHT_RIGA];
    char percorso_file[100];
    snprintf(percorso_file, sizeof(percorso_file), "%s/%s", nome_cartella, nome_file);

    FILE* file = fopen(percorso_file, "r");
    if (file == NULL) {
        printf("Impossibile aprire il file %s\n", percorso_file);
        return 1;
    }

    while (fgets(riga, sizeof(riga), file) != NULL) {
        strncpy(timestamp_ultimo_messaggio, riga, 26);
        numero_righe++;
    }
    fclose(file);
    return numero_righe;
}

/*
    COMANDO SHOW
    controllaShow:
    - controlla che l'utente che invia la richiesta sia effettivamente online
    - controlla se l'utente di cui si vogliono visualizzare i messaggi ricevuti esista davvero
    - controlla che l'utente selezionato abbia effettivamente inviato dei messaggi (verifica l'esistenza 
      del file nome_file.txt)
    - invia un ack di successo di show all'utente che ne ha fatto richiesta
*/
bool controllaShow(char nome_file[100], char nome_cartella[50], int socket_pronto) {
    int len;
    uint16_t lmsg;
    char riga[MAX_LENGHT_RIGA];
    char messaggio[1024];
    char percorso_file[100];
    char* ack = "successo show";
    bool esiste = false;
    snprintf(percorso_file, sizeof(percorso_file), "%s/messaggi_pendenti/%s.txt", nome_cartella, nome_file);

    if (nome_cartella == NULL) {
        erroreComando(socket_pronto);
        return false;
    }
    
    FILE* file_1 = fopen("users.txt", "r");
    if (file_1 == NULL) {
        printf("Impossibile aprire il file %s\n", percorso_file);
        return 0;
    }

    while (fgets(riga, sizeof(riga), file_1) != NULL) {
        riga[strcspn(riga, "\n")] = '\0';    
        if (strcmp(nome_file, riga) == 0) {
            esiste = true;
            break;
        }
    }
    fclose(file_1);

    if (esiste == false) {
        strcpy(messaggio, "Errore:\nl'utente non esiste.\n");
        len = strlen(messaggio) + 1;
        lmsg = htons(len);
        inviaMessaggio(socket_pronto, len, lmsg, messaggio);
        return false;
    }
    
    FILE* file_2 = fopen(percorso_file, "r");
    if (file_2 == NULL) {
        strcpy(messaggio, "L'utente selezionato non ha inviato messaggi");
        len = strlen(messaggio) + 1;
        lmsg = htons(len);
        inviaMessaggio(socket_pronto, len, lmsg, messaggio);
        return false;
    }

    len = strlen(ack) + 1;
    lmsg = htons(len);
    inviaMessaggio(socket_pronto, len, lmsg, ack);
    fclose(file_2);
    return true;
}

/*
    show:
    - apre il file mio_username/messaggi_pendenti/dest_username
    - invia ogni riga all'utente che ne ha fatto richiesta
    - elimina il file appena letto
    - invia un messaggio di ack di messaggi inviato all'utente che li aveva inviati in precedenza se questo è online,
      se non è online crea nella sua cartella un file messaggi_inviati dove memorizza la notifica da mandargli dopo
      che ha fatto il login
*/
void show(char dest_username[50], char mio_username[50], int socket_pronto, utente_t utente[MAX_UTENTI]) {
    int len, socket_mittente, i;
    uint16_t lmsg;
    char riga[MAX_LENGHT_RIGA];
    char messaggio[1024];
    char ack[100];
    char percorso_file[100];
    messaggio[0] = '\0';
    snprintf(ack, sizeof(ack), "[server]: %s ha ricevuto i messaggi\n", mio_username);
    snprintf(percorso_file, sizeof(percorso_file), "%s/messaggi_pendenti/%s.txt", mio_username, dest_username);
    
    FILE* file = fopen(percorso_file, "r+");

    while (fgets(riga, sizeof(riga), file) != NULL) {   
        strcat(messaggio, riga);
    }
    strcat(messaggio, "\0");
    len = strlen(messaggio) + 1;
    lmsg = htons(len);
    inviaMessaggio(socket_pronto, len, lmsg, messaggio);
    fclose(file);

    if (remove(percorso_file) == 0) {
        printf("Il file %s è stato eliminato con successo.\n", percorso_file);
    }
    else {
        perror("Errore nell'eliminazione del file\n");
    }

    for (i = 0; i < MAX_UTENTI; i++) {
        if (strcmp(utente[i].username, dest_username) == 0 && utente[i].timestamp_logout == 0) {
            socket_mittente = utente[i].socket;
            len = strlen(ack) + 1;
            lmsg = htons(len);
            inviaMessaggio(socket_mittente, len, lmsg, ack);
            return;
        }
    }
    snprintf(percorso_file, sizeof(percorso_file), "%s/messaggi_inviati.txt", dest_username);
    file = fopen(percorso_file, "a");
    fprintf(file, "%s", ack);
    fclose(file);
    return;
}

/*
    COMANDO CHAT
    controlloChat:
    - controlla che l'utente che ha fatto la richiesta di chat sia effettivamente online
    - verifica che il formato del messaggio di richiesta sia corretto
    - verifica che l'utente con cui si vuole chattare sia effetteivamente memorizzato nella propria rubrica
    - verifica che l'utente con cui si vuole chattare esista effettivamente
*/
bool controlloChat(char username[20], char mio_username[50], int socket_pronto, char** richiesta) {
    int len, i;
    uint16_t lmsg;
    char riga[MAX_LENGHT_RIGA];
    char* messaggio;
    char percorso_file_2[50];
    FILE* file_1 = fopen("users.txt", "r");
    snprintf(percorso_file_2, sizeof(percorso_file_2), "rubrica/%s.txt", mio_username);

    if (mio_username == NULL) {
        erroreComando(socket_pronto);
        return false;
    }

    if (richiesta[1] == NULL || richiesta[2] == NULL || richiesta[3] != NULL) {
        messaggio = "Errore nel comando chat:\nverifica di aver inviato il messaggio nel formato corretto:\nchat [username]\n\n";
        len = strlen(messaggio) + 1;
        lmsg = htons(len);
        inviaMessaggio(socket_pronto, len, lmsg, messaggio);
        if (file_1 != NULL) fclose(file_1);
        return false;
    }

    if (controllaRubrica(percorso_file_2, username, socket_pronto) == false) return false;

    while (fgets(riga, sizeof(riga), file_1) != NULL) {
        riga[strcspn(riga, "\n")] = '\0';
        if (strcmp(riga, username) == 0) {
            fclose(file_1);
            return true;
            }
    }

    messaggio = "Errore nel richiedere la chat:\nl'utente con cui si vuole chattare non esiste.\n";
    len = strlen(messaggio) + 1;
    lmsg = htons(len);
    inviaMessaggio(socket_pronto, len, lmsg, messaggio);
    fclose(file_1);
    return false;
}

/*
    controllaRubrica:
    - verifica se l'utente che vuole chattare abbia effettivamente dei contatti in rubrica
    - verifica che l'utente che vuole chattare abbia effettivamente il contatto richiesto in rubrica
*/
bool controllaRubrica(char percorso_file[50], char username[50], int socket_pronto) {
    FILE* file;
    int len;
    uint16_t lmsg;
    char riga[MAX_LENGHT_RIGA];
    char* messaggio;

    file = fopen(percorso_file, "r");
    if (file == NULL) {
        messaggio = "Non hai ancora nessun utente in rubrica\n";
        len = strlen(messaggio) + 1;
        lmsg = htons(len);
        inviaMessaggio(socket_pronto, len, lmsg, messaggio);
        return false;
    }

    while (fgets(riga, sizeof(riga), file) != NULL) {
        riga[strcspn(riga, "\n")] = '\0';
        if (strcmp(riga, username) == 0) {
            fclose(file);
            return true;
        }
    }

    messaggio = "L'utente richiesto non è in rubrica\n";
    len = strlen(messaggio) + 1;
    lmsg = htons(len);
    inviaMessaggio(socket_pronto, len, lmsg, messaggio);
    fclose(file);
    return false;
}

/*
    chat:
    - verifica se l'utente con cui si vuole chattare è online o meno
    - se è online invia un ack di utente online a chi ha richiesto di parire la chat, insieme alla porta del destinatario
    - se non è online invia un ack di utente offline a chi ha richiesto di parire la chat e apre una chat con il server
*/
void chat(char dest_username[20], utente_t utente[MAX_UTENTI], int socket_pronto) {
    int i, len;
    char dest_port[20];
    uint16_t lmsg;
    char* ack_online = "online";
    char* ack_offline = "offline";
    bool online = false;

    // controlla se il destinatario è online
    for (i = 0; i < MAX_UTENTI; i++) {
        if (strcmp(utente[i].username, dest_username) == 0 && utente[i].timestamp_logout == 0) {
            sprintf(dest_port, "%d", utente[i].porta);
            online = true;
        }
    }

    if (online) {
        len = strlen(ack_online) + 1;
        lmsg = htons(len);
        inviaMessaggio(socket_pronto, len, lmsg, ack_online);

        len = strlen(dest_port) + 1;
        lmsg = htons(len);
        inviaMessaggio(socket_pronto, len, lmsg, dest_port);
    }
    else {
        len = strlen(ack_offline) + 1;
        lmsg = htons(len);
        inviaMessaggio(socket_pronto, len, lmsg, ack_offline);
    }
    return;
}

/*
    chatServer:
    - crea o apre il file dest_usernaem/messaggi_pendenti/mittente_username.txt
    - elimina le parole del formato del messaggio che non devono essere visualizzate
    - aggiunge il messaggio al file dei messaggi pendenti
    - memorizza il messaggio nella chat
*/
void chatServer(char dest_username[50], char mittente_username[50], char messaggio[1024]) {
    char parole_da_eliminare[100];
    char percorso_file[50];
    FILE* file;
    parole_da_eliminare[0] = '\0';

    snprintf(percorso_file, sizeof(percorso_file), "%s/messaggi_pendenti/%s.txt", dest_username, mittente_username);
    file = fopen(percorso_file, "a");
    if (file == NULL) {
        perror("Errore nell'apertura del file");
        return;
    }

    strcat(messaggio, "\n");
    strcat(parole_da_eliminare, "chatServer ");
    strcat(parole_da_eliminare, dest_username);
    strcat(parole_da_eliminare, " ");
    rimuoviParole(messaggio, parole_da_eliminare);

    fprintf(file, "%s", messaggio);
    fclose(file);

    memorizzaMessaggio(mittente_username, dest_username, messaggio);
    return;
}

/*
    memorizzaMessaggio:
    - apre il file mittente_username/chat_con_dest_username
    - appende il messaggio
    - apre il file dest_username/chat_con_mittente_username
    - appende il messaggio
*/
void memorizzaMessaggio(char mittente_username[50], char dest_username[50], char messaggio[1024]) {
    char percorso_file_1[50];
    char percorso_file_2[50];
    FILE* file_1;
    FILE* file_2;

    snprintf(percorso_file_1, sizeof(percorso_file_1), "%s/chat_con_%s.txt", mittente_username, dest_username);
    snprintf(percorso_file_2, sizeof(percorso_file_2), "%s/chat_con_%s.txt", dest_username, mittente_username);
    
    file_1 = fopen(percorso_file_1, "a");
    file_2 = fopen(percorso_file_2, "a");

    fprintf(file_1, "%s", messaggio);
    fprintf(file_2, "%s", messaggio);

    fclose(file_1);
    fclose(file_2);
    return;
}

/*
    utentiOnline:
    - scrive in un buffer gli username degli utenti online (timestamp.logout == 0)
    - invia il messaggio all'utente che ha fatto la richiesta
*/
void utentiOnline(int socket_pronto, utente_t utente[MAX_UTENTI]) {
    int i, len;
    uint16_t lmsg;
    char buffer[1024];
    buffer[0] = '\0';

    for (i = 0; i < MAX_UTENTI; i++) {
        if (strcmp(utente[i].username, "\0") != 0 && utente[i].timestamp_logout == 0) {
            strcat(buffer, utente[i].username);
            strcat(buffer, "\n");
        }
    }

    len = strlen(buffer) + 1;
    lmsg = htons(len);
    inviaMessaggio(socket_pronto, len, lmsg, buffer);
    return;
}

/*
    aggiungiUtente:
    - riceve un messaggio "aggiungi dest_username"
    - trova la porta dell'utente da aggiungere nella struttura dati
    - invia a socket_pronto la porta dest_username
*/
void aggiungiUtente(int socket_pronto, utente_t utente[MAX_UTENTI], char buffer[1024]) {
    int i, len, dest_port;
    uint16_t lmsg;
    char dest_username[50];
    char temp_buffer[50];
    char* parole[10];
    char messaggio_porta[50];

    strcpy(temp_buffer, buffer);
    tockenizzaRichiesta(temp_buffer, parole);
    strcpy(dest_username, parole[1]);

    for (i = 0; i < MAX_UTENTI; i++) {
        if (strcmp(utente[i].username, dest_username) == 0) {
            dest_port = utente[i].porta;
            break;
        }
    }

    sprintf(messaggio_porta, "%d", dest_port);
    len = strlen(messaggio_porta) + 1;
    lmsg = htons(len);
    inviaMessaggio(socket_pronto, len, lmsg, messaggio_porta);
    return;
}