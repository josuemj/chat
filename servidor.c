// SERVIDOR DE CHAT EN C - LISTA DE USUARIOS COMO STRING JSON
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#include "cJSON/cJSON.h"

#pragma comment(lib, "ws2_32.lib")
#define MAX_CLIENTES 100
#define MAX_NOMBRE 50
#define MAX_MENSAJE 2048
#define PUERTO 50212
#define INET_ADDRSTRLEN 16

struct Cliente {
    SOCKET socket;
    char nombre[MAX_NOMBRE];
    char estado[20];
    char ip[INET_ADDRSTRLEN];
    time_t ultima_actividad;
};

struct Cliente *clientes[MAX_CLIENTES];
HANDLE mutex_clientes;

void broadcast_json(cJSON *json, SOCKET remitente_socket) {
    char *mensaje = cJSON_PrintUnformatted(json);
    printf("[SERVER] BROADCAST:\n%s\n", mensaje);
    WaitForSingleObject(mutex_clientes, INFINITE);
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (clientes[i] && clientes[i]->socket != remitente_socket) {
            send(clientes[i]->socket, mensaje, strlen(mensaje), 0);
        }
    }
    ReleaseMutex(mutex_clientes);
    free(mensaje);
}

void enviar_json(SOCKET socket, cJSON *json) {
    char *mensaje = cJSON_PrintUnformatted(json);
    printf("[SERVER] Enviado:\n%s\n", mensaje);
    send(socket, mensaje, strlen(mensaje), 0);
    free(mensaje);
}

DWORD WINAPI manejar_cliente(LPVOID arg) {
    struct Cliente *cliente = (struct Cliente *)arg;
    char buffer[MAX_MENSAJE];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_recibidos = recv(cliente->socket, buffer, sizeof(buffer), 0);
        if (bytes_recibidos <= 0) break;

        time(&cliente->ultima_actividad);

        cJSON *json = cJSON_Parse(buffer);
        if (!json) continue;

        const char *accion = cJSON_GetObjectItem(json, "accion") ?
                             cJSON_GetObjectItem(json, "accion")->valuestring :
                             cJSON_GetObjectItem(json, "tipo") ?
                             cJSON_GetObjectItem(json, "tipo")->valuestring :
                             NULL;

        if (!accion) {
            cJSON_Delete(json);
            continue;
        }

        if (strcmp(accion, "REGISTRO") == 0) {
            const char *usuario = cJSON_GetObjectItem(json, "usuario")->valuestring;
            const char *ip = cJSON_GetObjectItem(json, "direccionIP") ?
                             cJSON_GetObjectItem(json, "direccionIP")->valuestring : "";

            int duplicado = 0;
            WaitForSingleObject(mutex_clientes, INFINITE);
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
                cJSON_AddStringToObject(respuesta, "respuesta", "OK");
                enviar_json(cliente->socket, respuesta);
                cJSON_Delete(respuesta);
                printf("[SERVER] Registrado: %s (%s)\n", cliente->nombre, cliente->ip);
            } else {
                cJSON *error = cJSON_CreateObject();
                cJSON_AddStringToObject(error, "respuesta", "ERROR");
                cJSON_AddStringToObject(error, "razon", "Nombre o direccion duplicado");
                enviar_json(cliente->socket, error);
                cJSON_Delete(error);
                printf("[SERVER] Duplicado: %s\n", usuario);
            }
            ReleaseMutex(mutex_clientes);
        }
        else if (strcmp(accion, "EXIT") == 0) {
            WaitForSingleObject(mutex_clientes, INFINITE);
            for (int i = 0; i < MAX_CLIENTES; i++) {
                if (clientes[i] == cliente) {
                    clientes[i] = NULL;
                    break;
                }
            }
            ReleaseMutex(mutex_clientes);
            closesocket(cliente->socket);
            free(cliente);
            printf("[SERVER] Cliente desconectado.\n");
            ExitThread(0);
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
            WaitForSingleObject(mutex_clientes, INFINITE);
            for (int i = 0; i < MAX_CLIENTES; i++) {
                if (clientes[i] && strcmp(clientes[i]->nombre, destinatario) == 0) {
                    cJSON *dm_json = cJSON_CreateObject();
                    cJSON_AddStringToObject(dm_json, "accion", "DM");
                    cJSON_AddStringToObject(dm_json, "nombre_emisor", cliente->nombre);
                    cJSON_AddStringToObject(dm_json, "mensaje", mensaje);
                    enviar_json(clientes[i]->socket, dm_json);
                    cJSON_Delete(dm_json);
                    encontrado = 1;
                    break;
                }
            }
            ReleaseMutex(mutex_clientes);
            if (!encontrado) {
                cJSON *error = cJSON_CreateObject();
                cJSON_AddStringToObject(error, "accion", "ERROR");
                cJSON_AddStringToObject(error, "mensaje", "Usuario no encontrado");
                enviar_json(cliente->socket, error);
                cJSON_Delete(error);
                printf("[SERVER] DM no enviado, %s no encontrado\n", destinatario);
            } else {
                printf("[SERVER] DM enviado a %s\n", destinatario);
            }
        }

        else if (strcmp(accion, "MOSTRAR") == 0) {
            const char *usuario_buscado = cJSON_GetObjectItem(json, "usuario")->valuestring;
            int encontrado = 0;
        
            WaitForSingleObject(mutex_clientes, INFINITE);
            for (int i = 0; i < MAX_CLIENTES; i++) {
                if (clientes[i] && strcmp(clientes[i]->nombre, usuario_buscado) == 0) {
                    encontrado = 1;
                    cJSON *respuesta = cJSON_CreateObject();
                    cJSON_AddStringToObject(respuesta, "accion", "MOSTRAR");
                    cJSON_AddStringToObject(respuesta, "usuario", clientes[i]->nombre);
                    cJSON_AddStringToObject(respuesta, "estado", clientes[i]->estado);
                    enviar_json(cliente->socket, respuesta);
                    cJSON_Delete(respuesta);
                    break;
                }
            }
            ReleaseMutex(mutex_clientes);
        
            if (!encontrado) {
                cJSON *error = cJSON_CreateObject();
                cJSON_AddStringToObject(error, "respuesta", "ERROR");
                cJSON_AddStringToObject(error, "razon", "USUARIO_NO_ENCONTRADO");
                enviar_json(cliente->socket, error);
                cJSON_Delete(error);
            }
        }
        

        else if (strcmp(accion, "ESTADO") == 0) {
            const char *nuevo_estado = cJSON_GetObjectItem(json, "estado")->valuestring;
        
            if (strcmp(cliente->estado, nuevo_estado) == 0) {
                cJSON *error = cJSON_CreateObject();
                cJSON_AddStringToObject(error, "respuesta", "ERROR");
                cJSON_AddStringToObject(error, "razon", "ESTADO_YA_SELECCIONADO");
                enviar_json(cliente->socket, error);
                cJSON_Delete(error);
            } else {
                strcpy(cliente->estado, nuevo_estado);
                cJSON *ok = cJSON_CreateObject();
                cJSON_AddStringToObject(ok, "respuesta", "OK");
                enviar_json(cliente->socket, ok);
                cJSON_Delete(ok);
                printf("[SERVER] Estado actualizado para %s: %s\n", cliente->nombre, cliente->estado);
            }
        }
        
        else if (strcmp(accion, "LISTA") == 0) {
            WaitForSingleObject(mutex_clientes, INFINITE);
            cJSON *usuarios_array = cJSON_CreateArray();
            for (int i = 0; i < MAX_CLIENTES; i++) {
                if (clientes[i]) {
                    cJSON *item = cJSON_CreateObject();
                    cJSON_AddStringToObject(item, "nombre", clientes[i]->nombre);
                    cJSON_AddStringToObject(item, "ip", clientes[i]->ip);
                    cJSON_AddStringToObject(item, "estado", clientes[i]->estado);
                    cJSON_AddItemToArray(usuarios_array, item);
                }
            }
            ReleaseMutex(mutex_clientes);

            cJSON *usuarios_nombres = cJSON_CreateArray();
            for (int i = 0; i < MAX_CLIENTES; i++) {
                if (clientes[i]) {
                    cJSON_AddItemToArray(usuarios_nombres, cJSON_CreateString(clientes[i]->nombre));
                }
            }
            cJSON *respuesta = cJSON_CreateObject();
            cJSON_AddStringToObject(respuesta, "accion", "LISTA");
            cJSON_AddItemToObject(respuesta, "usuarios", usuarios_nombres);
            enviar_json(cliente->socket, respuesta);
            cJSON_Delete(respuesta);            
            printf("[SERVER] Lista enviada a %s\n", cliente->nombre);
        }

        cJSON_Delete(json);
    }

    WaitForSingleObject(mutex_clientes, INFINITE);
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (clientes[i] == cliente) {
            clientes[i] = NULL;
            break;
        }
    }
    ReleaseMutex(mutex_clientes);
    closesocket(cliente->socket);
    free(cliente);
    ExitThread(0);
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servidor_addr;
    servidor_addr.sin_family = AF_INET;
    servidor_addr.sin_port = htons(PUERTO);
    servidor_addr.sin_addr.s_addr = INADDR_ANY;
    bind(server_socket, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr));
    listen(server_socket, 10);
    mutex_clientes = CreateMutex(NULL, FALSE, NULL);
    printf("Servidor iniciado en puerto %d...\n", PUERTO);

    while (1) {
        struct sockaddr_in cliente_addr;
        int cliente_len = sizeof(cliente_addr);
        SOCKET cliente_socket = accept(server_socket, (struct sockaddr *)&cliente_addr, &cliente_len);
        struct Cliente *nuevo_cliente = (struct Cliente *)malloc(sizeof(struct Cliente));
        time(&nuevo_cliente->ultima_actividad);
        nuevo_cliente->socket = cliente_socket;
        CreateThread(NULL, 0, manejar_cliente, (void *)nuevo_cliente, 0, NULL);
        printf("[SERVER] Nuevo cliente conectado.\n");
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}
