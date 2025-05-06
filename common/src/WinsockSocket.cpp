#ifdef _WIN32

#include "WinsockSocket.h"

#include <cstring>
#include <iostream>

// Static members initialization
int WinsockSocket::winsock_init_count_ = 0;
std::mutex WinsockSocket::winsock_mutex_;

/**
 * @brief Initializes the Winsock library.
 *
 * @return True if initialization is successful, false otherwise.
 */
bool WinsockSocket::InitializeWinsock() {
  std::lock_guard<std::mutex> lock(winsock_mutex_);

  if (winsock_init_count_ == 0) {
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
      std::cerr << "WSAStartup failed: " << result << std::endl;
      return false;
    }
  }
  winsock_init_count_++;

  return true;
}

/**
 * @brief Cleans up the Winsock library.
 */
void WinsockSocket::CleanupWinsock() {
  std::lock_guard<std::mutex> lock(winsock_mutex_);
  winsock_init_count_--;
  if (winsock_init_count_ == 0) {
    WSACleanup();
  }
}

/**
 * @brief Constructs a new WinsockSocket object.
 *
 * @param socket_handle The handle for the socket. If INVALID_SOCKET, a new
 * socket will be created.
 */
WinsockSocket::WinsockSocket(SOCKET socket_handle) : socket_handle_(socket_handle) {
  if (!InitializeWinsock()) {
    // Handle Winsock initialization failure if necessary
    // For now, we'll just report the error in InitializeWinsock
  }

  if (socket_handle_ == INVALID_SOCKET) {
    // Create a new socket if one was not provided
    socket_handle_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_handle_ == INVALID_SOCKET) {
      std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
    }
  }
}

/**
 * @brief Destroys the WinsockSocket object. Closes the socket if it's open.
 */
WinsockSocket::~WinsockSocket() {
  Close();
  CleanupWinsock();
}

/**
 * @brief Connects the socket to a remote address and port.
 *
 * @param address The IP address or hostname.
 * @param port The port number.
 * @return True if connection is successful, false otherwise.
 */
bool WinsockSocket::Connect(const std::string &address, int port) {
  if (!IsValid()) {
    std::cerr << "Socket is not valid." << std::endl;
    return false;
  }

  struct addrinfo hints, *result = nullptr;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  // Resolve the server address and port
  int status = getaddrinfo(address.c_str(), std::to_string(port).c_str(), &hints, &result);
  if (status != 0) {
    std::cerr << "getaddrinfo failed: " << gai_strerror(status) << std::endl;
    return false;
  }

  // Attempt to connect to the first address returned by getaddrinfo
  bool connected = false;
  for (struct addrinfo *ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
    if (connect(socket_handle_, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
      std::cerr << "Connect failed with error: " << WSAGetLastError() << std::endl;
    } else {
      connected = true;
      break;
    }
  }

  freeaddrinfo(result);

  if (!connected) {
    std::cerr << "Unable to connect to server." << std::endl;
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
bool WinsockSocket::Bind(const std::string &address, int port) {
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
    // Convert string IP to IN_ADDR structure
    if (InetPton(AF_INET, address.c_str(), &server_addr.sin_addr) != 1) {
      std::cerr << "Invalid address: " << address << std::endl;
      return false;
    }
  }

  // Allow reuse of the address
  int opt = 1;
  if (setsockopt(socket_handle_, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) == SOCKET_ERROR) {
    std::cerr << "Error setting SO_REUSEADDR: " << WSAGetLastError() << std::endl;
  }

  if (bind(socket_handle_, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
    std::cerr << "Error binding to " << address << ":" << port << ": " << WSAGetLastError() << std::endl;
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
bool WinsockSocket::Listen(int backlog) {
  if (!IsValid()) {
    std::cerr << "Socket is not valid." << std::endl;
    return false;
  }

  if (listen(socket_handle_, backlog) == SOCKET_ERROR) {
    std::cerr << "Error listening on socket: " << WSAGetLastError() << std::endl;
    return false;
  }

  return true;
}

/**
 * @brief Accepts a new incoming connection.
 *
 * @return A pointer to a new ISocket representing the accepted connection,
 * or nullptr if an error occurs. The caller is responsible for
 * managing the lifetime of the returned socket.
 */
ISocket *WinsockSocket::Accept() {
  if (!IsValid()) {
    std::cerr << "Socket is not valid." << std::endl;
    return nullptr;
  }

  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);

  SOCKET client_socket_handle = accept(socket_handle_, (struct sockaddr *)&client_addr, &client_addr_len);

  if (client_socket_handle == INVALID_SOCKET) {
    int error_code = WSAGetLastError();
    std::cerr << "Error accepting connection: " << error_code << std::endl;
    return nullptr;
  }

  return new WinsockSocket(client_socket_handle);
}

/**
 * @brief Sends data through the socket.
 *
 * @param data The data to send.
 * @param size The number of bytes to send.
 * @return The number of bytes sent, or -1 on error.
 */
int WinsockSocket::Send(const void *data, size_t size) {
  if (!IsValid()) {
    std::cerr << "Socket is not valid." << std::endl;
    return -1;
  }

  // Use 0 flags for basic send
  int bytes_sent = send(socket_handle_, (const char *)data, (int)size, 0);
  if (bytes_sent == SOCKET_ERROR) {
    std::cerr << "Error sending data: " << WSAGetLastError() << std::endl;
    return -1;
  }

  return bytes_sent;
}

/**
 * @brief Receives data from the socket.
 *
 * @param buffer The buffer to store the received data.
 * @param size The maximum number of bytes to receive.
 * @return The number of bytes received, or -1 on error, 0 if the connection
 * has been closed by the peer.
 */
int WinsockSocket::Receive(void *buffer, size_t size) {
  if (!IsValid()) {
    std::cerr << "Socket is not valid." << std::endl;
    return -1;
  }

  // Use 0 flags for basic receive
  int bytes_received = recv(socket_handle_, (char *)buffer, (int)size, 0);
  if (bytes_received == SOCKET_ERROR) {
    int error_code = WSAGetLastError();
    // Ignore non-blocking errors if in non-blocking mode (not currently)
    // For now, treat any receive error as a failure.
    std::cerr << "Error receiving data: " << error_code << std::endl;
    return -1;
  }

  return bytes_received;
}

/**
 * @brief Closes the socket connection.
 */
void WinsockSocket::Close() {
  if (IsValid()) {
    closesocket(socket_handle_);
    socket_handle_ = INVALID_SOCKET; // Mark as invalid
  }
}

/**
 * @brief Checks if the socket is valid/open.
 * @return True if the socket is valid, false otherwise.
 */
bool WinsockSocket::IsValid() const {
  return socket_handle_ != INVALID_SOCKET;
}

#endif // _WIN32
