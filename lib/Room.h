#include <vector>
#include "Socket.h"
#include <string>
#include <optional>
#include "Protocol.h"

class Room {
private:
    int room_id_;
    std::vector<Socket> clients_;
    int max_clients_;
    int current_clients_;
public:
    Room(int room_id, int max_clients=5);
    ~Room();

    bool join_client(Socket&& client);
    std::optional<Socket> exit_client(int fd);
    const std::optional<Socket&> find_client(int fd);
    const std::vector<Socket>& clients();
    void send_message(Socket& sender, std::string message);
};