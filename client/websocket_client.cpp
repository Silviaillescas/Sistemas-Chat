#include "websocket_client.hpp"
#include <iostream>
#include <sstream>
#include <random>
#include <openssl/sha.h>
#include <iomanip>
#include <algorithm>
#include <cstring>

WebSocketClient::WebSocketClient(const std::string& url) {
    running = false;

    std::size_t namePos = url.find("?name=");
    if (namePos != std::string::npos) {
        username = url.substr(namePos + 6);
    }

    std::string address = "127.0.0.1";
    unsigned short port = 8080;

    if (socket.connect(address, port) != sf::Socket::Done) {
        std::cerr << "❌ Error al conectar con el servidor." << std::endl;
        return;
    }

    if (!performHandshake(url)) {
        std::cerr << "❌ Falló el handshake WebSocket." << std::endl;
        socket.disconnect();
        return;
    }

    running = true;
    receiverThread = std::thread(&WebSocketClient::receiveLoop, this);

    std::vector<char> listRequest = {1};
    sendBinaryFrame(listRequest);
}

WebSocketClient::~WebSocketClient() {
    running = false;
    socket.disconnect();
    if (receiverThread.joinable())
        receiverThread.join();
}

bool WebSocketClient::performHandshake(const std::string& url) {
    std::string secWebSocketKey = "x3JJHMbDL1EzLkh9GBhXDw==";

    std::ostringstream request;
    request << "GET " << url << " HTTP/1.1\r\n"
            << "Host: localhost\r\n"
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade\r\n"
            << "Sec-WebSocket-Key: " << secWebSocketKey << "\r\n"
            << "Sec-WebSocket-Version: 13\r\n\r\n";

    std::string reqStr = request.str();
    if (socket.send(reqStr.c_str(), reqStr.size()) != sf::Socket::Done)
        return false;

    char buffer[2048];
    std::size_t received = 0;
    if (socket.receive(buffer, sizeof(buffer), received) != sf::Socket::Done)
        return false;

    std::string response(buffer, received);
    return response.find("101 Switching Protocols") != std::string::npos;
}

void WebSocketClient::sendBinaryFrame(const std::vector<char>& data) {
    std::vector<char> frame;
    frame.push_back(0x82);

    size_t len = data.size();
    if (len <= 125) {
        frame.push_back(static_cast<char>(len | 0x80));
    } else if (len <= 65535) {
        frame.push_back(126 | 0x80);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    } else {
        frame.push_back(127 | 0x80);
        for (int i = 7; i >= 0; --i)
            frame.push_back((len >> (8 * i)) & 0xFF);
    }

    char mask[4] = {0x12, 0x34, 0x56, 0x78};
    frame.insert(frame.end(), mask, mask + 4);

    for (size_t i = 0; i < len; ++i)
        frame.push_back(data[i] ^ mask[i % 4]);

    socket.send(frame.data(), frame.size());
}

void WebSocketClient::sendMessage(const std::string& recipient, const std::string& message) {
    std::vector<char> payload;
    payload.push_back(4);
    payload.push_back(recipient.size());
    payload.insert(payload.end(), recipient.begin(), recipient.end());
    payload.push_back(message.size());
    payload.insert(payload.end(), message.begin(), message.end());

    sendBinaryFrame(payload);
}

void WebSocketClient::updateStatus(const std::string& status) {
    std::vector<char> payload;
    payload.push_back(3);

    if (status == "ACTIVO") payload.push_back(1);
    else if (status == "OCUPADO") payload.push_back(2);
    else if (status == "INACTIVO") payload.push_back(3);
    else if (status == "DESCONECTADO") payload.push_back(0);  // ⬅️ ¡Asegúrate de esto!

    sendBinaryFrame(payload);
}

std::vector<std::string> WebSocketClient::getMessages() {
    std::lock_guard<std::mutex> lock(dataMutex);
    return messages;
}

std::vector<std::pair<std::string, uint8_t>> WebSocketClient::getUsers() {
    std::lock_guard<std::mutex> lock(dataMutex);
    return users;
}


bool WebSocketClient::isConnected() const {
    return running;
}

void WebSocketClient::receiveLoop() {
    while (running) {
        char hdr[2];
        std::size_t received;

        if (socket.receive(hdr, 2, received) != sf::Socket::Done || received < 2)
            continue;

        bool fin = hdr[0] & 0x80;
        uint8_t opcode = hdr[0] & 0x0F;
        uint8_t len = hdr[1] & 0x7F;
        bool masked = hdr[1] & 0x80;

        uint64_t payloadLen = len;
        if (len == 126) {
            char ext[2];
            if (socket.receive(ext, 2, received) != sf::Socket::Done) continue;
            payloadLen = (uint8_t(ext[0]) << 8) | uint8_t(ext[1]);
        } else if (len == 127) {
            char ext[8];
            if (socket.receive(ext, 8, received) != sf::Socket::Done) continue;
            payloadLen = 0;
            for (int i = 0; i < 8; ++i)
                payloadLen = (payloadLen << 8) | static_cast<unsigned char>(ext[i]);
        }

        char maskKey[4] = {};
        if (masked) {
            if (socket.receive(maskKey, 4, received) != sf::Socket::Done) continue;
        }

        std::vector<char> payload(payloadLen);
        size_t offset = 0;
        while (offset < payloadLen) {
            if (socket.receive(&payload[offset], payloadLen - offset, received) != sf::Socket::Done)
                break;
            offset += received;
        }

        if (masked) {
            for (uint64_t i = 0; i < payloadLen; ++i)
                payload[i] ^= maskKey[i % 4];
        }

        parseFrame(payload);
    }
}

void WebSocketClient::parseFrame(const std::vector<char>& frameData) {
    if (frameData.empty()) return;

    uint8_t code = static_cast<uint8_t>(frameData[0]);
    std::lock_guard<std::mutex> lock(dataMutex);

    if (code == 50) {
        std::string msg(frameData.begin() + 2, frameData.end());
        messages.push_back("[Error] " + msg);

    } else if (code == 51) {
    users.clear();
    uint8_t count = frameData[1];
    size_t idx = 2;
    for (int i = 0; i < count && idx + 1 < frameData.size(); ++i) {
        uint8_t len = frameData[idx++];
        if (idx + len >= frameData.size()) break;

        std::string name(frameData.begin() + idx, frameData.begin() + idx + len);
        idx += len;

        uint8_t estado = frameData[idx++];

        users.emplace_back(name, estado);
    }
} else if (code == 53) {
    uint8_t len = frameData[1];
    std::string name(frameData.begin() + 2, frameData.begin() + 2 + len);
    
    // Evita sobrescribir si ya existe como DESCONECTADO
    bool yaExiste = std::any_of(users.begin(), users.end(), 
        [&name](const auto& u) { return u.first == name; });

    if (!yaExiste) {
        users.emplace_back(name, 1);  // ACTIVO solo si no existía
    }

    messages.push_back("[Nuevo] Usuario " + name + " se ha conectado");
}

 else if (code == 54) {
    uint8_t len = frameData[1];
    std::string name(frameData.begin() + 2, frameData.begin() + 2 + len);
    uint8_t newStatus = frameData[2 + len];

    bool found = false;
    for (auto& user : users) {
        if (user.first == name) {
            user.second = newStatus;  // ✅ Actualiza el estado
            found = true;
            break;
        }
    }

    if (!found) {
        users.emplace_back(name, newStatus);  // Si no existe, lo agrega
    }

    std::string estadoTexto;
    if (newStatus == 0) estadoTexto = "se ha DESCONECTADO";
    else if (newStatus == 1) estadoTexto = "ahora esta ACTIVO";
    else if (newStatus == 2) estadoTexto = "ahora esta OCUPADO";
    else if (newStatus == 3) estadoTexto = "ahora esta INACTIVO";
    else estadoTexto = "tiene un estado desconocido";

    messages.push_back("[Estado] " + name + " " + estadoTexto);
}  else if (code == 55) {
        uint8_t nameLen = frameData[1];
        std::string sender(frameData.begin() + 2, frameData.begin() + 2 + nameLen);
        uint8_t msgLen = frameData[2 + nameLen];
        std::string msg(frameData.begin() + 3 + nameLen, frameData.begin() + 3 + nameLen + msgLen);
        messages.push_back(sender + ": " + msg);

    } else if (code == 56) {
        uint8_t count = frameData[1];
        size_t idx = 2;
        for (int i = 0; i < count && idx < frameData.size(); ++i) {
            uint8_t len = frameData[idx++];
            std::string msg(frameData.begin() + idx, frameData.begin() + idx + len);
            messages.push_back("[Historial] " + msg);
            idx += len;
        }

    } else {
        messages.push_back("[Info] Mensaje recibido (código desconocido): " + std::to_string(code));
    }
}

void WebSocketClient::close() {
    running = false;
    socket.disconnect();
    if (receiverThread.joinable())
        receiverThread.join();
}

