#ifndef ISOCKET_H
#define ISOCKET_H

#include <string>
#include <vector>

/**
 * @brief The ISocket class is an interface for socket communication.
 *
 * This interface defines the basic operations required for network
 * communication, abstracting the underlying socket implementation (e.g., POSIX,
 * Winsock).
 */
class ISocket {
public:
  virtual ~ISocket() = default;

  /**
   * @brief Connects the socket to a remote address and port.
   *
   * @param address The IP address or hostname of the remote server.
   * @param port The port number of the remote server.
   * @return true if the connection was successful, false otherwise.
   */
  virtual bool Connect(const std::string &address, int port) = 0;

  /**
   * @brief Bind the socket to a local address and port.alignas
   *
   * @param address The IP address or hostname to bind to (e.g., "0.0.0.0" for
   * any).
   * @param port The port number to bind to.
   * @return true if the binding was successful, false otherwise.
   */
  virtual bool Bind(const std::string &address, int port) = 0;

  /**
   * @brief Listens for incoming connections on the bound socket.
   *
   * @param backlog The maximum length of the queue of pending connections.
   * @return true if the socket is successfully set to listen mode, false
   * otherwise.
   */
  virtual bool Listen(int backlog) = 0;

  /**
   * @brief Accepts an incoming connection on the listening socket.
   *
   * @return A pointer to a new ISocket instance representing the accepted
   * connection, or nullptr if the accept operation failed. The caller is
   * responsible for managing the lifetime of the returned socket. The returned
   * socket should be used for communication with the connected client. The
   * caller should delete the socket when done.
   */
  virtual ISocket *Accept() = 0;

  /**
   * @brief Sends data through the socket.
   *
   * @param data Pointer to the data to be sent.
   * @param size Size of the data in bytes.
   * @return The number of bytes sent, or -1 if an error occurred.
   */
  virtual int Send(const void *data, size_t size) = 0;

  /**
   * @brief Receives data from the socket.
   *
   * @param buffer Pointer to the buffer where received data will be stored.
   * @param size Size of the buffer in bytes.
   * @return The number of bytes received, or -1 if an error occurred. If the
   * return value is 0, it indicates that the connection has been closed.
   */
  virtual int Receive(void *buffer, size_t size) = 0;

  /**
   * @brief Closes the socket connection.
   */
  virtual void Close() = 0;

  /**
   * @brief Checks if the socket is valid/open.
   *
   * @return true if the socket is valid, false otherwise.
   */
  virtual bool IsValid() const = 0;
};

#endif // ISOCKET_H