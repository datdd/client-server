// server/src/ClientHandler.cc
#include "ClientHandler.h"

#include <iostream>
#include <string> // For std::string constructor from char vector
#include <thread> // Required for std::thread::joinable and detach
#include <vector>

// Need to include Message and Serialization headers to process incoming data
#include "Message.h"
#include "MessageSerialization.h"

// Need platform-specific headers for error checking
#ifdef _WIN32
#include <winsock2.h>
#else
#include <cstring> // For strerror
#endif

/**
 * @brief Constructs a new ClientHandler.
 * @param client_id A unique ID for this client.
 * @param client_socket The socket connected to the client.
 * @param server A pointer to the Server instance.
 * @param message_handler The message handler to process received messages.
 */
ClientHandler::ClientHandler(int client_id, std::unique_ptr<ISocket> client_socket, Server *server,
                             IMessageHandler *message_handler)
    : client_id_(client_id), client_socket_(std::move(client_socket)), server_(server),
      message_handler_(message_handler), running_(false) {}

/**
 * @brief Destroys the ClientHandler object. Stops the thread if running.
 */
ClientHandler::~ClientHandler() {
  // The cleanup thread in Server::CleanupClients is responsible for calling Stop()
  // and joining this thread before deleting the ClientHandler object.
  // However, if for some reason the thread is still joinable here,
  // detaching prevents the std::thread destructor from calling std::terminate.
  // This acts as a safety net. Ideally, this warning should not appear
  // during normal operation if Stop() was called and joined successfully.
  if (handler_thread_.joinable()) {
    handler_thread_.detach();
    std::cerr << "Warning: ClientHandler thread was detached for client " << client_id_
              << ". Run() did not complete before destructor." << std::endl;
  }
}

/**
 * @brief Starts the client handler thread.
 */
void ClientHandler::Start() {
  if (!running_.load()) {
    running_.store(true);
    handler_thread_ = std::thread(&ClientHandler::Run, this);
  }
}

/**
 * @brief Stops the client handler and closes the connection.
 *
 * This method signals the thread to stop, closes the socket to unblock
 * Receive, and waits for the thread to finish.
 */
void ClientHandler::Stop() {
  // Set the atomic flag 'running_' to false.
  // This signals the Run() loop to terminate.
  running_.store(false);

  // Closing the socket should unblock the Receive call in the Run thread.
  if (client_socket_ && client_socket_->IsValid()) {
    client_socket_->Close();
  }

  // Wait for the thread to finish its Run method.
  // This is called by the cleanup thread or Server::Stop.
  // The joinable() check is still needed here.
  if (handler_thread_.joinable()) {
    handler_thread_.join(); // Wait for the thread to complete
  }
}

/**
 * @brief Sends a message to the connected client.
 * @param message The message structure to send.
 * @return True if the message was sent successfully, false otherwise.
 */
bool ClientHandler::SendMessage(const Message &message) {
  if (!client_socket_ || !client_socket_->IsValid()) {
    std::cerr << "Error: Cannot send message, socket is not valid for client " << client_id_ << std::endl;
    return false;
  }

  // Serialize the message into a byte vector
  std::vector<char> data_to_send = SerializeMessage(message);

  // Send the serialized data. In a real application, you would handle partial sends.
  int bytes_sent = client_socket_->Send(data_to_send.data(), data_to_send.size());

  if (bytes_sent < 0) {
    std::cerr << "Error sending message to client " << client_id_ << std::endl;
    // Don't signal removal here, let the Receive loop detect the broken connection.
    return false;
  }
  // Note: bytes_sent could be less than the data_to_send size in case of partial
  // sends. For simplicity, we assume full send or error for now.

  return true;
}

/**
 * @brief Gets the unique identifier for this client handler.
 * @return The client ID.
 */
int ClientHandler::GetClientId() const {
  return client_id_;
}

/**
 * @brief Gets a pointer to the underlying socket.
 * @return A pointer to the ISocket instance.
 */
ISocket *ClientHandler::GetSocket() const {
  return client_socket_.get();
}

/**
 * @brief The main loop for the client handler thread.
 *
 * This function continuously receives data from the client socket,
 * reassembles messages, and passes them to the message handler.
 */
void ClientHandler::Run() {
  std::cout << "Client handler started for client " << client_id_ << std::endl;

  std::vector<char> buffer(1024); // Temporary buffer for receiving data

  while (running_.load() && client_socket_ && client_socket_->IsValid()) {
    int bytes_received = client_socket_->Receive(buffer.data(), buffer.size());

    if (bytes_received > 0) {
      receive_buffer_.insert(receive_buffer_.end(), buffer.begin(), buffer.begin() + bytes_received);

      // Process messages from the receive buffer
      while (receive_buffer_.size() >= kMessageHeaderSize) {
        // Peek at the header to get the payload size
        MessageHeader header;
        std::memcpy(&header, receive_buffer_.data(), kMessageHeaderSize);

        size_t total_message_size = kMessageHeaderSize + header.payload_size;

        if (receive_buffer_.size() >= total_message_size) {
          // We have a complete message
          std::vector<char> complete_message_data(receive_buffer_.begin(),
                                                  receive_buffer_.begin() + total_message_size);

          // Deserialize the message
          Message received_message = DeserializeMessage(complete_message_data);

          // Remove the processed message from the buffer
          receive_buffer_.erase(receive_buffer_.begin(), receive_buffer_.begin() + total_message_size);

          // Handle the message using the message handler
          if (message_handler_ && server_) {
            if (message_handler_->HandleMessage(received_message, this, server_)) {
              // Message handled successfully
            } else {
              std::cerr << "Message handler failed to process message from client " << client_id_ << std::endl;
            }
          } else {
            std::cerr << "No message handler or server available for client " << client_id_ << std::endl;
          }
        } else {
          // Not enough data for a complete message yet, wait for more
          break;
        }
      }
    } else if (bytes_received == 0) {
      // Connection closed by client
      std::cout << "Client " << client_id_ << " disconnected." << std::endl;
      running_.store(false); // Stop the thread loop
      // Signal the server that this client handler is finished
      if (server_) {
        server_->SignalClientFinished(client_id_);
      }
    } else {
      // Error occurred
      // Check errno/WSAGetLastError to see if it's a non-fatal error like EAGAIN/EWOULDBLOCK
      // For simplicity, we treat any receive error as connection closure here
      // Need to handle platform-specific error checking
#ifdef _WIN32
      int error_code = WSAGetLastError();
      // Check for non-blocking errors if implementing non-blocking sockets
      // if (error_code == WSAEWOULDBLOCK) { continue; }
      std::cerr << "Error receiving data from client " << client_id_ << ": " << error_code << ". Disconnecting."
                << std::endl;
#else
      // Check for non-blocking errors if implementing non-blocking sockets
      // if (errno == EAGAIN || errno == EWOULDBLOCK) { continue; }
      std::cerr << "Error receiving data from client " << client_id_ << ": " << strerror(errno) << ". Disconnecting."
                << std::endl;
#endif
      running_.store(false); // Stop the thread loop
      // Signal the server that this client handler is finished
      if (server_) {
        server_->SignalClientFinished(client_id_);
      }
    }
  }

  // Ensure socket is closed when the thread exits
  if (client_socket_ && client_socket_->IsValid()) {
    client_socket_->Close();
  }

  std::cout << "Client handler stopped for client " << client_id_ << std::endl;
}
