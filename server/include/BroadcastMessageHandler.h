#ifndef BROADCAST_MESSAGE_HANDLER_H_
#define BROADCAST_MESSAGE_HANDLER_H_

#include "IMessageHandler.h"
#include "Message.h"

/**
 * @brief Message handler for broadcasting messages to all clients.
 *
 * This class implements the IMessageHandler interface to handle incoming
 * messages by broadcasting them to all connected clients managed by the server.
 * Adheres to the IMessageHandler interface contract.
 */
class BroadcastMessageHandler : public IMessageHandler {
public:
  /**
   * @brief Constructs a new BroadcastMessageHandler.
   */
  BroadcastMessageHandler() = default;

  /**
   * @brief Destroys the BroadcastMessageHandler.
   */
  ~BroadcastMessageHandler() override = default;

  /**
   * @brief Handles an incoming message by broadcasting it to all clients.
   * 
   * @param message The deserialized Message object received.
   * @param sender The client handler that received the message.
   * @param server A pointer to the Server instance for broadcasting.
   * @return True if the message was handled successfully, false otherwise.
   */
  bool HandleMessage(const Message &message, IClientHandler *sender,
                     Server *server) override;
};

#endif // BROADCAST_MESSAGE_HANDLER_H_
