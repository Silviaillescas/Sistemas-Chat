#include <libwebsockets.h>
#include <openssl/sha.h>
#include <unordered_map>
#include <set>
#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace chrono;

// Estructuras y constantes
struct ClientInfo {
    string username;
    uint8_t status;  // 1=ACTIVO, 2=OCUPADO, 3=INACTIVO, 0=DESCONECTADO
    steady_clock::time_point last_activity;
    vector<uint8_t> write_buffer;
    mutex buffer_mutex;
    struct lws *wsi;
    
    ClientInfo() : status(1) {}  // Inicialización C++98 compatible
};

mutex clients_mutex;
unordered_map<string, ClientInfo> clients;
map<string, vector<string> > message_history;  // Espacio entre > >
vector<string> global_history;
set<string> all_users;

const int PORT = 9000;
const int INACTIVITY_TIMEOUT = 60;

// Prototipos
void broadcast_status_change(const string& username, uint8_t status);
void send_error(struct lws *wsi, uint8_t error_code);
void handle_message(struct lws *wsi, ClientInfo* client, unsigned char* data, size_t len);
void check_inactivity();
void update_user_activity(ClientInfo* client);

// Callbacks LWS
static int callback_protocol(struct lws *wsi, enum lws_callback_reasons reason, 
                            void *user, void *in, size_t len) {
    ClientInfo* client = (ClientInfo*)user;

    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            char uri[256];
            lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI);
            string query(uri);
            size_t name_pos = query.find("?name=");

            if (name_pos == string::npos || query.size() <= name_pos + 6) {
                lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, NULL, 0);
                return -1;
            }

            string username = query.substr(name_pos + 6);
            if (username.empty() || username == "~" || username.size() > 255) {
                lws_close_reason(wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, NULL, 0);
                return -1;
            }

            {
            lock_guard<mutex> lock(clients_mutex);
            
            if (clients.count(username) || all_users.count(username)) {
                lws_close_reason(wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, NULL, 0);
                return -1;
            }

            all_users.insert(username);
            clients.emplace(std::piecewise_construct,
                            std::forward_as_tuple(username),
                            std::make_tuple());
            
            ClientInfo& new_client = clients[username];
            new_client.username = username;
            new_client.wsi = wsi;
            client = &new_client;
        }

            broadcast_status_change(username, 53);
            break;
        }

        case LWS_CALLBACK_RECEIVE: {
            if (!client) return -1;
            update_user_activity(client);
            handle_message(wsi, client, (unsigned char*)in, len);
            break;
        }

        case LWS_CALLBACK_SERVER_WRITEABLE: {
            if (!client) return -1;
            
            lock_guard<mutex> lock(client->buffer_mutex);
            if (!client->write_buffer.empty()) {
                lws_write(wsi, client->write_buffer.data(), 
                         client->write_buffer.size(), LWS_WRITE_BINARY);
                client->write_buffer.clear();
            }
            break;
        }

        case LWS_CALLBACK_CLOSED: {
            if (!client) return 0;
            
            {
                lock_guard<mutex> lock(clients_mutex);
                client->status = 0;
                broadcast_status_change(client->username, 0);
                clients.erase(client->username);
            }
            break;
        }

        default:
            break;
    }
    return 0;
}

void handle_message(struct lws *wsi, ClientInfo* client, unsigned char* data, size_t len) {
    if (len < 1) return;
    uint8_t msg_type = data[0];

    switch (msg_type) {
        case 1: { // Listar usuarios
            vector<uint8_t> response;
            response.push_back(51);
            response.push_back(clients.size());
            
            lock_guard<mutex> lock(clients_mutex);
            for (unordered_map<string, ClientInfo>::iterator it = clients.begin(); it != clients.end(); ++it) {
                response.push_back(it->first.size());
                response.insert(response.end(), it->first.begin(), it->first.end());
                response.push_back(it->second.status);
            }

            lock_guard<mutex> buf_lock(client->buffer_mutex);
            client->write_buffer = response;
            lws_callback_on_writable(wsi);
            break;
        }

        case 3: { // Cambiar estado
            if (len < 2 || data[1] < 1 || data[1] > 3) {
                send_error(wsi, 2);
                return;
            }

            {
                lock_guard<mutex> lock(clients_mutex);
                client->status = data[1];
                broadcast_status_change(client->username, data[1]);
            }
            break;
        }

        case 4: { // Enviar mensaje
            if (len < 4) {
                send_error(wsi, 3);
                return;
            }

            uint8_t dest_len = data[1];
            string dest((char*)data + 2, dest_len);
            uint8_t msg_len = data[2 + dest_len];
            string message((char*)data + 3 + dest_len, msg_len);

            if (message.empty()) {
                send_error(wsi, 3);
                return;
            }

            vector<uint8_t> response;
            response.push_back(55);
            response.push_back(client->username.size());
            response.insert(response.end(), client->username.begin(), client->username.end());
            response.push_back(message.size());
            response.insert(response.end(), message.begin(), message.end());

            if (dest == "~") {
                global_history.push_back(client->username + ": " + message);
                
                lock_guard<mutex> lock(clients_mutex);
                for (unordered_map<string, ClientInfo>::iterator it = clients.begin(); it != clients.end(); ++it) {
                    lock_guard<mutex> buf_lock(it->second.buffer_mutex);
                    it->second.write_buffer = response;
                    lws_callback_on_writable(it->second.wsi);
                }
            } else {
                lock_guard<mutex> lock(clients_mutex);
                unordered_map<string, ClientInfo>::iterator it = clients.find(dest);
                if (it == clients.end() || it->second.status == 0) {
                    send_error(wsi, 4);
                    return;
                }

                message_history[dest].push_back(client->username + ": " + message);
                
                lock_guard<mutex> buf_lock(it->second.buffer_mutex);
                it->second.write_buffer = response;
                lws_callback_on_writable(it->second.wsi);
            }
            break;
        }

        case 5: { // Obtener historial
            if (len < 2) return;
            uint8_t chat_len = data[1];
            string chat((char*)data + 2, chat_len);

            vector<uint8_t> response;
            response.push_back(56);
            vector<string>* history = NULL;

            if (chat == "~") history = &global_history;
            else history = &message_history[chat];

            response.push_back(history->size());
            for (vector<string>::iterator it = history->begin(); it != history->end(); ++it) {
                response.push_back(it->size());
                response.insert(response.end(), it->begin(), it->end());
            }

            lock_guard<mutex> lock(client->buffer_mutex);
            client->write_buffer = response;
            lws_callback_on_writable(wsi);
            break;
        }

        default:
            send_error(wsi, 1);
            break;
    }
}

void broadcast_status_change(const string& username, uint8_t status) {
    vector<uint8_t> msg;
    msg.push_back(54);
    msg.push_back(username.size());
    msg.insert(msg.end(), username.begin(), username.end());
    msg.push_back(status);

    lock_guard<mutex> lock(clients_mutex);
    for (unordered_map<string, ClientInfo>::iterator it = clients.begin(); it != clients.end(); ++it) {
        lock_guard<mutex> buf_lock(it->second.buffer_mutex);
        it->second.write_buffer = msg;
        lws_callback_on_writable(it->second.wsi);
    }
}

void send_error(struct lws *wsi, uint8_t error_code) {
    vector<uint8_t> err;
    err.push_back(50);
    err.push_back(error_code);
    
    ClientInfo* client = (ClientInfo*)lws_wsi_user(wsi);
    if (!client) return;

    lock_guard<mutex> lock(client->buffer_mutex);
    client->write_buffer = err;
    lws_callback_on_writable(wsi);
}

void check_inactivity() {
    while (true) {
        this_thread::sleep_for(seconds(10));
        
        auto now = steady_clock::now();
        vector<string> inactive_users;

        {
            lock_guard<mutex> lock(clients_mutex);
            for (unordered_map<string, ClientInfo>::iterator it = clients.begin(); it != clients.end(); ++it) {
                duration<double> elapsed = now - it->second.last_activity;
                if (elapsed.count() >= INACTIVITY_TIMEOUT && it->second.status != 3) {
                    it->second.status = 3;
                    inactive_users.push_back(it->first);
                }
            }
        }

        for (vector<string>::iterator it = inactive_users.begin(); it != inactive_users.end(); ++it) {
            broadcast_status_change(*it, 3);
            cout << "[Inactividad] " << *it << " marcado como INACTIVO\n";
        }
    }
}

void update_user_activity(ClientInfo* client) {
    lock_guard<mutex> lock(clients_mutex);
    client->last_activity = steady_clock::now();
    
    if (client->status == 3) {
        client->status = 1;
        broadcast_status_change(client->username, 1);
    }
}

static struct lws_protocols protocols[] = {
    {"chat-protocol", callback_protocol, sizeof(ClientInfo), 0},
    {NULL, NULL, 0, 0}
};

int main() {
    lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = PORT;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    lws_context* context = lws_create_context(&info);
    if (!context) {
        cerr << "Error al crear contexto LWS\n";
        return 1;
    }

    thread(check_inactivity).detach();
    cout << "✅ Servidor iniciado en puerto " << PORT << endl;

    while (true) {
        lws_service(context, 50);
    }

    lws_context_destroy(context);
    return 0;
}