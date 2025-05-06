#include "PosixSocket.h"

#include <cstring> // For strerror
#include <iostream>

/**
 * @brief Constructs a new PosixSocket object.
 *
 * @param socket_fd The file descriptor for the socket. If -1, a new socket  will be created.
 */
PosixSocket::PosixSocket(int socket_fd) : socket_fd_(socket_fd) {
  if (socket_fd_ == -1) {
    // Create a new socket if one was not provided
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
      std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
    }
  }
}

/**
 * @brief Destroys the PosixSocket object. Closes the socket if it's open.
 */
PosixSocket::~PosixSocket() {
  Close();
}

/**
 * @brief Connects the socket to a remote address and port.
 *
 * @param address The IP address or hostname.
 * @param port The port number.
 * @return True if connection is successful, false otherwise.
 */
bool PosixSocket::Connect(const std::string &address, int port) {
  if (!IsValid()) {
    std::cerr << "Socket is not valid." << std::endl;
    return false;
  }

  struct sockaddr_in server_addr;
  std::memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  // Resolve hostname if necessary
  if (inet_pton(AF_INET, address.c_str(), &server_addr.sin_addr) <= 0) {
    struct hostent *host = gethostbyname(address.c_str());
    if (host == nullptr) {
      std::cerr << "Error resolving hostname " << address << ": " << hstrerror(h_errno) << std::endl;
      return false;
    }
    std::memcpy(&server_addr.sin_addr, host->h_addr_list[0], host->h_length);
  }

  if (connect(socket_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    std::cerr << "Error connecting to " << address << ":" << port << ": " << strerror(errno) << std::endl;
    return false;
  }

  return true;
}

/**
 * @brief Binds the socket to a local address and port.
 *
 * @param address The IP address to bind to (e.g., "0.0.0.0" for any).
 * @param port The port number.
 * @return True if binding is successful, false otherwise.
 */
bool PosixSocket::Bind(const std::string &address, int port) {
  if (!IsValid()) {
    std::cerr << "Socket is not valid." << std::endl;
    return false;
  }

  struct sockaddr_in server_addr;
  std::memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (address == "0.0.0.0") {
    server_addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    if (inet_pton(AF_INET, address.c_str(), &server_addr.sin_addr) <= 0) {
      std::cerr << "Invalid address: " << address << std::endl;
      return false;
    }
  }

  // Allow reuse of the address
  int opt = 1;
  if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    std::cerr << "Error setting SO_REUSEADDR: " << strerror(errno) << std::endl;
  }

  if (bind(socket_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    std::cerr << "Error binding to " << address << ":" << port << ": " << strerror(errno) << std::endl;
    return false;
  }

  return true;
}

/**
 * @brief Listens for incoming connections on the bound socket.
 *
 * @param backlog The maximum length of the queue of pending connections.
 * @return True if listening starts successfully, false otherwise.
 */
bool PosixSocket::Listen(int backlog) {
  if (!IsValid()) {
    std::cerr << "Socket is not valid." << std::endl;
    return false;
  }

  if (listen(socket_fd_, backlog) < 0) {
    std::cerr << "Error listening on socket: " << strerror(errno) << std::endl;
    return false;
  }

  return true;
}

/**
 * @brief Accepts a new incoming connection.
 *
 * @return A pointer to a new PosixSocket representing the accepted connection, or nullptr if an error occurs. The
 * caller is responsible for managing the lifetime of the returned socket.
 */
ISocket *PosixSocket::Accept() {
  if (!IsValid()) {
    std::cerr << "Socket is not valid." << std::endl;
    return nullptr;
  }

  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  int client_socket_fd = accept(socket_fd_, (struct sockaddr *)&client_addr, &client_addr_len);

  if (client_socket_fd < 0) {
    // Ignore interrupted system calls
    if (errno == EINTR) {
      return Accept(); // Retry accept
    }
    std::cerr << "Error accepting connection: " << strerror(errno) << std::endl;
    return nullptr;
  }

  return new PosixSocket(client_socket_fd);
}

/**
 * @brief Sends data through the socket.

 * @param data The data to send.
 * @param size The number of bytes to send.
 * @return The number of bytes sent, or -1 on error.
 */
int PosixSocket::Send(const void *data, size_t size) {
  if (!IsValid()) {
    std::cerr << "Socket is not valid." << std::endl;
    return -1;
  }

  // Use MSG_NOSIGNAL to prevent SIGPIPE on broken pipes
  int bytes_sent = send(socket_fd_, data, size, MSG_NOSIGNAL);
  if (bytes_sent < 0) {
    std::cerr << "Error sending data: " << strerror(errno) << std::endl;
  }

  return bytes_sent;
}

/**
 * @brief Receives data from the socket.
 *
 * @param buffer The buffer to store the received data.
 * @param size The maximum number of bytes to receive.
 * @return The number of bytes received, or -1 on error, 0 if the connection has been closed by the peer.
 */
int PosixSocket::Receive(void *buffer, size_t size) {
  if (!IsValid()) {
    std::cerr << "Socket is not valid." << std::endl;
    return -1;
  }

  int bytes_received = recv(socket_fd_, buffer, size, 0);
  if (bytes_received < 0) {
    // Ignore interrupted system calls
    if (errno == EINTR) {
      return Receive(buffer, size); // Retry receive
    }
    std::cerr << "Error receiving data: " << strerror(errno) << std::endl;
  }

  return bytes_received;
}

/**
 * @brief Closes the socket connection.
 */
void PosixSocket::Close() {
  if (IsValid()) {
    close(socket_fd_);
    socket_fd_ = -1;
  }
}

/**
 * @brief Checks if the socket is valid/open.
 *
 * @return True if the socket is valid, false otherwise.
 */
bool PosixSocket::IsValid() const {
  return socket_fd_ >= 0;
}
