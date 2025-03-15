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

        cJSON *json = cJSON_Parse(buffer);
        if (!json) continue;

        cJSON *tipo_json = cJSON_GetObjectItem(json, "tipo");
        if (!tipo_json || !cJSON_IsString(tipo_json)) {
            cJSON *mensaje = cJSON_GetObjectItem(json, "mensaje");
            if (mensaje && cJSON_IsString(mensaje)) {
                printf("[INFO] %s\n", mensaje->valuestring);
            }
            cJSON_Delete(json);
            continue;
        }

        const char *tipo = tipo_json->valuestring;

        if (strcmp(tipo, "BROADCAST") == 0) {
            printf("[CHAT] %s: %s\n",
                   cJSON_GetObjectItem(json, "nombre_emisor")->valuestring,
                   cJSON_GetObjectItem(json, "mensaje")->valuestring);
        } 
        else if (strcmp(tipo, "DM") == 0) {
            printf("[PRIVADO] %s: %s\n",
                   cJSON_GetObjectItem(json, "nombre_emisor")->valuestring,
                   cJSON_GetObjectItem(json, "mensaje")->valuestring);
        } 
        else if (strcmp(tipo, "MOSTRAR") == 0) {
            printf("\n[INFO] Usuario: %s\n", cJSON_GetObjectItem(json, "usuario")->valuestring);
            printf("[INFO] Estado: %s\n", cJSON_GetObjectItem(json, "estado")->valuestring);
        } 
        else if (strcmp(tipo, "ERROR") == 0) {
            cJSON *razon = cJSON_GetObjectItem(json, "razon");
            if (razon && cJSON_IsString(razon)) {
                printf("[ERROR] %s\n", razon->valuestring);
            }
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
    printf("6. Mostrar información de un usuario\n");
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
    cJSON_AddStringToObject(registro, "tipo", "REGISTRO");
    cJSON_AddStringToObject(registro, "usuario", nombre_usuario);
    cJSON_AddStringToObject(registro, "direccionIP", ip_servidor);
    enviar_json(registro);
    cJSON_Delete(registro);

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, recibir_mensajes, NULL);

    while (1) {
        mostrar_menu();
        int opcion;
        scanf("%d", &opcion);
        getchar();

        if (opcion == 1) {  
            printf("Mensaje: ");
            char mensaje[MAX_MENSAJE];
            fgets(mensaje, MAX_MENSAJE, stdin);
            mensaje[strcspn(mensaje, "\n")] = 0;

            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "tipo", "BROADCAST");
            cJSON_AddStringToObject(json, "nombre_emisor", nombre_usuario);
            cJSON_AddStringToObject(json, "mensaje", mensaje);
            enviar_json(json);
            cJSON_Delete(json);
        } 
        else if (opcion == 2) {  
            printf("Ingrese el nombre del destinatario: ");
            char destinatario[MAX_NOMBRE];
            fgets(destinatario, MAX_NOMBRE, stdin);
            destinatario[strcspn(destinatario, "\n")] = 0;

            printf("Ingrese el mensaje: ");
            char mensaje[MAX_MENSAJE];
            fgets(mensaje, MAX_MENSAJE, stdin);
            mensaje[strcspn(mensaje, "\n")] = 0;

            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "tipo", "DM");
            cJSON_AddStringToObject(json, "nombre_emisor", nombre_usuario);
            cJSON_AddStringToObject(json, "nombre_destinatario", destinatario);
            cJSON_AddStringToObject(json, "mensaje", mensaje);
            enviar_json(json);
            cJSON_Delete(json);
        }   
        else if (opcion == 3) {  
            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "tipo", "LISTA");
            cJSON_AddStringToObject(json, "nombre_usuario", nombre_usuario);
            enviar_json(json);
            cJSON_Delete(json);
            
            // Esperar respuesta del servidor antes de mostrar el menú
            char buffer[MAX_MENSAJE];
            memset(buffer, 0, sizeof(buffer));
            recv(sockfd, buffer, sizeof(buffer), 0);
            
            // Procesar la respuesta
            cJSON *respuesta = cJSON_Parse(buffer);
            if (respuesta) {
                cJSON *tipo_json = cJSON_GetObjectItem(respuesta, "tipo");
                if (tipo_json && cJSON_IsString(tipo_json) && strcmp(tipo_json->valuestring, "LISTA") == 0) {
                    printf("\n[INFO] Usuarios conectados:\n");
                    cJSON *usuarios = cJSON_GetObjectItem(respuesta, "usuarios");
            
                    if (!usuarios || !cJSON_IsArray(usuarios)) {
                        printf("Error al recibir la lista de usuarios.\n");
                    } else if (cJSON_GetArraySize(usuarios) == 0) {
                        printf("No hay usuarios conectados.\n");
                    } else {
                        for (int i = 0; i < cJSON_GetArraySize(usuarios); i++) {
                            printf("- %s\n", cJSON_GetArrayItem(usuarios, i)->valuestring);
                        }
                    }
                    printf("\n");
                }
                cJSON_Delete(respuesta);
            }
        }
        
        else if (opcion == 4) {  
            printf("Nuevo estado (ACTIVO/OCUPADO/INACTIVO): ");
            char estado[20];
            fgets(estado, 20, stdin);
            estado[strcspn(estado, "\n")] = 0;

            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "tipo", "ESTADO");
            cJSON_AddStringToObject(json, "usuario", nombre_usuario);
            cJSON_AddStringToObject(json, "estado", estado);
            enviar_json(json);
            cJSON_Delete(json);
        } 
        else if (opcion == 5) {  // Salir del chat
            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "tipo", "EXIT");
            cJSON_AddStringToObject(json, "usuario", nombre_usuario);
            cJSON_AddStringToObject(json, "estado", "");
            enviar_json(json);
            cJSON_Delete(json);
        
            char buffer[MAX_MENSAJE];
            memset(buffer, 0, sizeof(buffer));
            recv(sockfd, buffer, sizeof(buffer), 0);
        
            cJSON *respuesta = cJSON_Parse(buffer);
            if (respuesta) {
                cJSON *response = cJSON_GetObjectItem(respuesta, "response");
                if (response && cJSON_IsString(response) && strcmp(response->valuestring, "OK") == 0) {
                    printf("[INFO] Desconectado correctamente.\n");
                }
                cJSON_Delete(respuesta);
            }
        
            close(sockfd);
            break;
        }
        
        else if (opcion == 6) {  
            printf("Ingrese el nombre del usuario: ");
            char usuario[MAX_NOMBRE];
            fgets(usuario, MAX_NOMBRE, stdin);
            usuario[strcspn(usuario, "\n")] = 0;
        
            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "tipo", "MOSTRAR");
            cJSON_AddStringToObject(json, "usuario", usuario);
            enviar_json(json);
            cJSON_Delete(json);
        
            char buffer[MAX_MENSAJE];
            memset(buffer, 0, sizeof(buffer));
            recv(sockfd, buffer, sizeof(buffer), 0);
        
            // Procesar la respuesta
            cJSON *respuesta = cJSON_Parse(buffer);
            if (respuesta) {
                cJSON *tipo_json = cJSON_GetObjectItem(respuesta, "tipo");
                if (tipo_json && cJSON_IsString(tipo_json) && strcmp(tipo_json->valuestring, "MOSTRAR") == 0) {
                    printf("\n[INFO] Usuario: %s\n", cJSON_GetObjectItem(respuesta, "usuario")->valuestring);
                    printf("[INFO] Estado: %s\n", cJSON_GetObjectItem(respuesta, "estado")->valuestring);
                    printf("[INFO] IP: %s\n\n", cJSON_GetObjectItem(respuesta, "ip")->valuestring);
                } 
                else {
                    cJSON *error = cJSON_GetObjectItem(respuesta, "razon");
                    if (error && cJSON_IsString(error)) {
                        printf("[ERROR] %s\n", error->valuestring);
                    }
                }
                cJSON_Delete(respuesta);
            }
        }
        
        else {
            printf("Opción inválida.\n");
        }
    }
    return 0;
}
