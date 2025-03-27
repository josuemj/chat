// CLIENTE DE CHAT EN C - PROTOCOLO JSON CORREGIDO SEGUN PROTOCOLO LUCIDCHART
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
            printf("[INFO] Conexi\u00f3n cerrada por el servidor.\n");
            close(sockfd);
            exit(1);
        }

        cJSON *json = cJSON_Parse(buffer);
        if (!json) continue;

        cJSON *accion_json = cJSON_GetObjectItem(json, "accion");
        cJSON *tipo_json = cJSON_GetObjectItem(json, "tipo");
        cJSON *respuesta_json = cJSON_GetObjectItem(json, "respuesta");

        if (accion_json && cJSON_IsString(accion_json)) {
            const char *accion = accion_json->valuestring;
            if (strcmp(accion, "BROADCAST") == 0 || strcmp(accion, "DM") == 0) {
                printf("[%s] %s: %s\n", accion,
                    cJSON_GetObjectItem(json, "nombre_emisor")->valuestring,
                    cJSON_GetObjectItem(json, "mensaje")->valuestring);
            } else if (strcmp(accion, "LISTA") == 0) {
                printf("\n[INFO] Usuarios conectados:\n");
                cJSON *usuarios = cJSON_GetObjectItem(json, "usuarios");
                if (usuarios && cJSON_IsArray(usuarios)) {
                    for (int i = 0; i < cJSON_GetArraySize(usuarios); i++) {
                        printf("- %s\n", cJSON_GetArrayItem(usuarios, i)->valuestring);
                    }
                }
            } else if (strcmp(accion, "MOSTRAR") == 0) {
                printf("[INFO] Estado de %s: %s\n",
                    cJSON_GetObjectItem(json, "usuario")->valuestring,
                    cJSON_GetObjectItem(json, "estado")->valuestring);
            }
        } else if (respuesta_json && cJSON_IsString(respuesta_json)) {
            if (strcmp(respuesta_json->valuestring, "ERROR") == 0) {
                printf("[ERROR] %s\n", cJSON_GetObjectItem(json, "razon")->valuestring);
            } else {
                printf("[INFO] %s\n", respuesta_json->valuestring);
            }
        }

        cJSON_Delete(json);
        printf("> ");
        fflush(stdout);
    }
    return NULL;
}

void enviar_json(cJSON *json) {
    char *mensaje = cJSON_PrintUnformatted(json);
    send(sockfd, mensaje, strlen(mensaje), 0);
    free(mensaje);
}

void mostrar_menu() {
    printf("\n---- MEN\u00da ----\n");
    printf("1. Enviar mensaje al chat general\n");
    printf("2. Enviar mensaje privado\n");
    printf("3. Listar usuarios conectados\n");
    printf("4. Cambiar estado\n");
    printf("5. Mostrar estado de otro usuario\n");
    printf("6. Salir del chat\n");
    printf("Seleccione una opci\u00f3n: ");
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
        printf("[ERROR] No se pudo conectar al servidor.\n");
        return 1;
    }

    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    getsockname(sockfd, (struct sockaddr *)&local_addr, &addr_len);
    char ip_cliente[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, ip_cliente, sizeof(ip_cliente));

    cJSON *registro = cJSON_CreateObject();
    cJSON_AddStringToObject(registro, "tipo", "REGISTRO");
    cJSON_AddStringToObject(registro, "usuario", nombre_usuario);

    
    // ON_AddStringToObject(registro, "direccionIP", ip_cliente); // ESTA NO DEBE IR
    enviar_json(registro);
    cJSON_Delete(registro);

    char buffer[MAX_MENSAJE];
    memset(buffer, 0, sizeof(buffer));
    recv(sockfd, buffer, sizeof(buffer), 0);

    cJSON *respuesta = cJSON_Parse(buffer);
    if (respuesta) {
        cJSON *resp = cJSON_GetObjectItem(respuesta, "respuesta");
        if (resp && strcmp(resp->valuestring, "ERROR") == 0) {
            printf("[ERROR] %s\n", cJSON_GetObjectItem(respuesta, "razon")->valuestring);
            close(sockfd);
            return 1;
        } else {
            printf("[INFO] Registro exitoso.\n");
        }
        cJSON_Delete(respuesta);
    }

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, recibir_mensajes, NULL);

    while (1) {
        mostrar_menu();
        int opcion;
        scanf("%d", &opcion);
        getchar();

        if (opcion == 1) {
            char mensaje[MAX_MENSAJE];
            printf("Mensaje: ");
            fgets(mensaje, MAX_MENSAJE, stdin);
            mensaje[strcspn(mensaje, "\n")] = 0;

            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "accion", "BROADCAST");
            cJSON_AddStringToObject(json, "nombre_emisor", nombre_usuario);
            cJSON_AddStringToObject(json, "mensaje", mensaje);
            enviar_json(json);
            cJSON_Delete(json);
        }
        else if (opcion == 2) {
            char destinatario[MAX_NOMBRE], mensaje[MAX_MENSAJE];
            printf("Destinatario: ");
            fgets(destinatario, MAX_NOMBRE, stdin);
            destinatario[strcspn(destinatario, "\n")] = 0;

            printf("Mensaje: ");
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
        else if (opcion == 3) {
            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "tipo", "LISTA"); // acccion | tipo
            cJSON_AddStringToObject(json, "nombre_usuario", nombre_usuario);
            enviar_json(json);
            cJSON_Delete(json);
        }
        else if (opcion == 4) {
            char estado[20];
            printf("Nuevo estado: ");
            fgets(estado, 20, stdin);
            estado[strcspn(estado, "\n")] = 0;

            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "tipo", "ESTADO");
            cJSON_AddStringToObject(json, "usuario", nombre_usuario);
            cJSON_AddStringToObject(json, "estado", estado);
            enviar_json(json);
            cJSON_Delete(json);
        }
        else if (opcion == 5) {
            char usuario[MAX_NOMBRE];
            printf("Usuario a mostrar: ");
            fgets(usuario, MAX_NOMBRE, stdin);
            usuario[strcspn(usuario, "\n")] = 0;

            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "tipo", "MOSTRAR");
            cJSON_AddStringToObject(json, "usuario", usuario);
            enviar_json(json);
            cJSON_Delete(json);
        }
        else if (opcion == 6) {
            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "tipo", "EXIT");
            cJSON_AddStringToObject(json, "usuario", nombre_usuario);
            cJSON_AddStringToObject(json, "estado", "");
            enviar_json(json);
            cJSON_Delete(json);
            close(sockfd);
            break;
        }
    }
    return 0;
}
