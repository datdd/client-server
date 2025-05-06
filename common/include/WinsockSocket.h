// common/src/WinsockSocket.h
#ifndef WINSOCK_SOCKET_H_
#define WINSOCK_SOCKET_H_

#ifdef _WIN32

#include "ISocket.h"

#include <mutex>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

/**
 * @brief Winsock implementation of the ISocket interface.
 *
 * This class provides a concrete implementation of the ISocket interface
 * using Winsock API functions for Windows platforms. Adheres to the
 * Liskov Substitution Principle.
 */
class WinsockSocket : public ISocket {
public:
  /**
   * @brief Constructs a new WinsockSocket object.
   *
   * @param socket_handle The handle for the socket. If INVALID_SOCKET, a new
   * socket will be created.
   */
  explicit WinsockSocket(SOCKET socket_handle = INVALID_SOCKET);

  /**
   * @brief Destroys the WinsockSocket object. Closes the socket if it's open.
   */
  ~WinsockSocket() override;

  /**
   * @brief Connects the socket to a remote address and port.
   *
   * @param address The IP address or hostname.
   * @param port The port number.
   * @return True if connection is successful, false otherwise.
   */
  bool Connect(const std::string &address, int port) override;

  /**
   * @brief Binds the socket to a local address and port.
   *
   * @param address The IP address to bind to (e.g., "0.0.0.0" for any).
   * @param port The port number.
   * @return True if binding is successful, false otherwise.
   */
  bool Bind(const std::string &address, int port) override;

  /**
   * @brief Listens for incoming connections on the bound socket.
   *
   * @param backlog The maximum length of the queue of pending connections.
   * @return True if listening starts successfully, false otherwise.
   */
  bool Listen(int backlog) override;

  /**
   * @brief Accepts a new incoming connection.
   *
   * @return A pointer to a new ISocket representing the accepted connection,
   * or nullptr if an error occurs. The caller is responsible for
   * managing the lifetime of the returned socket.
   */
  ISocket *Accept() override;

  /**
   * @brief Sends data through the socket.
   *
   * @param data The data to send.
   * @param size The number of bytes to send.
   * @return The number of bytes sent, or -1 on error.
   */
  int Send(const void *data, size_t size) override;

  /**
   * @brief Receives data from the socket.
   *
   * @param buffer The buffer to store the received data.
   * @param size The maximum number of bytes to receive.
   * @return The number of bytes received, or -1 on error, 0 if the connection
   * has been closed by the peer.
   */
  int Receive(void *buffer, size_t size) override;

  /**
   * @brief Closes the socket connection.
   */
  void Close() override;

  /**
   * @brief Checks if the socket is valid/open.
   *
   * @return True if the socket is valid, false otherwise.
   */
  bool IsValid() const override;

private:
  SOCKET socket_handle_; /**< The Winsock socket handle. */

  /**
   * @brief Initializes the Winsock library.
   *
   * @return True if initialization is successful, false otherwise.
   */
  static bool InitializeWinsock();

  /**
   * @brief Cleans up the Winsock library.
   */
  static void CleanupWinsock();

  static int winsock_init_count_;
  static std::mutex winsock_mutex_;
};

#endif // _WIN32

#endif // WINSOCK_SOCKET_H_
