# **Proyecto de Chat - Sistemas Operativos**

## **Descripción del Proyecto**

Este proyecto tiene como objetivo el desarrollo de una aplicación de chat utilizando **C/C++** con el protocolo **WebSocket**. El sistema está compuesto por un **servidor** que gestiona las conexiones de múltiples **clientes** en tiempo real, permitiendo el envío de mensajes generales y privados, así como el cambio de estado de los usuarios. La comunicación entre el servidor y los clientes se realiza mediante WebSockets, garantizando una interacción eficiente y en tiempo real.

### **Objetivos**
- Implementar un servidor que pueda gestionar múltiples conexiones de clientes concurrentes.
- Desarrollar un cliente con una interfaz gráfica que permita el envío de mensajes, consulta de usuarios, y gestión de estados.
- Usar WebSockets para una comunicación bidireccional en tiempo real.

## **Tecnologías Utilizadas**

- **Lenguaje de Programación**: **C/C++**
- **Protocolo de Comunicación**: **WebSocket**
- **Librerías Requeridas**:
  - **SFML**: Para la creación de la interfaz gráfica del cliente.
  - **OpenSSL**: Para conexiones seguras.
- **Plataforma de Ejecución**: Amazon Web Services (AWS) EC2 para el servidor.
- **Sistema Operativo**: Linux (Ubuntu recomendado).

## **Arquitectura del Sistema**

### **1. Servidor**
- El servidor se encarga de gestionar las conexiones de los clientes y realizar las siguientes acciones:
  - Registrar y liberar usuarios.
  - Actualizar y gestionar el estado de los usuarios.
  - Enviar y recibir mensajes grupales y privados.
  - Mantener el historial de mensajes mientras el servidor esté activo.
  - Asegurarse de que no existan usuarios duplicados, manteniendo la unicidad de los nombres de usuario.
  - Utiliza **multithreading** para manejar múltiples clientes simultáneamente.

### **2. Cliente**
- El cliente se conecta al servidor y permite al usuario interactuar a través de una interfaz gráfica creada con SFML. Las funcionalidades principales del cliente son:
  - Conexión al servidor mediante WebSocket.
  - Enviar y recibir mensajes tanto en un chat general como privado.
  - Cambiar su estado entre **ACTIVO**, **OCUPADO** e **INACTIVO**.
  - Ver el listado de usuarios conectados y consultar información de un usuario específico.
  - Gestionar el historial de mensajes.

## **Flujo de Comunicación**

El flujo de interacción sigue el ciclo de vida del cliente en tres fases principales: **creación de conexión**, **transmisión de mensajes** y **cierre de sesión**.

1. **Creación de Conexión**:
   - El cliente se conecta enviando su nombre de usuario al servidor.
   - Si el nombre es válido y único, el servidor lo registra y asigna el estado **ACTIVO**.
   - Si el nombre ya está en uso, el servidor rechazará la conexión.

2. **Transmisión de Mensajes**:
   - Los mensajes se pueden enviar al **chat general** (visible para todos los usuarios) o **privados** (dirigidos a un usuario específico).
   - El cliente puede cambiar su estado a **ACTIVO**, **OCUPADO** o **INACTIVO** en cualquier momento.

3. **Cierre de Conexión**:
   - Cuando un cliente decide desconectarse, su estado se actualiza a **INACTIVO** y se notifica a los demás usuarios.

## **Protocolo de Comunicación WebSocket**

La comunicación entre el cliente y el servidor sigue un formato binario definido por el protocolo WebSocket, el cual utiliza mensajes con un **código de mensaje (ID)**, **tamaño de los campos** y **datos**. A continuación se describen los tipos de mensajes:

### **Mensajes Cliente a Servidor**
1. **ID 1**: Solicitar el listado de usuarios conectados.
2. **ID 2**: Obtener información de un usuario por su nombre.
3. **ID 3**: Cambiar el estado de un usuario.
4. **ID 4**: Enviar un mensaje a un usuario o al chat general.

### **Mensajes Servidor a Cliente**
1. **ID 50**: Error en la solicitud del cliente.
2. **ID 51**: Respuesta al listado de usuarios.
3. **ID 53**: Notificación de un nuevo usuario registrado.
4. **ID 54**: Notificación de un cambio de estado de usuario.

### **Errores Comunes**
- **Error 1**: El nombre de usuario ya está registrado.
- **Error 2**: El estado enviado es inválido.
- **Error 3**: El mensaje está vacío.
- **Error 4**: El usuario al que se envió el mensaje está desconectado.

## **Instrucciones de Ejecución**

### **Servidor**
1. **Compilación**:
   Abre una terminal en la carpeta del servidor y ejecuta el siguiente comando para compilar el servidor:
   ```bash
   g++ -o servidor servidor.cpp -lssl -lcrypto -lsfml-graphics -lsfml-window -lsfml-system
   ```
   Este comando compilará el servidor, enlazando las bibliotecas necesarias (OpenSSL y SFML).

2. **Ejecución**:
   Para ejecutar el servidor en un puerto específico, utiliza el siguiente comando:
   ```bash
   ./servidor <puerto>
   ```
   Donde `<puerto>` es el número de puerto en el que el servidor escuchará las conexiones entrantes.

3. **Configuración en AWS EC2**:
   - Crea una instancia EC2 gratuita en AWS y configura el puerto del servidor para permitir conexiones externas.
   - Ejecuta el servidor en la instancia EC2 utilizando el comando mencionado.

### **Cliente**
1. **Compilación**:
   Abre una terminal en la carpeta del cliente y ejecuta el siguiente comando para compilar el cliente:
   ```bash
   g++ -o cliente cliente.cpp -lssl -lcrypto -lsfml-graphics -lsfml-window -lsfml-system
   ```
   Este comando compilará el cliente, enlazando las bibliotecas necesarias (OpenSSL y SFML).

2. **Ejecución**:
   Para ejecutar el cliente y conectarse al servidor, utiliza el siguiente comando:
   ```bash
   ./cliente <IP_del_servidor> <puerto> <nombre_de_usuario>
   ```
   Donde:
   - `<IP_del_servidor>` es la dirección IP del servidor.
   - `<puerto>` es el puerto donde el servidor está escuchando.
   - `<nombre_de_usuario>` es el nombre de usuario único para el cliente.

3. **Interfaz Gráfica**:
   - La interfaz gráfica del cliente permite:
     - Enviar mensajes a un chat general o privado.
     - Cambiar el estado del usuario.
     - Ver el listado de usuarios conectados y consultar información detallada sobre ellos.

## **Requisitos del Sistema**
- **Sistema Operativo**: Linux (Ubuntu recomendado)
- **Compilador**: **G++** (GNU C++ Compiler)
- **Librerías**:
  - **SFML**: Para la interfaz gráfica.
  - **OpenSSL**: Si se utiliza conexión segura.
- **Conexión a Internet**: Para la comunicación remota con el servidor.

## **Consideraciones Finales**
- Este proyecto debe ejecutarse en un entorno de servidor remoto (AWS EC2) para facilitar la comunicación entre los clientes de diferentes grupos.

## **Autores**

Este proyecto fue desarrollado por:
- Michelle Mejía
- Diederich Solis
- Silvia Illescas
  
---
