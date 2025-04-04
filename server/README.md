# Servidor de Chat en C

Este es un servidor de chat básico desarrollado en **C** para sistemas basados en Linux. El servidor permite que múltiples clientes se conecten y se comuniquen entre sí a través de mensajes generales y mensajes directos. El servidor está diseñado para manejar múltiples conexiones simultáneas mediante **multithreading**.

## Características

- **Registro de usuarios**: Los usuarios se registran con un nombre único.
- **Mensajes generales (broadcasting)**: Todos los usuarios conectados reciben los mensajes enviados al chat general.
- **Mensajes directos**: Los usuarios pueden enviarse mensajes entre sí.
- **Manejo de conexiones**: El servidor puede manejar múltiples clientes al mismo tiempo utilizando hilos.
- **Desconexión de usuarios**: El servidor detecta y maneja la desconexión de clientes correctamente.

## Requisitos

- **gcc**: El compilador de C.
- **pthread**: Para la implementación de multithreading.

## Instalación y ejecución

1. **Clonar el repositorio**:
   git clone <https://github.com/michellemej22596/Proyecto1-Sistemas-Operativos-Chat.git>

2. **Compilar el servidor**:
    gcc -o server server.c -pthread

3. **Ejecutar el servidor**:
    ./server 8080

## Uso
- Conexión de clientes: Los clientes deben conectarse al servidor utilizando un cliente de chat basado en WebSockets o cualquier otra herramienta que soporte la conexión a un servidor de chat por IP y puerto.

- Registrar un usuario: Al conectarse por primera vez, el cliente debe enviar su nombre de usuario. El servidor validará que no haya otro usuario con el mismo nombre.

- Enviar y recibir mensajes: Los clientes pueden enviar mensajes al chat general o a un usuario específico (mensaje directo).

- Desconexión: Cuando un cliente se desconecta, el servidor elimina al usuario de la lista de clientes conectados.    



