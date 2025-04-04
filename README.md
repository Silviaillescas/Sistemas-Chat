# Proyecto1-Sistemas-Operativos-Chat
Michelle Mejía, Silvia Illescas y Diederich Solis 

Este proyecto consiste en un **sistema de chat en C** donde los clientes pueden conectarse a un servidor y comunicarse entre sí utilizando mensajes generales y privados. El servidor maneja múltiples clientes utilizando **multithreading**, asegurando una comunicación en tiempo real.

## Estructura del Proyecto

Este repositorio contiene el código para el **servidor** y el **cliente** del sistema de chat, así como configuraciones para su **despliegue en la nube** y detalles para **pruebas**.

### Estructura del Repositorio

```
/Proyecto-Chat
│
├── /server                # Código fuente del servidor
│   └── server.c           # Código principal del servidor en C
│
├── /client                # Código fuente del cliente
│   └── client.c           # Código principal del cliente en C
│
├── /deploy                # Archivos para desplegar en la nube
│   └── deploy.sh          # Script para desplegar el servidor en AWS
│
├── .gitignore             # Archivos y carpetas a ignorar por git
├── README.md              # Documentación del proyecto
```

## Descripción

### **Servidor**

- El servidor escucha en un puerto específico y acepta conexiones de múltiples clientes.
- Gestiona el **registro de usuarios**, la **transmisión de mensajes** (tanto generales como privados) y el **cambio de estatus**.
- Soporta **desconexión controlada** y **notificaciones** cuando los usuarios se conectan o desconectan.
- Utiliza **multithreading** para manejar múltiples clientes de manera simultánea.

### **Cliente**

- El cliente se conecta al servidor proporcionando su nombre de usuario, la dirección IP del servidor y el puerto.
- Permite a los usuarios **enviar y recibir mensajes**, **ver el estado de otros usuarios** y **cambiar su propio estado**.
- El cliente puede conectarse a un servidor en ejecución en la misma máquina o en la nube.

### **Despliegue**

El servidor puede ser desplegado en una **instancia de Amazon EC2** utilizando el script `deploy.sh`. Esto permite que los clientes se conecten de manera remota a una instancia de servidor en la nube.

### **Pruebas**

Para probar el funcionamiento del servidor y el cliente:
1. **Prueba local**: Puedes ejecutar el servidor y múltiples instancias de clientes en tu máquina local utilizando `telnet` o cualquier cliente de tu preferencia.
2. **Prueba en la nube**: Después de desplegar el servidor en una instancia EC2, podrás probar la comunicación remota entre varios clientes.

## Requisitos

- **gcc** (compilador de C).
- **pthread** (para manejo de multithreading).
- **Amazon Web Services (AWS)** para el despliegue en la nube.

## Instalación y Ejecución

### **Compilación del servidor**

1. Clona el repositorio:

   ```bash
   git clone https://github.com/michellemej22596/Proyecto1-Sistemas-Operativos-Chat.git
   cd Proyecto-Chat
   ```

2. Compila el servidor:

   ```bash
   gcc -o server server/server.c -pthread
   ```

3. Ejecuta el servidor en un puerto de tu elección:

   ```bash
   ./server 8080
   ```

### **Compilación del cliente**

1. Compila el cliente:

   ```bash
   gcc -o client client/client.c
   ```

2. Ejecuta el cliente y conéctate al servidor proporcionando el nombre de usuario, IP del servidor y el puerto:

   ```bash
   ./client <ip-servidor> <puerto>
   ```

   En este caso, si estás ejecutando el servidor en la misma máquina, puedes usar `localhost` como IP.

### **Despliegue en AWS**

1. Crea una instancia EC2 gratuita en AWS (si no tienes una cuenta, sigue la [guía de AWS](https://aws.amazon.com/es/ec2/)).
2. Sube el archivo `server.c` y el script `deploy.sh` a la instancia EC2.
3. Ejecuta el script `deploy.sh` para iniciar el servidor en la instancia EC2:

   ```bash
   ./deploy.sh
   ```

   Esto hará que el servidor se ejecute en la instancia EC2, y los clientes podrán conectarse utilizando la IP pública de la instancia EC2.

### **Pruebas**

Para probar el sistema de chat:

1. **Prueba local**: Conecta varios clientes usando `telnet` en diferentes terminales.

   ```bash
   telnet localhost <puerto>
   ```

   En cada terminal, ingresa un nombre de usuario y empieza a enviar mensajes.

2. **Prueba en la nube**: Después de desplegar el servidor en AWS, conecta los clientes utilizando la **IP pública de la instancia EC2**:

   ```bash
   telnet <IP-Pública-EC2> <puerto>
   ```

### **Funciones principales**

1. **Registro de usuario**: Los usuarios se registran en el servidor con un nombre único.
2. **Manejo de mensajes**: Los usuarios pueden enviar mensajes al chat general o directamente a otros usuarios.
3. **Cambio de estado**: Los usuarios pueden cambiar su estado a `ACTIVO`, `OCUPADO`, o `INACTIVO`.
4. **Desconexión**: Los clientes pueden desconectarse y el servidor notificará a los demás usuarios.
5. **Multithreading**: El servidor maneja múltiples clientes simultáneamente sin bloquear la ejecución de otros clientes.

## Contribuciones

Si deseas contribuir al proyecto, por favor abre un **issue** o **pull request**.

## Licencia

Este proyecto está bajo la Licencia MIT - ver el archivo [LICENSE](LICENSE) para más detalles.

