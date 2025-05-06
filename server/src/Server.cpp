#include "Server.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include "WinsockSocket.h"
#else
#include "PosixSocket.h"
#endif

#include "ClientHandler.h"

/**
 * @brief Constructs a new Server object.
 *
 * @param port The port number the server will listen on.
 * @param server_socket The socket to use for listening (dependency injected).
 * @param message_handler The message handler to use for processing client messages (dependency injected).
 */
Server::Server(int port, std::unique_ptr<ISocket> server_socket, std::unique_ptr<IMessageHandler> message_handler)
    : port_(port), server_socket_(std::move(server_socket)), // Store the injected socket
      message_handler_(std::move(message_handler)), running_(false), next_client_id_(1) {}

/**
 * @brief Destroys the Server object. Stops the server and cleans up clients.
 */
Server::~Server() {
  Stop();
}

/**
 * @brief Starts the server, binds to the port, and begins listening.
 * @return True if the server starts successfully, false otherwise.
 */
bool Server::Start() {
  if (!server_socket_->Bind("0.0.0.0", port_)) {
    std::cerr << "Failed to bind server socket." << std::endl;
    return false;
  }

  if (!server_socket_->Listen(10)) {
    std::cerr << "Failed to listen on server socket." << std::endl;
    return false;
  }

  running_.store(true);
  std::cout << "Server started and listening on port " << port_ << std::endl;

  return true;
}

/**
 * @brief Stops the server and disconnects all clients.
 */
void Server::Stop() {
  if (running_.load()) {
    running_.store(false);
    // Close the server socket to unblock the Accept call
    if (server_socket_ && server_socket_->IsValid()) {
      server_socket_->Close();
    }

    // Stop all client handlers
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (const auto &client : clients_) {
      client->Stop();
    }
    clients_.clear(); // Clear the list after stopping
    std::cout << "Server stopped." << std::endl;
  }
}

/**
 * @brief Main loop for accepting incoming client connections.
 *
 * This method runs indefinitely until the server is stopped.
 */
void Server::AcceptConnections() {
  while (running_.load()) {
    std::cout << "Waiting for connections..." << std::endl;
    // Use the injected server_socket_ to accept connections
    std::unique_ptr<ISocket> client_socket(server_socket_->Accept());

    if (client_socket && client_socket->IsValid()) {
      int assigned_client_id = next_client_id_++;
      std::cout << "Accepted new connection. Assigning ID: " << assigned_client_id << std::endl;

      // Create a new client handler for the accepted connection
      auto client_handler =
          std::make_unique<ClientHandler>(assigned_client_id, std::move(client_socket), this, message_handler_.get());

      // Send the assigned client ID back to the client
      Message id_assignment_msg;
      id_assignment_msg.header.type = MessageType::CLIENT_ID_ASSIGNMENT;
      id_assignment_msg.header.sender_id = -1;                    // Server is the sender (-1 indicates server)
      id_assignment_msg.header.recipient_id = assigned_client_id; // Message is for this specific client

      // Payload contains the client ID as a string
      std::string id_str = std::to_string(assigned_client_id);
      id_assignment_msg.payload.assign(id_str.begin(), id_str.end());
      id_assignment_msg.header.payload_size = id_assignment_msg.payload.size();

      // Send the message using the client handler's SendMessage method
      client_handler->SendMessage(id_assignment_msg);

      // Add the client handler to the list and start its thread
      std::lock_guard<std::mutex> lock(clients_mutex_);
      clients_.push_back(std::move(client_handler));
      clients_.back()->Start(); // Start the thread for the new client
    } else {
      // Error accepting connection or server is stopping
      if (!running_.load()) {
        break;
      }
      std::cerr << "Error accepting connection." << std::endl;
    }
  }
}

/**
 * @brief Broadcasts a message to all connected clients except the sender.
 * @param message The message to broadcast.
 * @param sender The client handler that sent the message (can be nullptr).
 */
void Server::BroadcastMessage(const Message &message, IClientHandler *sender) {
  std::lock_guard<std::mutex> lock(clients_mutex_);
  for (const auto &client : clients_) {
    // Send message to all clients except the sender
    if (client.get() != sender) {
      client->SendMessage(message);
    }
  }
}

/**
 * @brief Removes a client handler from the server's list.
 * @param client_handler The client handler to remove.
 */
void Server::RemoveClient(IClientHandler *client_handler) {
  std::lock_guard<std::mutex> lock(clients_mutex_);
  // Find and remove the client handler
  // The unique_ptr will automatically delete the ClientHandler object when it's removed from the vector.
  clients_.erase(
      std::remove_if(clients_.begin(), clients_.end(),
                     [&](const std::unique_ptr<IClientHandler> &client) { return client.get() == client_handler; }),
      clients_.end());
  std::cout << "Removed client " << client_handler->GetClientId() << " from the list." << std::endl;
}

/**
 * @brief Gets a client handler by its ID.
 * @param client_id The ID of the client handler to retrieve.
 * @return A pointer to the ClientHandler if found, nullptr otherwise.
 */
IClientHandler *Server::GetClientHandler(int client_id) {
  std::lock_guard<std::mutex> lock(clients_mutex_);
  for (const auto &client : clients_) {
    if (client->GetClientId() == client_id) {
      return client.get();
    }
  }
  return nullptr;
}
