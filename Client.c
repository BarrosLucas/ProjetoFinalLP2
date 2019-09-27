#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LENGTH_NAME 31
#define LENGTH_MSG  101
#define LENGTH_SEND 201
#define LENGTH_IP   16

// Global variables
volatile sig_atomic_t flag = 0;
int sock = 0;
int port;
char ip[LENGTH_IP];
char nickname[LENGTH_NAME] = {};


//Retira a quebra de linha
void withoutNewLine (char* str, int size) {
    int i;
    for (i = 0; i < size; i++) { // trim \n
        if (str[i] == '\n') {
            str[i] = '\0';
            break;
        }
    }
}

//Gera o "console"
void console() {
    printf("\r%s", "> ");
    fflush(stdout);
}

//É acionado quando o usuário clica "Ctrl+C"
void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

//É acionado quando a conexão é estabelecida e o usuário fica ativo na escuta
void onListenner() {
    char message[LENGTH_SEND] = {};
    while (1) {
        int receive = recv(sock, message, LENGTH_SEND, 0);
        //Se houver algo recebido
        if (receive > 0) {
            printf("\r%s\n", message);
            console();
        } else if (receive == 0) {
            break;
        } else {

        }
    }
}

void sendListenner() {
    char message[LENGTH_MSG] = {};
    while (1) {
        console();
        //Espera até que haja algo no buffer para ser lido
        while (fgets(message, LENGTH_MSG, stdin) != NULL) {
            withoutNewLine(message, LENGTH_MSG);
            if (strlen(message) == 0) {
                console();
            } else {
                break;
            }
        }
        send(sock, message, LENGTH_MSG, 0);
        if (strcmp(message, "sair") == 0) {
            break;
        }
    }
    catch_ctrl_c_and_exit(2);
}

int main(int argc, char **argv)
{
    signal(SIGINT, catch_ctrl_c_and_exit);
    strcpy(nickname,argv[3]);
    strcpy(ip,argv[1]);
    port = atoi(argv[2]);

    // Naming
    /*printf("Informe seu apelido: ");
    if (fgets(nickname, LENGTH_NAME, stdin) != NULL) {
        withoutNewLine(nickname, LENGTH_NAME);
    }
    if (strlen(nickname) < 2 || strlen(nickname) >= LENGTH_NAME-1) {
        printf("\nSeu nome deve ter mais que uma letra e menos que 30 letras.\n");
        exit(EXIT_FAILURE);
    }*/

    // Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1) {
        printf("Falha na criacao do socket.");
        exit(EXIT_FAILURE);
    }

    // Socket information
    struct sockaddr_in server_info, client_info;
    int server_sizeAddr = sizeof(server_info);
    int client_sizeAddr = sizeof(client_info);
    memset(&server_info, 0, server_sizeAddr);
    memset(&client_info, 0, client_sizeAddr);
    server_info.sin_family = PF_INET;
    server_info.sin_addr.s_addr = inet_addr(ip);
    server_info.sin_port = htons(port);

    // Connect to Server
    int err = connect(sock, (struct sockaddr *)&server_info, server_sizeAddr);
    if (err == -1) {
        printf("Falha na conexao com o servidor!\n");
        exit(EXIT_FAILURE);
    }

    // Names
    getsockname(sock, (struct sockaddr*) &client_info, (socklen_t*) &client_sizeAddr);
    getpeername(sock, (struct sockaddr*) &server_info, (socklen_t*) &server_sizeAddr);
    printf("Conectado ao servidor com endereco IP %s na porta %d\n", inet_ntoa(server_info.sin_addr), ntohs(server_info.sin_port));

    send(sock, nickname, LENGTH_NAME, 0);

    pthread_t send_thread;
    if (pthread_create(&send_thread, NULL, (void *) sendListenner, NULL) != 0) {
        printf ("Falha na escuta!\n");
        exit(EXIT_FAILURE);
    }

    pthread_t read_thread;
    if (pthread_create(&read_thread, NULL, (void *) onListenner, NULL) != 0) {
        printf ("Falha na escuta!\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        if(flag) {
            break;
        }
    }

    close(sock);
    return 0;
}
