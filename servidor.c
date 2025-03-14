// ===========================================
// SERVIDOR DE CHAT EN C - PROTOCOLO JSON
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

#define MAX_CLIENTES 100
#define MAX_NOMBRE 50
#define MAX_MENSAJE 1024
#define PUERTO 50213

struct Cliente {
    int socket;
    char nombre[MAX_NOMBRE];
    char estado[20];
    char ip[INET_ADDRSTRLEN];
};

struct Cliente *clientes[MAX_CLIENTES];
pthread_mutex_t mutex_clientes = PTHREAD_MUTEX_INITIALIZER;

void broadcast_json(cJSON *json, int remitente_socket) {
    char *mensaje = cJSON_PrintUnformatted(json);
    pthread_mutex_lock(&mutex_clientes);
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (clientes[i] && clientes[i]->socket != remitente_socket) {
            send(clientes[i]->socket, mensaje, strlen(mensaje), 0);
        }
    }
    pthread_mutex_unlock(&mutex_clientes);
    free(mensaje);
}

void enviar_json(int socket, cJSON *json) {
    char *mensaje = cJSON_PrintUnformatted(json);
    send(socket, mensaje, strlen(mensaje), 0);
    free(mensaje);
}

void *manejar_cliente(void *arg) {
    struct Cliente *cliente = (struct Cliente *)arg;
    char buffer[MAX_MENSAJE];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_recibidos = recv(cliente->socket, buffer, sizeof(buffer), 0);
        if (bytes_recibidos <= 0) break;

        cJSON *json = cJSON_Parse(buffer);
        if (!json) continue;

        const char *accion = cJSON_GetObjectItem(json, "accion")->valuestring;

        if (strcmp(accion, "REGISTRO") == 0) {
            const char *usuario = cJSON_GetObjectItem(json, "usuario")->valuestring;
            const char *ip = cJSON_GetObjectItem(json, "direccionIP")->valuestring;

            int duplicado = 0;
            pthread_mutex_lock(&mutex_clientes);
            for (int i = 0; i < MAX_CLIENTES; i++) {
                if (clientes[i] && strcmp(clientes[i]->nombre, usuario) == 0) {
                    duplicado = 1;
                    break;
                }
            }

            if (!duplicado) {
                strcpy(cliente->nombre, usuario);
                strcpy(cliente->ip, ip);
                strcpy(cliente->estado, "ACTIVO");

                for (int i = 0; i < MAX_CLIENTES; i++) {
                    if (clientes[i] == NULL) {
                        clientes[i] = cliente;
                        break;
                    }
                }

                cJSON *respuesta = cJSON_CreateObject();
                cJSON_AddStringToObject(respuesta, "accion", "INFO");
                cJSON_AddStringToObject(respuesta, "mensaje", "Registro exitoso");
                enviar_json(cliente->socket, respuesta);
                cJSON_Delete(respuesta);
            } else {
                cJSON *error = cJSON_CreateObject();
                cJSON_AddStringToObject(error, "accion", "ERROR");
                cJSON_AddStringToObject(error, "mensaje", "Nombre o dirección duplicado");
                enviar_json(cliente->socket, error);
                cJSON_Delete(error);
            }
            pthread_mutex_unlock(&mutex_clientes);
        } 
        else if (strcmp(accion, "EXIT") == 0) {
                pthread_mutex_lock(&mutex_clientes);
                for (int i = 0; i < MAX_CLIENTES; i++) {
                    if (clientes[i] == cliente) {
                        clientes[i] = NULL;
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex_clientes);
            
                cJSON *mensaje = cJSON_CreateObject();
                cJSON_AddStringToObject(mensaje, "accion", "INFO");
                cJSON_AddStringToObject(mensaje, "mensaje", cliente->nombre);
                broadcast_json(mensaje, cliente->socket);
                cJSON_Delete(mensaje);
            
                close(cliente->socket);
                free(cliente);
                pthread_exit(NULL);           
        } 
        else if (strcmp(accion, "BROADCAST") == 0) {
            const char *mensaje = cJSON_GetObjectItem(json, "mensaje")->valuestring;
            cJSON *mensaje_json = cJSON_CreateObject();
            cJSON_AddStringToObject(mensaje_json, "accion", "BROADCAST");
            cJSON_AddStringToObject(mensaje_json, "nombre_emisor", cliente->nombre);
            cJSON_AddStringToObject(mensaje_json, "mensaje", mensaje);
            broadcast_json(mensaje_json, cliente->socket);
            cJSON_Delete(mensaje_json);
        } 
        else if (strcmp(accion, "DM") == 0) {
            const char *destinatario = cJSON_GetObjectItem(json, "nombre_destinatario")->valuestring;
            const char *mensaje = cJSON_GetObjectItem(json, "mensaje")->valuestring;

            int encontrado = 0;
            pthread_mutex_lock(&mutex_clientes);
            for (int i = 0; i < MAX_CLIENTES; i++) {
                if (clientes[i] && strcmp(clientes[i]->nombre, destinatario) == 0) {
                    cJSON *dm_json = cJSON_CreateObject();
                    cJSON_AddStringToObject(dm_json, "accion", "DM");
                    cJSON_AddStringToObject(dm_json, "nombre_emisor", cliente->nombre);
                    cJSON_AddStringToObject(dm_json, "mensaje", mensaje);
                    enviar_json(clientes[i]->socket, dm_json);
                    cJSON_Delete(dm_json);

                    cJSON *confirmacion = cJSON_CreateObject();
                    cJSON_AddStringToObject(confirmacion, "accion", "INFO");
                    cJSON_AddStringToObject(confirmacion, "mensaje", "Mensaje enviado");
                    enviar_json(cliente->socket, confirmacion);
                    cJSON_Delete(confirmacion);

                    encontrado = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex_clientes);

            if (!encontrado) {
                cJSON *error = cJSON_CreateObject();
                cJSON_AddStringToObject(error, "accion", "ERROR");
                cJSON_AddStringToObject(error, "mensaje", "Usuario no encontrado");
                enviar_json(cliente->socket, error);
                cJSON_Delete(error);
            }
        }
        else if (strcmp(accion, "LISTA") == 0) {
            cJSON *lista_json = cJSON_CreateObject();
            cJSON_AddStringToObject(lista_json, "accion", "LISTA");
            cJSON *usuarios_array = cJSON_CreateArray();

            pthread_mutex_lock(&mutex_clientes);
            for (int i = 0; i < MAX_CLIENTES; i++) {
                if (clientes[i]) {
                    cJSON_AddItemToArray(usuarios_array, cJSON_CreateString(clientes[i]->nombre));
                }
            }
            pthread_mutex_unlock(&mutex_clientes);

            cJSON_AddItemToObject(lista_json, "usuarios", usuarios_array);
            enviar_json(cliente->socket, lista_json);
            cJSON_Delete(lista_json);
        }

        cJSON_Delete(json);
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

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servidor_addr;

    servidor_addr.sin_family = AF_INET;
    servidor_addr.sin_port = htons(PUERTO);
    servidor_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_socket, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr));
    listen(server_socket, 10);

    printf("Servidor iniciado en puerto %d...\n", PUERTO);

    while (1) {
        struct sockaddr_in cliente_addr;
        socklen_t cliente_len = sizeof(cliente_addr);
        int cliente_socket = accept(server_socket, (struct sockaddr *)&cliente_addr, &cliente_len);

        struct Cliente *nuevo_cliente = (struct Cliente *)malloc(sizeof(struct Cliente));
        nuevo_cliente->socket = cliente_socket;

        pthread_t hilo;
        pthread_create(&hilo, NULL, manejar_cliente, (void *)nuevo_cliente);
    }
    return 0;
}
