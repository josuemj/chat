// ===========================================
// CLIENTE DE CHAT EN C - PROTOCOLO JSON
// ===========================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cjson/cJSON.h>

#define MAX_NOMBRE 50
#define MAX_MENSAJE 1024

char nombre_usuario[MAX_NOMBRE];
int sockfd;

void *recibir_mensajes(void *arg) {
    char buffer[MAX_MENSAJE];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int len = recv(sockfd, buffer, sizeof(buffer), 0);
        if (len <= 0) {
            printf("[INFO] Conexión cerrada por el servidor.\n");
            close(sockfd);
            exit(1);
        }

        printf("\n[DEBUG] Mensaje recibido: %s\n", buffer);

        cJSON *json = cJSON_Parse(buffer);
        if (!json) {
            printf("[ERROR] Error al parsear JSON.\n");
            continue;
        }

        cJSON *accion_json = cJSON_GetObjectItem(json, "accion");
        if (!accion_json || !cJSON_IsString(accion_json)) {
            cJSON *mensaje = cJSON_GetObjectItem(json, "mensaje");
            if (mensaje && cJSON_IsString(mensaje)) {
                printf("[INFO] %s\n", mensaje->valuestring);
            } else {
                printf("[ERROR] JSON recibido sin 'accion' ni 'mensaje'.\n");
            }
            cJSON_Delete(json);
            continue;
        }

        const char *accion = accion_json->valuestring;

        if (strcmp(accion, "BROADCAST") == 0) {
            cJSON *nombre_emisor = cJSON_GetObjectItem(json, "nombre_emisor");
            cJSON *mensaje = cJSON_GetObjectItem(json, "mensaje");

            if (nombre_emisor && mensaje && cJSON_IsString(nombre_emisor) && cJSON_IsString(mensaje)) {
                printf("[CHAT] %s: %s\n", nombre_emisor->valuestring, mensaje->valuestring);
            }
        } 
        else if (strcmp(accion, "DM") == 0) {
            cJSON *nombre_emisor = cJSON_GetObjectItem(json, "nombre_emisor");
            cJSON *mensaje = cJSON_GetObjectItem(json, "mensaje");

            if (nombre_emisor && mensaje && cJSON_IsString(nombre_emisor) && cJSON_IsString(mensaje)) {
                printf("[PRIVADO] %s: %s\n", nombre_emisor->valuestring, mensaje->valuestring);
            }
        } 
        else if (strcmp(accion, "LISTA") == 0) {
            cJSON *usuarios = cJSON_GetObjectItem(json, "usuarios");

            printf("\n[INFO] Usuarios conectados:\n");
            if (!usuarios || !cJSON_IsArray(usuarios)) {
                printf("Error al recibir la lista de usuarios.\n");
            } else if (cJSON_GetArraySize(usuarios) == 0) {
                printf("No hay usuarios conectados.\n");
            } else {
                for (int i = 0; i < cJSON_GetArraySize(usuarios); i++) {
                    printf("- %s\n", cJSON_GetArrayItem(usuarios, i)->valuestring);
                }
            }
        } 
        else if (strcmp(accion, "INFO") == 0) {
            cJSON *mensaje = cJSON_GetObjectItem(json, "mensaje");
            if (mensaje && cJSON_IsString(mensaje)) {
                printf("[INFO] %s\n", mensaje->valuestring);
            }
        } 
        else if (strcmp(accion, "ERROR") == 0) {
            cJSON *mensaje = cJSON_GetObjectItem(json, "mensaje");
            if (mensaje && cJSON_IsString(mensaje)) {
                printf("[ERROR] %s\n", mensaje->valuestring);
            }
        } 
        else {
            printf("[ERROR] Acción desconocida: %s\n", accion);
        }

        cJSON_Delete(json);
        fflush(stdout);
        printf("> ");
    }
    return NULL;
}


void enviar_json(cJSON *json) {
    char *mensaje = cJSON_PrintUnformatted(json);
    send(sockfd, mensaje, strlen(mensaje), 0);
    free(mensaje);
}

void mostrar_menu() {
    printf("\n---- MENÚ ----\n");
    printf("1. Enviar mensaje al chat general\n");
    printf("2. Enviar mensaje privado\n");
    printf("3. Listar usuarios conectados\n");
    printf("4. Cambiar estado\n");
    printf("5. Salir del chat\n");
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

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error al conectar con el servidor.\n");
        return 1;
    }

    // Enviar mensaje de registro al servidor
    cJSON *registro = cJSON_CreateObject();
    cJSON_AddStringToObject(registro, "accion", "REGISTRO");
    cJSON_AddStringToObject(registro, "usuario", nombre_usuario);
    cJSON_AddStringToObject(registro, "direccionIP", ip_servidor);
    enviar_json(registro);
    cJSON_Delete(registro);
    printf("[DEBUG] Se envió el mensaje de registro para %s\n", nombre_usuario);


    pthread_t thread_id;
    pthread_create(&thread_id, NULL, recibir_mensajes, NULL);

    while (1) {
        mostrar_menu();
        int opcion;
        scanf("%d", &opcion);
        getchar();

        if (opcion == 1) {  // Mensaje general (BROADCAST)
            printf("Mensaje: ");
            char mensaje[MAX_MENSAJE];
            fgets(mensaje, MAX_MENSAJE, stdin);
            mensaje[strcspn(mensaje, "\n")] = 0;

            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "accion", "BROADCAST");
            cJSON_AddStringToObject(json, "nombre_emisor", nombre_usuario);
            cJSON_AddStringToObject(json, "mensaje", mensaje);
            enviar_json(json);
            cJSON_Delete(json);
        } 
        else if (opcion == 2) {  // Mensaje privado (DM)
            printf("Ingrese el nombre del destinatario: ");
            char destinatario[MAX_NOMBRE];
            fgets(destinatario, MAX_NOMBRE, stdin);
            destinatario[strcspn(destinatario, "\n")] = 0;

            printf("Ingrese el mensaje: ");
            char mensaje[MAX_MENSAJE];
            fgets(mensaje, MAX_MENSAJE, stdin);
            mensaje[strcspn(mensaje, "\n")] = 0;

            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "accion", "DM");
            cJSON_AddStringToObject(json, "nombre_emisor", nombre_usuario);
            cJSON_AddStringToObject(json, "nombre_destinatario", destinatario);
            cJSON_AddStringToObject(json, "mensaje", mensaje);
            enviar_json(json);
            cJSON_Delete(json);
        } 
        else if (opcion == 3) {  // Listar usuarios conectados
            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "accion", "LISTA");
            cJSON_AddStringToObject(json, "nombre_usuario", nombre_usuario);
            enviar_json(json);
            cJSON_Delete(json);
        } 
        else if (opcion == 4) {  // Cambiar estado
            printf("Nuevo estado (ACTIVO/OCUPADO/INACTIVO): ");
            char estado[20];
            fgets(estado, 20, stdin);
            estado[strcspn(estado, "\n")] = 0;

            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "accion", "ESTADO");
            cJSON_AddStringToObject(json, "usuario", nombre_usuario);
            cJSON_AddStringToObject(json, "estado", estado);
            enviar_json(json);
            cJSON_Delete(json);
        } 
        else if (opcion == 5) {  // Salir del chat
            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "accion", "EXIT");
            cJSON_AddStringToObject(json, "usuario", nombre_usuario);
            enviar_json(json);
            cJSON_Delete(json);
            printf("Desconectado.\n");
            close(sockfd);
            break;
        } 
        else {
            printf("Opción inválida.\n");
        }
    }
    return 0;
}
