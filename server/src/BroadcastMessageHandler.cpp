#include "BroadcastMessageHandler.h"
#include "IClientHandler.h" // Include IClientHandler.h
#include "Server.h"         // Include Server.h to use Server methods

#include <iostream>
#include <string> // For std::string constructor from char vector

/**
 * @brief Handles an incoming message by broadcasting it to all clients.
 * @param message The deserialized Message object received.
 * @param sender The client handler that received the message.
 * @param server A pointer to the Server instance for broadcasting.
 * @return True if the message was handled successfully, false otherwise.
 */
bool BroadcastMessageHandler::HandleMessage(const Message &message,
                                            IClientHandler *sender,
                                            Server *server) {
  if (server == nullptr) {
    std::cerr << "Error: Server pointer is null in BroadcastMessageHandler."
              << std::endl;
    return false;
  }
  if (sender == nullptr) {
    std::cerr << "Error: Sender pointer is null in BroadcastMessageHandler."
              << std::endl;
    return false;
  }

  if (message.header.type != MessageType::BROADCAST_MESSAGE) {
    return false;
  }

  // Assuming the payload is a simple chat message string
  // Convert payload (vector<char>) to string
  std::string chat_message(message.payload.begin(), message.payload.end());

  std::string broadcast_message =
      "Client " + std::to_string(sender->GetClientId()) + ": " + chat_message;

  std::cout << "Broadcasting: " << broadcast_message << std::endl;

  // Broadcast the message to all connected clients
  // Need to create a new Message object for broadcasting
  Message broadcast_msg;
  broadcast_msg.header.type = MessageType::BROADCAST_MESSAGE;
  broadcast_msg.header.sender_id = sender->GetClientId();
  broadcast_msg.header.recipient_id = -1;
  broadcast_msg.payload.assign(broadcast_message.begin(),
                               broadcast_message.end());
  broadcast_msg.header.payload_size = broadcast_msg.payload.size();

  server->BroadcastMessage(broadcast_msg, sender);

  return true;
}
