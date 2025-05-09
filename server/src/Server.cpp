#include "Server.h"

#include <algorithm> // For std::remove_if
#include <chrono>    // For std::chrono::milliseconds
#include <iostream>
#include <string>  // For std::to_string
#include <thread>  // For std::this_thread::sleep_for
#include <utility> // For std::move
#include <vector>  // For std::vector

// Include the correct socket implementation header based on the platform
#ifdef _WIN32
#include "WinsockSocket.h"
#else
#include "PosixSocket.h"
#endif

#include "ClientHandler.h" // Use ClientHandler implementation

/**
 * @brief Constructs a new Server object.
 * @param port The port number the server will listen on.
 * @param server_socket The socket to use for listening (dependency injected).
 * @param message_handler The message handler to use for processing client messages (dependency injected).
 */
Server::Server(int port, std::unique_ptr<ISocket> server_socket, std::unique_ptr<IMessageHandler> message_handler)
    : port_(port), server_socket_(std::move(server_socket)), // Store the injected socket
      message_handler_(std::move(message_handler)), running_(false), next_client_id_(1), cleanup_running_(false) {
} // Initialize cleanup flag

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

  if (!server_socket_->Listen(10)) { // Max 10 pending connections
    std::cerr << "Failed to listen on server socket." << std::endl;
    return false;
  }

  running_.store(true);
  cleanup_running_.store(true); // Start the cleanup thread flag

  // Start the accept and cleanup threads
  accept_thread_ = std::thread(&Server::AcceptConnections, this);
  cleanup_thread_ = std::thread(&Server::CleanupClients, this);

  std::cout << "Server started and listening on port " << port_ << std::endl;

  return true;
}

/**
 * @brief Stops the server and disconnects all clients.
 */
void Server::Stop() {
  if (running_.load()) {
    running_.store(false);
    cleanup_running_.store(false); // Signal cleanup thread to stop

    // Notify the cleanup thread to wake up and check the flag
    finished_clients_cv_.notify_one();

    // Close the server socket to unblock the Accept call
    if (server_socket_ && server_socket_->IsValid()) {
      server_socket_->Close();
    }

    // Join the accept thread
    if (accept_thread_.joinable()) {
      accept_thread_.join();
    }

    // Join the cleanup thread
    if (cleanup_thread_.joinable()) {
      cleanup_thread_.join();
    }

    // Stop all client handlers and join their threads
    // This is now primarily for ensuring threads are joined during clean shutdown,
    // as the cleanup thread handles removal during normal operation.
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (const auto &client : clients_) {
      client->Stop(); // Signal stop and join the thread within ClientHandler::Stop
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
      // Note: This might block if the client is not ready to receive immediately.
      // In a real server, this initial message might be handled differently
      // or the ClientHandler's send queue would be used. For simplicity,
      // we send directly here.
      client_handler->SendMessage(id_assignment_msg);

      // Add the client handler to the list and start its thread
      std::lock_guard<std::mutex> lock(clients_mutex_);
      clients_.push_back(std::move(client_handler));
      clients_.back()->Start(); // Start the thread for the new client
    } else {
      // Error accepting connection or server is stopping
      if (!running_.load()) {
        // Server is stopping, exit the loop
        break;
      }
      std::cerr << "Error accepting connection." << std::endl;
    }
  }

  // Ensure server socket is closed when the loop exits
  if (server_socket_ && server_socket_->IsValid()) {
    server_socket_->Close();
  }
}

/**
 * @brief The main loop for the cleanup thread.
 *
 * This function waits for signals from finished client handlers
 * and cleans up their resources.
 */
void Server::CleanupClients() {
  std::cout << "Cleanup thread started." << std::endl;
  while (cleanup_running_.load()) {
    int client_id_to_remove = -1;
    {
      std::unique_lock<std::mutex> lock(finished_clients_mutex_);
      // Wait for a client to be finished or for the cleanup thread to stop
      finished_clients_cv_.wait(lock, [&] { return !finished_client_ids_.empty() || !cleanup_running_.load(); });

      if (!cleanup_running_.load()) {
        break; // Exit the loop if the thread is stopping
      }

      if (!finished_client_ids_.empty()) {
        client_id_to_remove = finished_client_ids_.front();
        finished_client_ids_.pop();
      }
    }

    if (client_id_to_remove != -1) {
      // Now remove the client from the main clients list
      RemoveClient(client_id_to_remove);
    }
  }
  std::cout << "Cleanup thread stopped." << std::endl;
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
      // Ensure the client pointer is still valid before sending.
      if (client) {
        // For simplicity, sending directly. In a real server, this would
        // likely add the message to the client handler's send queue.
        // Need to ensure SendMessage takes a Message object.
        // (It does in the latest ClientHandler versions)
        client->SendMessage(message);
      }
    }
  }
}

/**
 * @brief Removes a client handler from the server's list.
 *
 * This method is called by the cleanup thread to remove a finished client.
 * Assumes clients_mutex_ is already locked.
 *
 * @param client_handler_id The ID of the client handler to remove.
 */
void Server::RemoveClient(int client_handler_id) {
  // The cleanup thread calls this method after getting the client ID from the queue.
  // We need to acquire the clients_mutex_ here because this method modifies the clients_ vector.
  std::lock_guard<std::mutex> lock(clients_mutex_);

  // Find the client handler by ID
  auto it = std::remove_if(clients_.begin(), clients_.end(), [&](const std::unique_ptr<IClientHandler> &client) {
    return client && client->GetClientId() == client_handler_id;
  });

  if (it != clients_.end()) {
    // Before erasing, explicitly stop and join the client handler's thread.
    // This is crucial and happens in the cleanup thread, not the client handler's own thread.
    // Need to cast back to ClientHandler to access the thread member for joining.
    // This is acceptable for this project's scope.
    static_cast<ClientHandler *>(it->get())->Stop(); // Stop signals and joins the thread

    // Now erase the unique_ptr from the vector
    clients_.erase(it, clients_.end());
    std::cout << "Removed client " << client_handler_id << " from the list." << std::endl;
    // The unique_ptr will automatically delete the ClientHandler object
    // when it's removed from the vector.
  } else {
    std::cerr << "Warning: Attempted to remove client ID " << client_handler_id << " but could not find it in the list."
              << std::endl;
  }
}

/**
 * @brief Gets a client handler by its ID.
 * @param client_id The ID of the client handler to retrieve.
 * @return A pointer to the ClientHandler if found, nullptr otherwise.
 */
IClientHandler *Server::GetClientHandler(int client_id) {
  std::lock_guard<std::mutex> lock(clients_mutex_);
  for (const auto &client : clients_) {
    if (client && client->GetClientId() == client_id) {
      return client.get();
    }
  }
  return nullptr; // Client not found
}

/**
 * @brief Signals that a client handler has finished and is ready for cleanup.
 * @param client_id The ID of the client that finished.
 */
void Server::SignalClientFinished(int client_id) {
  {
    std::lock_guard<std::mutex> lock(finished_clients_mutex_);
    finished_client_ids_.push(client_id);
  }
  finished_clients_cv_.notify_one(); // Notify the cleanup thread
}
