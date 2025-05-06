#ifndef COMPOSITE_MESSAGE_HANDLER_H_
#define COMPOSITE_MESSAGE_HANDLER_H_

#include "IClientHandler.h"
#include "IMessageHandler.h"
#include "Message.h"
#include "Server.h"

#include <memory>
#include <vector>

/**
 * @brief A message handler that dispatches messages to a list of other
 * handlers.
 *
 * This handler iterates through a list of registered IMessageHandler instances
 * and passes the incoming message to the first handler that indicates it
 * processed the message.
 */
class CompositeMessageHandler : public IMessageHandler {
public:
  /**
   * @brief Constructs a new CompositeMessageHandler.
   */
  CompositeMessageHandler() = default;

  /**
   * @brief Destroys the CompositeMessageHandler.
   */
  ~CompositeMessageHandler() override = default;

  /**
   * @brief Adds a message handler to the composite.
   *
   * Handlers are tried in the order they are added.
   *
   * @param handler The message handler to add.
   */
  void AddHandler(std::unique_ptr<IMessageHandler> handler);

  /**
   * @brief Handles an incoming message by dispatching it to registered
   * handlers.
   *
   * @param message The deserialized Message object received.
   * @param sender The client handler that received the message.
   * @param server A pointer to the Server instance.
   * @return True if any handler processed the message, false otherwise.
   */
  bool HandleMessage(const Message &message, IClientHandler *sender,
                     Server *server) override;

private:
  std::vector<std::unique_ptr<IMessageHandler>> handlers_;
};

#endif // COMPOSITE_MESSAGE_HANDLER_H_
