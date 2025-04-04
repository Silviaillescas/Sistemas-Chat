#include <SFML/Network.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <set>
#include <map>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <arpa/inet.h>
#include <openssl/sha.h>

struct ClientInfo {
    sf::TcpSocket* socket = nullptr;
    uint8_t status = 0;
    std::chrono::steady_clock::time_point ultimaActividad;

    // Constructor por defecto necesario para operator[]
    ClientInfo() : socket(nullptr), status(0), ultimaActividad(std::chrono::steady_clock::now()) {}

    // Constructor personalizado
    ClientInfo(sf::TcpSocket* s, uint8_t st)
        : socket(s), status(st), ultimaActividad(std::chrono::steady_clock::now()) {}
};



std::unordered_map<std::string, ClientInfo> clientes;
std::set<std::string> todosUsuarios;
std::map<std::string, std::vector<std::string>> historial;
std::vector<std::string> historialGlobal;  
std::mutex usersMutex;
sf::TcpListener listener;
bool serverRunning = true;

const std::string WEBSOCKET_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

std::string base64Encode(const std::vector<unsigned char>& data); // definida como antes
std::vector<unsigned char> base64Decode(const std::string& input); // definida como antes
bool enviarFrame(sf::TcpSocket* socket, const std::vector<char>& data); // definida como antes

void registrarLog(const std::string& evento) {
    std::ofstream log("registro.log", std::ios::app);
    std::time_t ahora = std::time(nullptr);
    log << std::ctime(&ahora) << " - " << evento << "\n";
}

void enviarError(sf::TcpSocket* socket, const std::string& mensaje) {
    std::vector<char> err;
    err.push_back(50);
    err.push_back(mensaje.size());
    err.insert(err.end(), mensaje.begin(), mensaje.end());
    enviarFrame(socket, err);
}

void responderInfoUsuario(sf::TcpSocket* socket, const std::string& nombre) {
    std::vector<char> respuesta;
    respuesta.push_back(52);
    respuesta.push_back(nombre.size());
    respuesta.insert(respuesta.end(), nombre.begin(), nombre.end());
    {
        std::lock_guard<std::mutex> lock(usersMutex);
        auto it = clientes.find(nombre);
        uint8_t estado = (it != clientes.end()) ? it->second.status : 0;
        respuesta.push_back(estado);
    }
    enviarFrame(socket, respuesta);
}

void enviarHistorial(sf::TcpSocket* socket, const std::string& nombre) {
    std::vector<char> frame;
    frame.push_back(56);
    std::lock_guard<std::mutex> lock(usersMutex);
    auto& mensajes = historial[nombre];
    frame.push_back(mensajes.size());
    for (auto& m : mensajes) {
        frame.push_back(m.size());
        frame.insert(frame.end(), m.begin(), m.end());
    }
    enviarFrame(socket, frame);
}

void manejarMensajePrivado(const std::string& origen, const std::vector<char>& payload) {
    if (payload.size() < 4) return;
    std::string destino(payload.begin() + 2, payload.begin() + 2 + payload[1]);
    uint8_t nombreLen = payload[1];
    uint8_t mensajeLen = payload[2 + nombreLen];
    std::string mensaje(payload.begin() + 3 + nombreLen, payload.begin() + 3 + nombreLen + mensajeLen);
    std::lock_guard<std::mutex> lock(usersMutex);
    if (clientes.find(destino) == clientes.end()) return;
    std::vector<char> respuesta;
    respuesta.push_back(55);
    respuesta.push_back(payload[1]);
    respuesta.insert(respuesta.end(), destino.begin(), destino.end());
    respuesta.push_back(mensaje.size());
    respuesta.insert(respuesta.end(), mensaje.begin(), mensaje.end());
    auto it = clientes.find(destino);
    if (it != clientes.end()) {
        enviarFrame(it->second.socket, respuesta);
    }

        historial[destino].push_back(origen + ": " + mensaje);
    }

void cambiarEstado(const std::string& nombreUsuario, uint8_t nuevoEstado) {
    std::lock_guard<std::mutex> lock(usersMutex);
    auto it = clientes.find(nombreUsuario);
    if (it != clientes.end()) {
        it->second.status = nuevoEstado;
        std::vector<char> notif;
        notif.push_back(54);
        notif.push_back(nombreUsuario.size());
        notif.insert(notif.end(), nombreUsuario.begin(), nombreUsuario.end());
        notif.push_back(nuevoEstado);
        for (auto& [nombre, cliente] : clientes) {
            enviarFrame(cliente.socket, notif);  
        }
    }
}
// Función para codificar datos en Base64
std::string base64Encode(const std::vector<unsigned char>& data) {
    static const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve(((data.size() + 2) / 3) * 4);
    unsigned int val = 0;
    int valb = -6;
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64Chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        encoded.push_back(base64Chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (encoded.size() % 4) {
        encoded.push_back('=');
    }
    return encoded;
}

// Función para decodificar una cadena Base64 a bytes
std::vector<unsigned char> base64Decode(const std::string& input) {
    static const std::string base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<int> index(256, -1);
    for (int i = 0; i < 64; ++i) {
        index[base64Chars[i]] = i;
    }
    std::vector<unsigned char> output;
    int val = 0;
    int valb = -8;
    for (unsigned char c : input) {
        if (index[c] == -1) break;
        val = (val << 6) + index[c];
        valb += 6;
        if (valb >= 0) {
            output.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return output;
}

// Función para enviar un frame WebSocket a un cliente (sin máscara)
bool enviarFrame(sf::TcpSocket* socket, const std::vector<char>& data) {
    std::vector<char> frame;
    uint64_t dataLen = data.size();
    frame.push_back(0x82); // Primer byte: FIN=1, opcode=2
    if (dataLen <= 125) {
        frame.push_back(static_cast<char>(dataLen));
    } else if (dataLen <= 0xFFFF) {
        frame.push_back(126);
        uint16_t len16 = htons(static_cast<uint16_t>(dataLen));
        frame.insert(frame.end(), reinterpret_cast<char*>(&len16), reinterpret_cast<char*>(&len16) + 2);
    } else {
        frame.push_back(127);
        uint32_t high = htonl(static_cast<uint32_t>(dataLen >> 32));
        uint32_t low = htonl(static_cast<uint32_t>(dataLen & 0xFFFFFFFF));
        uint64_t netLen = ((uint64_t)low << 32) | high;
        frame.insert(frame.end(), reinterpret_cast<char*>(&netLen), reinterpret_cast<char*>(&netLen) + 8);
    }
    frame.insert(frame.end(), data.begin(), data.end());
    sf::Socket::Status status = socket->send(frame.data(), frame.size());
    return (status == sf::Socket::Done);
}

// Función que maneja la comunicación con un cliente (se ejecuta en un thread por cliente)
// Función que maneja la comunicación con un cliente (se ejecuta en un thread por cliente)
void atenderCliente(sf::TcpSocket* socket) {
    sf::IpAddress clientIp = socket->getRemoteAddress();
    unsigned short clientPort = socket->getRemotePort();
    std::string nombreUsuario;
    uint8_t miEstado = 1;

    std::vector<char> notif;
    std::vector<sf::TcpSocket*> recipients;

    // Leer y procesar el handshake HTTP inicial del cliente
    char buffer[1024];
    std::string request = "";
    std::size_t received;
    sf::Socket::Status status;

    while (true) {
        status = socket->receive(buffer, sizeof(buffer) - 1, received);
        if (status != sf::Socket::Done && status != sf::Socket::Partial) {
            std::cerr << "Conexión cerrada antes de completar handshake (" 
                      << clientIp.toString() << ":" << clientPort << ")\n";
            delete socket;
            return;
        }
        if (received > 0) {
            buffer[received] = '\0';
            request += buffer;
            if (request.find("\r\n\r\n") != std::string::npos) break;
        }
        if (status == sf::Socket::Done && received == 0) {
            delete socket;
            return;
        }
    }

    std::size_t posGet = request.find("GET ");
    std::size_t posHTTP = request.find(" HTTP/1.1");
    if (posGet == std::string::npos || posHTTP == std::string::npos) {
        const char* badReq = "HTTP/1.1 400 Bad Request\r\n\r\n";
        socket->send(badReq, std::strlen(badReq));
        socket->disconnect();
        delete socket;
        return;
    }
    std::string getLine = request.substr(posGet, posHTTP - posGet);
    std::size_t posName = getLine.find("?name=");
    if (posName == std::string::npos) {
        const char* badReq = "HTTP/1.1 400 Bad Request\r\n\r\n";
        socket->send(badReq, std::strlen(badReq));
        socket->disconnect();
        delete socket;
        return;
    }
    posName += 6;  
    nombreUsuario = getLine.substr(posName);
    if (!nombreUsuario.empty()) {
        std::size_t posSpace = nombreUsuario.find(" ");
        if (posSpace != std::string::npos) {
            nombreUsuario = nombreUsuario.substr(0, posSpace);
        }
    }
    if (nombreUsuario.empty() || nombreUsuario == "~") {
        const char* badReq = "HTTP/1.1 400 Bad Request\r\n\r\n";
        socket->send(badReq, std::strlen(badReq));
        socket->disconnect();
        delete socket;
        return;
    }

    // Extraer la clave 'Sec-WebSocket-Key' del encabezado
    std::string secKey;
    std::size_t keyPos = request.find("Sec-WebSocket-Key:");
    if (keyPos != std::string::npos) {
        std::size_t keyEnd = request.find("\r\n", keyPos);
        std::string keyLine = request.substr(keyPos, keyEnd - keyPos);
        std::size_t colonPos = keyLine.find(":");
        if (colonPos != std::string::npos) {
            secKey = keyLine.substr(colonPos + 1);
            while (!secKey.empty() && (secKey.front() == ' ' || secKey.front() == '\t')) {
                secKey.erase(secKey.begin());
            }
        }
    }
    if (secKey.empty()) {
        const char* badReq = "HTTP/1.1 400 Bad Request\r\n\r\n";
        socket->send(badReq, std::strlen(badReq));
        socket->disconnect();
        delete socket;
        return;
    }

    bool nuevoUsuario = false;
    {
        std::lock_guard<std::mutex> lock(usersMutex);
        if (clientes.find(nombreUsuario) != clientes.end()) {
            const char* conflict = "HTTP/1.1 400 Bad Request\r\n\r\n";
            socket->send(conflict, std::strlen(conflict));
            socket->disconnect();
            delete socket;
            return;
        }
        nuevoUsuario = (todosUsuarios.count(nombreUsuario) == 0);
        todosUsuarios.insert(nombreUsuario);
        clientes[nombreUsuario] = ClientInfo(socket, 1);
    }

    // Generar el Sec-WebSocket-Accept
    std::vector<unsigned char> keyBytes = base64Decode(secKey);
    for (char c : WEBSOCKET_GUID) {
        keyBytes.push_back(static_cast<unsigned char>(c));
    }
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(keyBytes.data(), keyBytes.size(), hash);
    std::vector<unsigned char> hashVec(hash, hash + SHA_DIGEST_LENGTH);
    std::string acceptKey = base64Encode(hashVec);

    // Enviar la respuesta de handshake
    std::string handshakeResponse =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
    status = socket->send(handshakeResponse.c_str(), handshakeResponse.size());
    if (status != sf::Socket::Done) {
        std::lock_guard<std::mutex> lock(usersMutex);
        clientes.erase(nombreUsuario);
        if (nuevoUsuario) {
            todosUsuarios.erase(nombreUsuario);
        }
        std::cerr << "Error enviando respuesta de handshake a " << nombreUsuario << "\n";
        socket->disconnect();
        delete socket;
        return;
    }
    std::cout << "Usuario \"" << nombreUsuario << "\" conectado (" 
              << clientIp.toString() << ":" << clientPort << ")\n";

              
    // Notificar a los demás usuarios que este usuario se ha conectado
    notif.push_back(nuevoUsuario ? 53 : 54);
    uint8_t nameLen = nombreUsuario.size();
    notif.push_back(static_cast<char>(nameLen));
    notif.insert(notif.end(), nombreUsuario.begin(), nombreUsuario.end());
    uint8_t statusByte = 1;  // ACTIVO
    notif.push_back(static_cast<char>(statusByte));

    for (auto& par : clientes) {
        if (par.first != nombreUsuario) {
            recipients.push_back(par.second.socket);
        }
    }

    // Enviar la notificación a todos los destinatarios
    for (sf::TcpSocket* sock : recipients) {
        enviarFrame(sock, notif);
    }

    // Ciclo para recibir mensajes del cliente
    while (serverRunning) {
        // Leer encabezado de frame (2 bytes)
        char hdr[2];
        std::size_t hdrReceived;
        status = socket->receive(hdr, 2, hdrReceived);
        if (status == sf::Socket::Disconnected || status == sf::Socket::Error) {
            break;
        }
        if (status == sf::Socket::Partial) {
            continue;
        }
        if (hdrReceived < 2) {
            continue;
        }
        {
        std::lock_guard<std::mutex> lock(usersMutex);
        auto it = clientes.find(nombreUsuario);
        if (it != clientes.end()) {
            it->second.ultimaActividad = std::chrono::steady_clock::now();  // ⏱ actualizar
        }
        }

        bool fin = (hdr[0] & 0x80) != 0;
        uint8_t opcode = hdr[0] & 0x0F;
        bool masked = (hdr[1] & 0x80) != 0;  // (debe ser 1 para frames de cliente)
        uint64_t payloadLen = hdr[1] & 0x7F;
        if (payloadLen == 126) {
            char ext[2];
            socket->receive(ext, 2, hdrReceived);
            if (hdrReceived < 2) continue;
            uint16_t len16;
            std::memcpy(&len16, ext, 2);
            payloadLen = ntohs(len16);
        } else if (payloadLen == 127) {
            char ext[8];
            socket->receive(ext, 8, hdrReceived);
            if (hdrReceived < 8) continue;
            uint64_t len64;
            std::memcpy(&len64, ext, 8);
            uint64_t high_part = ntohl(static_cast<uint32_t>(len64 >> 32));
            uint64_t low_part = ntohl(static_cast<uint32_t>(len64 & 0xFFFFFFFF));
            payloadLen = (low_part << 32) | high_part;
        }
        uint8_t maskKey[4] = {0,0,0,0};
        if (masked) {
            char maskBytes[4];
            socket->receive(maskBytes, 4, hdrReceived);
            if (hdrReceived < 4) continue;
            std::memcpy(maskKey, maskBytes, 4);
        }
        std::vector<char> payload;
        payload.resize(payloadLen);
        std::size_t offset = 0;
        while (offset < payloadLen) {
            status = socket->receive(payload.data() + offset, payloadLen - offset, hdrReceived);
            if (status == sf::Socket::Disconnected || status == sf::Socket::Error) {
                offset = 0;
                break;
            }
            if (status != sf::Socket::Done && status != sf::Socket::Partial) {
                break;
            }
            offset += hdrReceived;
        }
        if (offset < payloadLen) {
            break;
        }
        if (masked) {
            for (uint64_t i = 0; i < payloadLen; ++i) {
                payload[i] ^= maskKey[i % 4];
            }
        }
        if (!fin) {
            continue;  // no soporta frames fragmentados 
        }
        if (opcode == 0x8) {  // frame de cierre de cliente
            break;
        }
        {
            std::lock_guard<std::mutex> lock(usersMutex);
            clientes[nombreUsuario].ultimaActividad = std::chrono::steady_clock::now();
        }

        if (opcode != 0x1 && opcode != 0x2) {
            continue;  // ignorar frames que no sean texto/binario
        }
        if (payload.empty()) continue;
        uint8_t codigo = static_cast<uint8_t>(payload[0]);
        if (codigo == 1) {
                // Listar usuarios conectados actualmente
            std::vector<char> respuesta;
            respuesta.push_back(51);
            std::vector<std::pair<std::string, uint8_t>> lista;

            {
                std::lock_guard<std::mutex> lock(usersMutex);
                for (const auto& nombre : todosUsuarios) {
                    uint8_t stat = 0;
                    auto it = clientes.find(nombre);
                    if (it != clientes.end()) {
                        stat = it->second.status;
                    }
                    lista.emplace_back(nombre, stat);
                }
            }

            uint8_t numUsuarios = lista.size();
            respuesta.push_back(static_cast<char>(numUsuarios));
            for (auto& [nombre, stat] : lista) {
                uint8_t nameLen = nombre.size();
                respuesta.push_back(static_cast<char>(nameLen));
                respuesta.insert(respuesta.end(), nombre.begin(), nombre.end());
                respuesta.push_back(static_cast<char>(stat));
            }

            enviarFrame(socket, respuesta);  // ✅ ya se mando el código 51

           
            // Enviar historial grupal si existe
            if (!historialGlobal.empty()) {
                std::vector<char> frame = {56, static_cast<char>(historialGlobal.size())};
                for (const auto& msg : historialGlobal) {
                    frame.push_back(static_cast<char>(msg.size()));
                    frame.insert(frame.end(), msg.begin(), msg.end());
                }
                enviarFrame(socket, frame);  // ✅ código 56
            }
            
        
        } else if (codigo == 3) {
            // Cambio de estado
            if (payload.size() >= 2 && payload[1] >= 1 && payload[1] <= 3) {
                uint8_t nuevoEstado = payload[1];
                cambiarEstado(nombreUsuario, nuevoEstado);
                std::cout << "[Cambio de estado] " << nombreUsuario << " ahora está ";
                if (nuevoEstado == 1) std::cout << "ACTIVO\n";
                else if (nuevoEstado == 2) std::cout << "OCUPADO\n";
                else if (nuevoEstado == 3) std::cout << "INACTIVO\n";
            }
        
        }else if (codigo == 5 && payload.size() == 1) {
            // ✅ Solo historial grupal
            if (!historialGlobal.empty()) {
                std::vector<char> frame = {56, static_cast<char>(historialGlobal.size())};
                for (const auto& msg : historialGlobal) {
                    frame.push_back(static_cast<char>(msg.size()));
                    frame.insert(frame.end(), msg.begin(), msg.end());
                }
                enviarFrame(socket, frame);
            }
        }
        else if (codigo == 4) {
            //  Ahora esto es para enviar mensaje

            if (payload.size() < 4) return;

            uint8_t nombreLen = payload[1];
            std::string destinatario(payload.begin() + 2, payload.begin() + 2 + nombreLen);
            uint8_t mensajeLen = payload[2 + nombreLen];
            std::string mensaje(payload.begin() + 3 + nombreLen, payload.begin() + 3 + nombreLen + mensajeLen);

            std::cout << "➡️  Recibido mensaje: de '" << nombreUsuario
                    << "' para '" << destinatario 
                    << "' (len=" << (int)nombreLen << "): " << mensaje << "\n";

            if (destinatario == "~") {
                std::string completo = nombreUsuario + ": " + mensaje;
                historialGlobal.push_back(completo);

                std::vector<char> frame = {55, static_cast<char>(nombreUsuario.size())};
                frame.insert(frame.end(), nombreUsuario.begin(), nombreUsuario.end());
                frame.push_back(static_cast<char>(mensaje.size()));
                frame.insert(frame.end(), mensaje.begin(), mensaje.end());

                {
                    std::lock_guard<std::mutex> lock(usersMutex);
                    for (auto& [otroNombre, info] : clientes) {
                        enviarFrame(info.socket, frame);
                    }
                }
                std::cout << "[Mensaje General] " << completo << "\n";
            } else {
                manejarMensajePrivado(nombreUsuario, payload);
                std::cout << "[Privado] " << nombreUsuario << " → " << destinatario << ": " << mensaje << "\n";
            }
        }

        else {
            // Mensaje general (difusión)
            std::string msg(payload.begin() + 1, payload.end());
            std::lock_guard<std::mutex> lock(usersMutex);
            for (auto& [otroNombre, info] : clientes) {
                if (otroNombre != nombreUsuario) {
                    std::vector<char> frame;
                    frame.push_back(55); // código mensaje
                    frame.push_back(nombreUsuario.size());
                    frame.insert(frame.end(), nombreUsuario.begin(), nombreUsuario.end());
                    frame.push_back(msg.size());
                    frame.insert(frame.end(), msg.begin(), msg.end());
                    enviarFrame(info.socket, frame);
                    historial[otroNombre].push_back(nombreUsuario + ": " + msg);
                }
            }
            std::cout << "[Mensaje] " << nombreUsuario << ": " << msg << "\n";
        }
    }

    // Salir del bucle: el cliente cerró la conexión
    socket->disconnect();

    // 🟡 Actualizar estado a INACTIVO antes de borrar
    {
        std::lock_guard<std::mutex> lock(usersMutex);
        auto it = clientes.find(nombreUsuario);
        if (it != clientes.end()) {
            it->second.status = 0;  // Estado 0 = INACTIVO
        }
    }

    // 🔴 Ahora eliminamos
    {
        std::lock_guard<std::mutex> lock(usersMutex);
        clientes.erase(nombreUsuario);
    }


    notif.clear();  // 👈 IMPORTANTE
    notif.push_back(54);
    notif.push_back(static_cast<char>(nombreUsuario.size()));
    notif.insert(notif.end(), nombreUsuario.begin(), nombreUsuario.end());
    notif.push_back(0);
    recipients.clear();
    {
        std::lock_guard<std::mutex> lock(usersMutex);
        for (auto& par : clientes) {
            recipients.push_back(par.second.socket);
        }
    }
    for (sf::TcpSocket* sock : recipients) {
        enviarFrame(sock, notif);
    }
    std::cout << "Usuario \"" << nombreUsuario << "\" desconectado\n";
    delete socket;
}


void controlInactividad() {
    while (serverRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto ahora = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(usersMutex);
        for (auto& [nombre, info] : clientes) {
            if (info.status != 3) {
                auto tiempoInactivo = std::chrono::duration_cast<std::chrono::seconds>(ahora - info.ultimaActividad).count();
                if (tiempoInactivo >= 60) {
                    info.status = 3;
                    std::vector<char> notif = {54, static_cast<char>(nombre.size())};
                    notif.insert(notif.end(), nombre.begin(), nombre.end());
                    notif.push_back(3);
                    for (auto& [_, cliente] : clientes) {
                        enviarFrame(cliente.socket, notif);
                    }
                    std::cout << "[Inactividad] " << nombre << " marcado como INACTIVO\n";
                }
            }
        }
    }
}
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " <puerto>\n";
        return 1;
    }

    unsigned short puerto = std::atoi(argv[1]);
    if (listener.listen(puerto) != sf::Socket::Done) {
        std::cerr << "Error: no se pudo iniciar el servidor en el puerto " << puerto << "\n";
        return 1;
    }

    std::cout << "Servidor escuchando en el puerto " << puerto << "...\n";

    std::thread hiloInactividad(controlInactividad);

    while (serverRunning) {
        sf::TcpSocket* nuevo = new sf::TcpSocket();
        if (listener.accept(*nuevo) == sf::Socket::Done) {
            std::thread(atenderCliente, nuevo).detach();
        } else {
            delete nuevo;
        }
    }

    hiloInactividad.join();
    return 0;
}