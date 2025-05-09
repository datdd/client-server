// server/src/IClientHandler.h
#ifndef ICLIENT_HANDLER_H_
#define ICLIENT_HANDLER_H_

#include <string>
#include <vector>

#include "ISocket.h" // Required for ISocket* return type in GetSocket()
#include "Message.h" // Include the Message structure

// Forward declarations to avoid circular dependencies
class Server; // Forward declare Server

/**
 * @brief Interface for handling a single client connection on the server.
 *
 * This interface defines the basic contract for managing a connection with a single
 * client, including receiving and sending messages. Adheres to the
 * Interface Segregation Principle.
 */
class IClientHandler {
public:
  /**
   * @brief Virtual destructor.
   */
  virtual ~IClientHandler() = default;

  /**
   * @brief Starts the client handler thread.
   *
   * This method should initiate the thread that listens for messages
   * from the client.
   */
  virtual void Start() = 0;

  /**
   * @brief Stops the client handler and closes the connection.
   */
  virtual void Stop() = 0;

  /**
   * @brief Sends a message to the connected client.
   * @param message The message structure to send.
   * @return True if the message was sent successfully, false otherwise.
   */
  virtual bool SendMessage(const Message &message) = 0; // Updated to take Message object

  /**
   * @brief Gets the unique identifier for this client handler.
   * @return The client ID.
   */
  virtual int GetClientId() const = 0;

  /**
   * @brief Gets a pointer to the underlying socket.
   * @return A pointer to the ISocket instance.
   */
  virtual ISocket *GetSocket() const = 0;

  // Removed IsThreadJoinable() as it's not needed for queue-based cleanup
};

#endif // ICLIENT_HANDLER_H_
