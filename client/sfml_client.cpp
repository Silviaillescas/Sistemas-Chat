#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <iostream>

typedef websocketpp::client<websocketpp::config::asio_client> client;
typedef websocketpp::connection_hdl connection_hdl;

client ws_client;

void on_message(client* c, connection_hdl hdl, client::message_ptr msg) {
    std::cout << "Mensaje recibido: " << msg->get_payload() << std::endl;
}

int main() {
    // Inicializa el cliente WebSocket
    ws_client.init_asio();
    ws_client.set_message_handler(&on_message);
    
    // Conecta al servidor WebSocket
    std::string uri = "ws://localhost:8080";
    websocketpp::lib::error_code ec;
    client::connection_ptr con = ws_client.get_connection(uri, ec);

    if (ec) {
        std::cerr << "Error al conectar al servidor WebSocket: " << ec.message() << std::endl;
        return 1;
    }

    ws_client.connect(con);
    std::thread wsThread([&]() {
        ws_client.run();
    });


    // Configuración de la ventana de SFML
    sf::RenderWindow window(sf::VideoMode(800, 600), "Cliente de Chat");
    sf::Clock actividadClock;  // para medir tiempo desde la última interacción
    bool estaInactivo = false;


    sf::String inputText;
    sf::Text inputTextDisplay;
    sf::Font font;
    
    if (!font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
        std::cerr << "No se pudo cargar la fuente." << std::endl;
        return -1;
    }

    inputTextDisplay.setFont(font);
    inputTextDisplay.setCharacterSize(24);

    // Bucle principal
       while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            else if (event.type == sf::Event::TextEntered) {
                actividadClock.restart();  

                if (estaInactivo) {
                    // ⚡ Cambiar a ACTIVO si estaba inactivo
                    std::vector<char> estadoActivo = {3, 1};  // Código 2, estado 1 (ACTIVO)
                    ws_client.send(con, &estadoActivo[0], estadoActivo.size(), websocketpp::frame::opcode::binary);
                    estaInactivo = false;
                }

                if (event.text.unicode == 13) {  // ENTER
                    std::string message = inputText.toAnsiString();
                   std::vector<char> payload;
                    std::string destinatario = "~"; 

                    payload.push_back(4);  // ID tipo de mensaje
                    payload.push_back(destinatario.size());
                    payload.insert(payload.end(), destinatario.begin(), destinatario.end());
                    payload.push_back(message.size());
                    payload.insert(payload.end(), message.begin(), message.end());

                    ws_client.send(con, payload.data(), payload.size(), websocketpp::frame::opcode::binary);

                    inputText = "";
                }
                else if (event.text.unicode == 8 && inputText.getSize() > 0) {  // BACKSPACE
                    inputText = inputText.substring(0, inputText.getSize() - 1);
                }
                else if (event.text.unicode < 128) {  // Solo texto válido
                    inputText += event.text.unicode;
                }
            }
        }

        // ⏱ Detectar inactividad después de 10 segundos
        if (actividadClock.getElapsedTime().asSeconds() > 10.f && !estaInactivo) {
            std::vector<char> estadoInactivo = {3, 3}; // Código 2, estado 3 (INACTIVO)
            ws_client.send(con, &estadoInactivo[0], estadoInactivo.size(), websocketpp::frame::opcode::binary);
            estaInactivo = true;
        }

        // Dibujar
        window.clear(sf::Color::Black);
        inputTextDisplay.setString(inputText);
        window.draw(inputTextDisplay);
        window.display();
    }


    // Cerrar la conexión WebSocket y salir
        ws_client.stop();
        if (wsThread.joinable()) wsThread.join();

    ws_client.stop();
    return 0;
}