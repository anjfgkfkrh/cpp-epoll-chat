#include <iostream>
#include <stdexcept>
#include <cstring>

#include "lib/Socket.cpp"
#include "lib/Epoll.cpp"

constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 64;
constexpr int BUF_SIZE = 1024;

// 리스닝 소켓
Socket create_listener(int port) {
    Socket sock(socket(AF_INET, SOCK_STREAM, 0));

    int opt = 1;
    setsockopt(sock.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind failed");

    if (listen(sock.get(), 128) < 0)
        throw std::runtime_error("listen failed");

    return sock;
}

int main() {
    try {
        auto listener = create_listener(PORT);
        Epoll epoll;
        epoll.add(listener.get(), EPOLLIN);

        std::vector<epoll_event> events(MAX_EVENTS);
        char buf[BUF_SIZE];

        std::cout << "Listening on port " << PORT << "\n";

        while(true){
            int n = epoll.wait(events);

            for (int i = 0; i < n; i++){
                int fd = events[i].data.fd;

                if(fd == listener.get()) {
                    // 새 연결
                    int client = accept(listener.get(), nullptr, nullptr);
                    if (client >= 0) {
                        epoll.add(client, EPOLLIN);
                        std::cout << "New client: " << client << "\n";
                    }
                } else {
                    // 데이터 수신
                    ssize_t bytes = read(fd, buf, BUF_SIZE);
                    if (bytes <= 0) {
                        epoll.remove(fd);
                        close(fd);
                        std::cout << "Client disconnected: " << fd << "\n";
                    } else {
                        // TODO: 연결된 클라이언트에게서 정상적인 데이터 수신
                        write(fd, buf, bytes); // 임시 echo
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}