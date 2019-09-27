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

/*
Lista de Clientes usando Nós
ClientNode:
data -> socket
prev -> aponta para o nó anterior
link -> aponta para o próximo nó
ip   -> string que armazena o IP
name -> nome do cliente (o apelido)
*/
typedef struct ClientNode {
    int data;
    struct ClientNode* prev;
    struct ClientNode* link;
    char ip[16];
    char name[31];
} ClientList;

/*
Cria um nó
*/
ClientList *newNode(int socket, char* ip) {
    ClientList *client = (ClientList *)malloc( sizeof(ClientList) );
    client->data = socket;
    client->prev = NULL;
    client->link = NULL;
    strncpy(client->ip, ip, 16);
    strncpy(client->name, "NULL", 5);
    return client;
}

// Global variables
int server = 0;
int client = 0;
int port;
ClientList *root, *now;

//Encerra todas as conexoes quando o usuário digitar "Ctrl+C"
void catch_ctrl_c_and_exit(int sig) {
    ClientList *tmp;
    while (root != NULL) {
        printf("\nSocket fechado: %d\n", root->data);
        close(root->data);
        tmp = root;
        root = root->link;
        free(tmp);
    }
    exit(EXIT_SUCCESS);
}

//Enviar as mensagens para todos os usuários, exceto o próprio que a criou
void sendAll(ClientList *node, char tmp_buffer[]) {
    //printf("sendALL\n");
    //Envia para todos os integrantes da sala, considerando que root representa o próprio server
    ClientList *tmp = root->link;
    while (tmp != NULL) {
        //printf("Datas: %d == %d = %d\n",node->data,tmp->data,(node->data != tmp->data));
        if (node->data != tmp->data) {
            printf("Mensagem enviada para %d: %s\n", tmp->data, tmp_buffer);
            send(tmp->data, tmp_buffer, LENGTH_SEND, 0);
        }
        tmp = tmp->link;
    }
}

//Executa quando um cliente se conecta
void onConnected(void *p_client) {
    int  output = 0;

    char nickname[LENGTH_NAME] = {};
    char read[LENGTH_MSG] = {};
    char send[LENGTH_SEND] = {};

    ClientList *client = (ClientList *)p_client;

    //Se não possui entrada ou se o tamanho do apelido for menor que dois ou maior que 30
    if (recv(client->data, nickname, LENGTH_NAME, 0) <= 0 || strlen(nickname) < 2 || strlen(nickname) >= LENGTH_NAME-1) {
        printf("%s nao tem nome de entrada.\n", client->ip);
        output = 1;
    } else {
        strncpy(client->name, nickname, LENGTH_NAME);
        printf("%s com IP: %s e ID: %d se conectou.\n", client->name, client->ip, client->data);
        sprintf(send, "%s entrou no bate-papo.", client->name);
        sendAll(client, send);
    }

    // Fica sempre a espera de que algo seja recebido
    while (1) {
        if (output) {
            break;
        }
        int receive = recv(client->data, read, LENGTH_MSG, 0);
        if (receive > 0) {
            //Se não tiver nada na variavel read vai para o próximo loop
            if (strlen(read) == 0) {
                continue;
            }
            sprintf(send, "Mensagem de %s %s: %s", client->ip,client->name, read);
        } else if (receive == 0 || strcmp(read, "sair") == 0) { //Se a conexão tiver perdida ou o usuário ter pedido para sair
            printf("%s com IP: %s e ID: %d se desconectou.\n", client->name, client->ip, client->data);
            sprintf(send, "%s saiu da sala.", client->name);
            output = 1;
        } else {
            printf("Erro: -1\n");
            output = 1;
        }
        sendAll(client, send);
    }

    //Quando um cliente se desconectar, libera o espaço que ele está armazenando na lista
    close(client->data);
    if (client == now) { // Se ele for o ultimo da lista, o ultimo será o anterior
        now = client->prev;
        now->link = NULL;
    } else {
        client->prev->link = client->link;
        client->link->prev = client->prev;
    }
    free(client);
}

int main(int argc, char **argv)
{
    port = atoi(argv[1]);
    //Para o evento de Ctrl+c
    signal(SIGINT, catch_ctrl_c_and_exit);

    // Create o servidor
    server = socket(AF_INET , SOCK_STREAM , 0);
    if (server == -1) {
        printf("Falha para criar um socket.");
        exit(EXIT_FAILURE);
    }

    // Seta as informações de sockets
    struct sockaddr_in server_info;
    struct sockaddr_in client_info;
    int server_sizeAddr = sizeof(server_info);
    int client_sizeAddr = sizeof(client_info);

    //Inicializa server_info e client_info com tudo nulo
    memset(&server_info, 0, server_sizeAddr);
    memset(&client_info, 0, client_sizeAddr);

    server_info.sin_family = PF_INET;
    server_info.sin_addr.s_addr = INADDR_ANY;
    server_info.sin_port = htons(port);

    //Associa o socket à porta e deixa o servidor em escuta
    bind(server, (struct sockaddr *)&server_info, server_sizeAddr);
    //Habilita o socket para que receba as conexões e a quantidade de conexões que podem ficar em aberto
    listen(server, 15);

    // Obtem o nome do socket
    getsockname(server, (struct sockaddr*) &server_info, (socklen_t*) &server_sizeAddr);
    printf("Executando servidor de chat na porta %d\n", ntohs(server_info.sin_port));
    printf("Aguardando novos clientes.\n");

    // Cria o primeiro nó de conexão, chamando-o de root, e o último (now) é o primeiro
    root = newNode(server, inet_ntoa(server_info.sin_addr));
    now = root;

    while (1) {
        client = accept(server, (struct sockaddr*) &client_info, (socklen_t*) &client_sizeAddr);

        // Pega as informações do cliente e imprime o IP
        getpeername(client, (struct sockaddr*) &client_info, (socklen_t*) &client_sizeAddr);
        printf("Novo cliente com IP %s se conectou.\n", inet_ntoa(client_info.sin_addr));

        // Faz um Append de cliente
        ClientList *c = newNode(client, inet_ntoa(client_info.sin_addr));
        c->prev = now;
        now->link = c;
        now = c;

        pthread_t id;
        if (pthread_create(&id, NULL, (void *)onConnected, (void *)c) != 0) {
            perror("Falha na conexao!\n");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
