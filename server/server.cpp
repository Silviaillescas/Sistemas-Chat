#include <libwebsockets.h>
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
#include <openssl/sha.h>

struct ClientInfo {
    struct lws *wsi = nullptr;  // WebSocket instance
    uint8_t status = 0;  // Estado del usuario
    std::chrono::steady_clock::time_point ultimaActividad;

    ClientInfo() : wsi(nullptr), status(0), ultimaActividad(std::chrono::steady_clock::now()) {}
    ClientInfo(struct lws *w, uint8_t st) : wsi(w), status(st), ultimaActividad(std::chrono::steady_clock::now()) {}
};

std::unordered_map<std::string, ClientInfo> clientes;  // Mapa de clientes conectados
std::set<std::string> todosUsuarios;  // Todos los usuarios conectados
std::map<std::string, std::vector<std::string>> historial;  // Historial de mensajes por usuario
std::vector<std::string> historialGlobal;  // Historial de mensajes global
std::mutex usersMutex;  // Mutex para proteger el acceso a los datos
bool serverRunning = true;

const std::string WEBSOCKET_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";  // GUID para WebSocket

// Funciones necesarias
std::string base64Encode(const std::vector<unsigned char>& data);
std::vector<unsigned char> base64Decode(const std::string& input);
bool enviarFrame(struct lws *wsi, const std::vector<char>& data);
void registrarLog(const std::string& evento);

// Función para codificar en Base64
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

// Función para decodificar Base64
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

// Función para enviar un frame WebSocket a un cliente
bool enviarFrame(struct lws *wsi, const std::vector<char>& data) {
    unsigned char *buffer = new unsigned char[LWS_PRE + data.size()];
    memcpy(buffer + LWS_PRE, data.data(), data.size());

    int bytesWritten = lws_write(wsi, buffer + LWS_PRE, data.size(), LWS_WRITE_BINARY);
    delete[] buffer;

    return bytesWritten == data.size();
}

// Manejador de eventos de WebSocket
static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            // El cliente se conecta
            std::cout << "Nuevo cliente conectado." << std::endl;
            break;
        }

        case LWS_CALLBACK_RECEIVE: {
            unsigned char *data = (unsigned char*)in;
            uint8_t messagetype = data[0];

            switch (messagetype) {
                case 1: {
                    // Listar usuarios
                    std::cout << "Listando usuarios conectados." << std::endl;
                    break;
                }
                case 2: {
                    // Obtener información de usuario
                    std::cout << "Obteniendo información de un usuario." << std::endl;
                    break;
                }
                case 3: {
                    // Cambiar estado
                    std::cout << "Cambio de estado." << std::endl;
                    break;
                }
                case 4: {
                    // Enviar mensaje privado
                    std::cout << "Enviando mensaje privado." << std::endl;
                    break;
                }
                default:
                    break;
            }
            break;
        }

        case LWS_CALLBACK_CLOSED: {
            // El cliente desconecta
            std::cout << "Conexión cerrada." << std::endl;
            break;
        }

        default:
            break;
    }

    return 0;
}

static struct lws_protocols protocols[] = {
    { "ws-protocol", ws_callback, 0, 1024 },  // Protocolo WebSocket
    { NULL, NULL, 0, 0 }
};

// Función para gestionar los mensajes privados
void enviarMensajePrivado(const std::string& destinatario, const std::string& mensaje) {
    std::lock_guard<std::mutex> lock(usersMutex);
    auto it = clientes.find(destinatario);
    if (it != clientes.end()) {
        std::vector<char> frame;
        frame.push_back(55);  // Código para mensaje privado
        frame.push_back(destinatario.size());
        frame.insert(frame.end(), destinatario.begin(), destinatario.end());
        frame.push_back(mensaje.size());
        frame.insert(frame.end(), mensaje.begin(), mensaje.end());
        enviarFrame(it->second.wsi, frame);
        std::cout << "Mensaje privado enviado a " << destinatario << ": " << mensaje << std::endl;
    } else {
        std::cout << "Error: El destinatario " << destinatario << " no está conectado." << std::endl;
    }
}

// Función para cambiar el estado de un usuario
void cambiarEstado(const std::string& nombreUsuario, uint8_t nuevoEstado) {
    std::lock_guard<std::mutex> lock(usersMutex);
    auto it = clientes.find(nombreUsuario);
    if (it != clientes.end()) {
        it->second.status = nuevoEstado;
        std::cout << nombreUsuario << " ha cambiado su estado a " << nuevoEstado << std::endl;

        // Notificar a todos los usuarios sobre el cambio de estado
        std::vector<char> notif;
        notif.push_back(54);  // Código para cambio de estado
        notif.push_back(nombreUsuario.size());
        notif.insert(notif.end(), nombreUsuario.begin(), nombreUsuario.end());
        notif.push_back(nuevoEstado);

        // Enviar la notificación a todos los usuarios
        for (auto& [_, cliente] : clientes) {
            enviarFrame(cliente.wsi, notif);
        }
    }
}

// Función para iniciar múltiples hilos de servicio
void startServer(int port) {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.timeout_secs = 0;
    info.port = port;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
    info.count_threads = 4;  // Usamos múltiples hilos

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        std::cerr << "Error creando el contexto WebSocket." << std::endl;
        return;
    }

    std::cout << "Servidor WebSocket iniciado en el puerto " << port << std::endl;

    // Aquí puedes usar múltiples hilos para procesar las conexiones
    while (true) {
        lws_service(context, 0);  // Este ciclo procesa las conexiones WebSocket
    }

    lws_context_destroy(context);
}

int main() {
    const int PORT = 8080;  // Puerto en el que escuchará el servidor
    startServer(PORT);
    return 0;
}
