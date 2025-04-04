#ifndef WEBSOCKET_CLIENT_HPP
#define WEBSOCKET_CLIENT_HPP

#include <SFML/Network.hpp>
#include <vector>
#include <string>
#include <thread>
#include <mutex>

class WebSocketClient {
public:
    WebSocketClient(const std::string& url);
    ~WebSocketClient();

    void sendUserName(const std::string& username);
    void sendMessage(const std::string& recipient, const std::string& message);
    void updateStatus(const std::string& status);
    std::vector<std::string> getMessages();
    std::vector<std::pair<std::string, uint8_t>> getUsers();
    bool isConnected() const;
    
    void sendBinaryFrame(const std::vector<char>& data);  // ✅ Añadido

    void close();  // Método para cerrar conexión

private:
    sf::TcpSocket socket;
    std::vector<std::string> messages;
    std::vector<std::pair<std::string, uint8_t>> users;
    std::string username;
    bool running;

    std::thread receiverThread;
    std::mutex dataMutex;

    bool performHandshake(const std::string& url);
    void receiveLoop();
    void parseFrame(const std::vector<char>&);
};

#endif
