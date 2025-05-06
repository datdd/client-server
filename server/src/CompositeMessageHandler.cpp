#include "CompositeMessageHandler.h"

#include <iostream>
#include <utility> // For std::move

/**
 * @brief Adds a message handler to the composite.
 *
 * Handlers are tried in the order they are added.
 *
 * @param handler The message handler to add.
 */
void CompositeMessageHandler::AddHandler(
    std::unique_ptr<IMessageHandler> handler) {
  if (handler) {
    handlers_.push_back(std::move(handler));
  }
}

/**
 * @brief Handles an incoming message by dispatching it to registered handlers.
 *
 * @param message The deserialized Message object received.
 * @param sender The client handler that received the message.
 * @param server A pointer to the Server instance.
 * @return True if any handler processed the message, false otherwise.
 */
bool CompositeMessageHandler::HandleMessage(const Message &message,
                                            IClientHandler *sender,
                                            Server *server) {
  for (const auto &handler : handlers_) {
    if (handler->HandleMessage(message, sender, server)) {
      return true;
    }
  }

  // No handler processed the message
  std::cerr << "No handler processed message of type: "
            << static_cast<int>(message.header.type) << " from client "
            << message.header.sender_id << std::endl;

  // Optional: Send an error message back to the sender for unhandled messages
  // This requires the Server or ClientHandler to have a way to send errors
  // back based on the message type. For now, we'll just log it.

  return false;
}
