#ifndef POSIX_SOCKET_H_
#define POSIX_SOCKET_H_

#include "ISocket.h"

#include <string>

// Include necessary POSIX headers
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * @brief POSIX implementation of the ISocket interface.
 *
 * This class provides a concrete implementation of the ISocket interface
 * using POSIX socket functions. Adheres to the Liskov Substitution Principle
 * as it can be substituted for ISocket.
 */
class PosixSocket : public ISocket {
public:
  /**
   * @brief Constructs a new PosixSocket object.
   * @param socket_fd The file descriptor for the socket. If -1, a new socket
   * will be created.
   */
  explicit PosixSocket(int socket_fd = -1);

  /**
   * @brief Destroys the PosixSocket object. Closes the socket if it's open.
   */
  ~PosixSocket() override;

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
   * @return A pointer to a new PosixSocket representing the accepted connection,
   * or nullptr if an error occurs. The caller is responsible for managing the
   * lifetime of the returned socket.
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
  int socket_fd_; /**< The POSIX socket file descriptor. */
};

#endif // POSIX_SOCKET_H_
