// ===========================================
// SERVIDOR DE CHAT EN C - SOCKET + MULTITHREAD
// ===========================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTES 100
#define MAX_NOMBRE 50
#define MAX_MENSAJE 1024

struct Cliente {
    int socket;
    char nombre[MAX_NOMBRE];
    char estado[20];
    struct sockaddr_in direccion;
};

struct Cliente *clientes[MAX_CLIENTES];
pthread_mutex_t mutex_clientes = PTHREAD_MUTEX_INITIALIZER;

void broadcast(char *mensaje, int remitente_socket) {
    pthread_mutex_lock(&mutex_clientes);
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (clientes[i] && clientes[i]->socket != remitente_socket) {
            send(clientes[i]->socket, mensaje, strlen(mensaje), 0);
        }
    }
    pthread_mutex_unlock(&mutex_clientes);
}

void *manejar_cliente(void *arg) {
    struct Cliente *cliente = (struct Cliente *)arg;
    char buffer[MAX_MENSAJE];
    char mensaje_formateado[MAX_MENSAJE + MAX_NOMBRE];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_recibidos = recv(cliente->socket, buffer, sizeof(buffer), 0);
        if (bytes_recibidos <= 0) break;

        if (strncmp(buffer, "/status", 7) == 0) {
            char nuevo_estado[20];
            sscanf(buffer, "/status %s", nuevo_estado);
            strcpy(cliente->estado, nuevo_estado);
            snprintf(mensaje_formateado, sizeof(mensaje_formateado), "[INFO] %s cambió su estado a %s\n", cliente->nombre, cliente->estado);
            printf("%s", mensaje_formateado);
            broadcast(mensaje_formateado, cliente->socket);
        } else if (strncmp(buffer, "/exit", 5) == 0) {
            snprintf(mensaje_formateado, sizeof(mensaje_formateado), "[INFO] %s se ha desconectado.\n", cliente->nombre);
            printf("%s", mensaje_formateado);
            broadcast(mensaje_formateado, cliente->socket);
            break;
        } else if (strncmp(buffer, "/list", 5) == 0) {
            char lista[MAX_MENSAJE] = "[INFO] Usuarios conectados:\n";
            
            pthread_mutex_lock(&mutex_clientes);
            for (int i = 0; i < MAX_CLIENTES; i++) {
                if (clientes[i]) {
                    char usuario_info[100];
                    snprintf(usuario_info, sizeof(usuario_info), "- %s (%s)\n", clientes[i]->nombre, clientes[i]->estado);
                    strcat(lista, usuario_info);
                }
            }
            pthread_mutex_unlock(&mutex_clientes);

            send(cliente->socket, lista, strlen(lista), 0);
        } else if (strncmp(buffer, "/msg", 4) == 0) {
            char destinatario[MAX_NOMBRE], mensaje[MAX_MENSAJE];
            sscanf(buffer, "/msg %s %[^\n]", destinatario, mensaje);

            int encontrado = 0;
            pthread_mutex_lock(&mutex_clientes);
            for (int i = 0; i < MAX_CLIENTES; i++) {
                if (clientes[i] && strcmp(clientes[i]->nombre, destinatario) == 0) {
                    char mensaje_privado[MAX_MENSAJE + MAX_NOMBRE];
                    snprintf(mensaje_privado, sizeof(mensaje_privado), "[PRIVADO] %s: %s\n", cliente->nombre, mensaje);
                    send(clientes[i]->socket, mensaje_privado, strlen(mensaje_privado), 0);
                    
                    char confirmacion[MAX_MENSAJE];
                    snprintf(confirmacion, sizeof(confirmacion), "[INFO] Mensaje enviado a %s.\n", destinatario);
                    send(cliente->socket, confirmacion, strlen(confirmacion), 0);
                    
                    encontrado = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex_clientes);

            if (!encontrado) {
                char respuesta[MAX_MENSAJE];
                snprintf(respuesta, sizeof(respuesta), "[ERROR] El usuario %s no está conectado. No se pudo enviar el mensaje.\n", destinatario);
                send(cliente->socket, respuesta, strlen(respuesta), 0);
            }
        } else {
            snprintf(mensaje_formateado, sizeof(mensaje_formateado), "%s: %s\n", cliente->nombre, buffer);
            printf("%s", mensaje_formateado);
            broadcast(mensaje_formateado, cliente->socket);
        }
    }

    pthread_mutex_lock(&mutex_clientes);
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (clientes[i] == cliente) {
            clientes[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_clientes);
    close(cliente->socket);
    free(cliente);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <PUERTO>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int puerto = atoi(argv[1]);
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servidor_addr;

    servidor_addr.sin_family = AF_INET;
    servidor_addr.sin_port = htons(puerto);
    servidor_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_socket, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr));
    listen(server_socket, 10);

    printf("Servidor iniciado en puerto %d...\n", puerto);

    while (1) {
        struct sockaddr_in cliente_addr;
        socklen_t cliente_len = sizeof(cliente_addr);
        int cliente_socket = accept(server_socket, (struct sockaddr *)&cliente_addr, &cliente_len);

        struct Cliente *nuevo_cliente = (struct Cliente *)malloc(sizeof(struct Cliente));
        nuevo_cliente->socket = cliente_socket;
        nuevo_cliente->direccion = cliente_addr;
        strcpy(nuevo_cliente->estado, "ACTIVO");

        send(cliente_socket, "Ingresa tu nombre: ", 21, 0);
        recv(cliente_socket, nuevo_cliente->nombre, MAX_NOMBRE, 0);

        pthread_mutex_lock(&mutex_clientes);
        for (int i = 0; i < MAX_CLIENTES; i++) {
            if (clientes[i] == NULL) {
                clientes[i] = nuevo_cliente;
                break;
            }
        }
        pthread_mutex_unlock(&mutex_clientes);

        char bienvenida[100];
        snprintf(bienvenida, sizeof(bienvenida), "[INFO] %s se ha unido al chat.\n", nuevo_cliente->nombre);
        printf("%s", bienvenida);
        broadcast(bienvenida, nuevo_cliente->socket);

        pthread_t hilo;
        pthread_create(&hilo, NULL, manejar_cliente, (void *)nuevo_cliente);
    }
    return 0;
}
