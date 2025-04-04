#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

// Estructura para manejar la información de los clientes
typedef struct {
    int socket;
    char username[50];
} ClientInfo;

// Lista de clientes conectados (para manejar broadcasting)
ClientInfo clients[100];
int clientCount = 0;

void *handleClient(void *arg) {
    ClientInfo *client = (ClientInfo *)arg;
    char buffer[256];
    int n;

    // Enviar un mensaje de bienvenida al cliente
    char *welcome_msg = "Ingrese su nombre de usuario: ";
    write(client->socket, welcome_msg, strlen(welcome_msg));

    // Recibir el nombre de usuario
    bzero(buffer, 256);
    n = read(client->socket, buffer, 255);
    if (n < 0) {
        perror("Error al leer el nombre de usuario");
        close(client->socket);
        free(client);
        pthread_exit(NULL);
    }

    // Eliminar el salto de línea al final del nombre de usuario
    buffer[strcspn(buffer, "\n")] = 0;

    // Verificar si el nombre ya está en uso
    for (int i = 0; i < clientCount; i++) {
        if (strcmp(clients[i].username, buffer) == 0) {
            char *error_msg = "Este nombre de usuario ya está en uso. Conéctese con otro nombre.\n";
            write(client->socket, error_msg, strlen(error_msg));
            close(client->socket);
            free(client);
            pthread_exit(NULL);
        }
    }

    // Si el nombre es único, asignarlo al cliente y agregarlo a la lista
    strcpy(client->username, buffer);
    clients[clientCount++] = *client;

    // Notificar al cliente que está registrado
    char *success_msg = "Nombre de usuario registrado exitosamente. ¡Conéctate y empieza a chatear!\n";
    write(client->socket, success_msg, strlen(success_msg));

    // Enviar mensaje a los demás usuarios cuando alguien se conecta
    char notification[256];
    snprintf(notification, sizeof(notification), "El usuario %s se ha conectado.\n", client->username);
    for (int i = 0; i < clientCount; i++) {
        if (clients[i].socket != client->socket) {
            write(clients[i].socket, notification, strlen(notification));
        }
    }

    // Manejo de mensajes
    while (1) {
        bzero(buffer, 256);
        n = read(client->socket, buffer, 255);
        if (n <= 0) { // Si se desconecta o hay un error
            printf("El cliente %s se desconectó\n", client->username);
            break;
        }

        // Si el mensaje es "salir", desconectar al cliente
        if (strncmp("salir", buffer, 5) == 0) {
            break;
        }

        // Enviar el mensaje a todos los clientes conectados (broadcasting)
        for (int i = 0; i < clientCount; i++) {
            if (clients[i].socket != client->socket) {
                write(clients[i].socket, buffer, strlen(buffer));
            }
        }
    }

    // Cuando el cliente se desconecta, notificamos a los demás usuarios
    char disconnect_msg[256];
    snprintf(disconnect_msg, sizeof(disconnect_msg), "El usuario %s se ha desconectado.\n", client->username);
    for (int i = 0; i < clientCount; i++) {
        if (clients[i].socket != client->socket) {
            write(clients[i].socket, disconnect_msg, strlen(disconnect_msg));
        }
    }

    // Eliminar al cliente de la lista de usuarios
    for (int i = 0; i < clientCount; i++) {
        if (clients[i].socket == client->socket) {
            for (int j = i; j < clientCount - 1; j++) {
                clients[j] = clients[j + 1]; // Mover los clientes a la izquierda
            }
            clientCount--;
            break;
        }
    }

    // Cerrar la conexión del cliente
    close(client->socket);
    free(client);
    pthread_exit(NULL);
}


int main(int argc, char *argv[]) {
    int serverSock, clientSock, port;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen;
    pthread_t tid;

    if (argc != 2) {
        printf("Uso: %s <puerto>\n", argv[0]);
        exit(1);
    }

    port = atoi(argv[1]);

    // Crear socket
    serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        perror("No se pudo crear el socket");
        exit(1);
    }

    // Configurar dirección del servidor
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    // Enlazar el socket
    if (bind(serverSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Error al enlazar");
        exit(1);
    }

    // Escuchar por conexiones entrantes
    listen(serverSock, 5);

    printf("Servidor escuchando en el puerto %d...\n", port);

    // Aceptar conexiones entrantes
    while (1) {
        clientAddrLen = sizeof(clientAddr);
        clientSock = accept(serverSock, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (clientSock < 0) {
            perror("Error al aceptar la conexión");
            continue;
        }

        // Crear un hilo para manejar el cliente
        ClientInfo *newClient = malloc(sizeof(ClientInfo));
        newClient->socket = clientSock;
        // Aquí puedes agregar lógica para registrar el nombre de usuario
        pthread_create(&tid, NULL, handleClient, (void *)newClient);
    }

    close(serverSock);
    return 0;
}
