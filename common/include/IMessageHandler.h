#ifndef IMESSAGE_HANDLER_H_
#define IMESSAGE_HANDLER_H_

#include <string>
#include <vector>

#include "Message.h"

// Forward declarations to avoid circular dependencies
class IClientHandler;
class Server;

/**
 * @brief Interface for handling incoming messages.
 *
 * This interface defines the contract for processing messages received
 * from a client. Different implementations can handle different message types
 * or protocols. Adheres to the Open/Closed Principle for adding new message
 * handling logic.
 */
class IMessageHandler {
public:
  /**
   * @brief Virtual destructor.
   */
  virtual ~IMessageHandler() = default;

  /**
   * @brief Handles an incoming message.
   *
   * This method is called by the client handler when a complete message is
   * received. The handler should process the message based on its type.
   *
   * @param message The deserialized Message object received.
   * @param sender The client handler that received the message.
   * @param server A pointer to the Server instance (for actions like
   * broadcasting).
   * @return True if the message was handled successfully, false otherwise.
   */
  virtual bool HandleMessage(const Message &message, IClientHandler *sender, Server *server) = 0;
};

#endif // IMESSAGE_HANDLER_H_
