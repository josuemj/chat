// ===========================================
// CLIENTE DE CHAT EN C - CON MENÚ INTERACTIVO
// ===========================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_MENSAJE 1024
char nombre_usuario[50];
int sockfd;

void *recibir_mensajes(void *arg) {
    char buffer[MAX_MENSAJE];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int len = recv(sockfd, buffer, sizeof(buffer), 0);
        if (len > 0) {
            printf("\n%s\n> ", buffer);
            fflush(stdout);
        }
    }
    return NULL;
}

void mostrar_menu() {
    printf("\n---- MENÚ ----\n");
    printf("1. Enviar mensaje al chat general\n");
    printf("2. Cambiar estado (/status ACTIVO/OCUPADO/INACTIVO)\n");
    printf("3. Salir del chat (/exit)\n");
    printf("Seleccione una opción: ");
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: %s <nombre_usuario> <IP_servidor> <puerto_servidor>\n", argv[0]);
        return 1;
    }

    strcpy(nombre_usuario, argv[1]);
    char *ip_servidor = argv[2];
    int puerto = atoi(argv[3]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(puerto);
    inet_pton(AF_INET, ip_servidor, &server_addr.sin_addr);

    connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    char tmp[100]; recv(sockfd, tmp, sizeof(tmp), 0);
    send(sockfd, nombre_usuario, strlen(nombre_usuario), 0);

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, recibir_mensajes, NULL);

    while (1) {
        mostrar_menu();
        int opcion;
        scanf("%d", &opcion);
        getchar();

        char mensaje[MAX_MENSAJE];
        if (opcion == 1) {
            printf("Mensaje: ");
            fgets(mensaje, MAX_MENSAJE, stdin);
            mensaje[strcspn(mensaje, "\n")] = 0;
            send(sockfd, mensaje, strlen(mensaje), 0);
        } else if (opcion == 2) {
            printf("Nuevo estado (ACTIVO/OCUPADO/INACTIVO): ");
            fgets(mensaje, MAX_MENSAJE, stdin);
            mensaje[strcspn(mensaje, "\n")] = 0;
            char comando[MAX_MENSAJE];
            snprintf(comando, sizeof(comando), "/status %s", mensaje);
            send(sockfd, comando, strlen(comando), 0);
        } else if (opcion == 3) {
            send(sockfd, "/exit", strlen("/exit"), 0);
            printf("Desconectado.\n");
            close(sockfd);
            break;
        } else {
            printf("Opción inválida.\n");
        }
    }
    return 0;
}
