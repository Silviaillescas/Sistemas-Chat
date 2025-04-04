#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include "websocket_client.hpp"

sf::Font loadFont() {
    sf::Font font;

#ifdef __APPLE__
    const std::string fontPath = "/System/Library/Fonts/Supplemental/Arial.ttf";
#else
    const std::string fontPath = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
#endif

    if (!font.loadFromFile(fontPath)) {
        std::cerr << "⚠ No se pudo cargar la fuente desde: " << fontPath << std::endl;
        exit(1);
    }

    return font;
}

int main() {
    std::string username;
    std::cout << "Introduce tu nombre de usuario: ";
    std::cin >> username;

    std::string url = "/?name=" + username;
    WebSocketClient wsClient(url);

    if (!wsClient.isConnected()) {
        std::cerr << "❌ No se pudo establecer conexión con el servidor." << std::endl;
        return 1;
    }

    std::string destinatarioActual = "~";  

    sf::RenderWindow window(sf::VideoMode(900, 600), "Chat Cliente", sf::Style::Titlebar | sf::Style::Close);
    sf::Font font = loadFont();

    sf::Text userNameText("Usuario: " + username, font, 18);
    userNameText.setPosition(60, 20);
    userNameText.setFillColor(sf::Color(0, 200, 255));

    sf::Text ocultoText;
    ocultoText.setFont(font);
    ocultoText.setCharacterSize(16);
    ocultoText.setFillColor(sf::Color(200, 200, 100));
    ocultoText.setPosition(30, 460);


    sf::RectangleShape chatBox({600, 400});
    chatBox.setPosition(20, 50);
    chatBox.setFillColor(sf::Color(45, 45, 45));
    chatBox.setOutlineThickness(2);
    chatBox.setOutlineColor(sf::Color(80, 80, 80));

    sf::RectangleShape usersBox({250, 400});
    usersBox.setPosition(630, 50);
    usersBox.setFillColor(sf::Color(35, 35, 35));
    usersBox.setOutlineThickness(2);
    usersBox.setOutlineColor(sf::Color(80, 80, 80));

    sf::Text usersTitle("Usuarios", font, 22);
    usersTitle.setPosition(640, 20);
    usersTitle.setFillColor(sf::Color(0, 180, 255));

    sf::RectangleShape generalChatButton({250, 40});
    generalChatButton.setPosition(630, 400);
    generalChatButton.setFillColor(sf::Color(0, 130, 200));

    sf::Text generalChatText(" Chat General", font, 20);
    generalChatText.setPosition(640, 408);
    generalChatText.setFillColor(sf::Color::White);


    sf::RectangleShape inputBox({700, 50});
    inputBox.setPosition(20, 500);
    inputBox.setFillColor(sf::Color(60, 60, 60));
    inputBox.setOutlineThickness(2);
    inputBox.setOutlineColor(sf::Color(100, 100, 100));

    sf::Text inputText("", font, 24);
    inputText.setPosition(30, 515);
    inputText.setFillColor(sf::Color::White);

    sf::Text recipientText("Enviando a: Todos", font, 18);
    recipientText.setPosition(30, 480);
    recipientText.setFillColor(sf::Color::Cyan);

    sf::Text chatMessages("", font, 18);
    chatMessages.setPosition(30, 60);
    chatMessages.setFillColor(sf::Color::White);

    sf::Text userListText("", font, 18);
    userListText.setPosition(640, 60);
    userListText.setFillColor(sf::Color::Green);

    sf::RectangleShape sendButton({160, 50});
    sendButton.setPosition(730, 500);
    sendButton.setFillColor(sf::Color(0, 180, 255));
    sf::Text sendButtonText("Enviar", font, 22);
    sendButtonText.setPosition(770, 515);
    sendButtonText.setFillColor(sf::Color::White);

    sf::RectangleShape historyButton({160, 40});
    historyButton.setPosition(730, 450);
    historyButton.setFillColor(sf::Color(100, 100, 255));

    sf::Text historyButtonText("Historial", font, 20);
    historyButtonText.setPosition(745, 458);
    historyButtonText.setFillColor(sf::Color::White);


    sf::RectangleShape closeButton({40, 30});
    closeButton.setPosition(10, 10);
    closeButton.setFillColor(sf::Color(150, 50, 50));
    sf::Text closeButtonText("X", font, 18);
    closeButtonText.setPosition(20, 15);
    closeButtonText.setFillColor(sf::Color::White);

    std::string estadoActual = "ACTIVO";
    sf::Text currentStatus("Estado: ACTIVO", font, 16);
    currentStatus.setPosition(420, 565);
    currentStatus.setFillColor(sf::Color::Cyan);

    sf::RectangleShape statusBtns[3];
    sf::Text statusLabels[3];
    std::string estados[3] = { "ACTIVO", "OCUPADO", "INACTIVO" };
    sf::Color colores[3] = { sf::Color(0, 180, 0), sf::Color(200, 140, 0), sf::Color(100, 100, 100) };

    for (int i = 0; i < 3; ++i) {
        statusBtns[i].setSize({100, 30});
        statusBtns[i].setPosition(60 + i * 110, 560);
        statusBtns[i].setFillColor(colores[i]);

        statusLabels[i].setFont(font);
        statusLabels[i].setString(estados[i]);
        statusLabels[i].setCharacterSize(16);
        statusLabels[i].setPosition(70 + i * 110, 565);
        statusLabels[i].setFillColor(sf::Color::White);
    }

    std::string inputString;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
                wsClient.close();
            } else if (event.type == sf::Event::TextEntered) {
                if (event.text.unicode == 13 && !inputString.empty()) {
                    wsClient.sendMessage(destinatarioActual, inputString);
                    inputString.clear();
                } else if (event.text.unicode == 8 && !inputString.empty()) {
                    inputString.pop_back();
                } else if (event.text.unicode < 128 && event.text.unicode != 13 && event.text.unicode != 8) {
                    inputString += static_cast<char>(event.text.unicode);
                }
            } else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                if (sendButton.getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y)) {
                    if (!inputString.empty()) {
                        wsClient.sendMessage(destinatarioActual, inputString);
                        inputString.clear();
                    }
                }
                if (historyButton.getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y)) {
                    std::vector<char> solicitud = {5};  // Código 5 = solicitud de historial
                    wsClient.sendBinaryFrame(solicitud);
                }
                if (generalChatButton.getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y)) {
                    destinatarioActual = "~";
                    recipientText.setString("Enviando a: Todos");
                }


                if (closeButton.getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y)) {
                    window.close();
                    wsClient.close();
                }
                for (int i = 0; i < 3; ++i) {
                    if (statusBtns[i].getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y)) {
                        estadoActual = estados[i];
                        currentStatus.setString("Estado: " + estadoActual);
                        wsClient.updateStatus(estadoActual);
                    }
                }

                if (usersTitle.getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y)) {
                    destinatarioActual = "~";
                    recipientText.setString("Enviando a: Todos");
                } else {
                     std::vector<std::pair<std::string, uint8_t>> users = wsClient.getUsers();
                    float yStart = 60;
                    float lineHeight = 20;
                    for (int i = 0; i < users.size(); ++i) {
                        float userY = yStart + i * lineHeight;
                        if (event.mouseButton.x >= 640 && event.mouseButton.x <= 880 &&
                            event.mouseButton.y >= userY && event.mouseButton.y <= userY + lineHeight) {
                            destinatarioActual = users[i].first;
                            recipientText.setString("Enviando a: " + destinatarioActual);
                        }
                    }
                }
            }
        }

        std::vector<std::string> messages = wsClient.getMessages();
        std::string chat;
        bool mostrarMensajes = (estadoActual == "ACTIVO");

        for (const auto& msg : messages) {
            if (mostrarMensajes) {
                chat += msg + "\n";
            }
        }
        chatMessages.setString(chat);


        // ⬇️ Calcular altura total del texto
        float lineSpacing = chatMessages.getFont()->getLineSpacing(chatMessages.getCharacterSize());
        float totalTextHeight = lineSpacing * messages.size();

        // ⬇️ Si es más alto que el área visible, lo "subimos"
        if (totalTextHeight > chatBox.getSize().y) {
            float offset = totalTextHeight - chatBox.getSize().y;
            chatMessages.setPosition(30, 60 - offset);  // scroll visual hacia arriba
        } else {
            chatMessages.setPosition(30, 60);  // posición normal
        }

        inputText.setString(inputString);

        std::vector<std::pair<std::string, uint8_t>> users = wsClient.getUsers();
        std::string userList;
        for (const auto& [name, status] : users) {
        std::string estadoTexto;
        if (status == 1) estadoTexto = " [ACTIVO]";
        else if (status == 2) estadoTexto = " [OCUPADO]";
        else if (status == 3) estadoTexto = " [INACTIVO]";
        else if (status == 0) estadoTexto = " [DESCONECTADO]";  
        else estadoTexto = " [---]";

        userList += name + estadoTexto + "\n";
    }



        userListText.setString(userList);


        window.clear(sf::Color(30, 30, 30));
        window.draw(userNameText);
        window.draw(chatBox);
        window.draw(chatMessages);
        window.draw(usersBox);
        window.draw(usersTitle);
        window.draw(userListText);
        window.draw(inputBox);
        window.draw(recipientText);  // ⬅ muestra a quién se envía
        window.draw(inputText);
        window.draw(sendButton);
        window.draw(historyButton);
        window.draw(historyButtonText);
        window.draw(generalChatButton);
        window.draw(generalChatText);
        window.draw(sendButtonText);
        window.draw(closeButton);
        window.draw(closeButtonText);
        if (estadoActual == "OCUPADO") {
            ocultoText.setString(" Estado OCUPADO. Los mensajes estan ocultos.");
            window.draw(ocultoText);
        }

        for (int i = 0; i < 3; ++i) {
            window.draw(statusBtns[i]);
            window.draw(statusLabels[i]);
        }
        window.display();
    }

    wsClient.close();
    return 0;
}
