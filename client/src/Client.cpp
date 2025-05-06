#include "Client.h"

#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include "WinsockSocket.h"
#else
#include "PosixSocket.h"
#endif

#include "ClientFileTransferHandler.h"

/**
 * @brief Constructs a new Client object.
 * @param server_address The IP address or hostname of the server.
 * @param server_port The port number of the server.
 */
Client::Client(const std::string &server_address, int server_port)
    : server_address_(server_address), server_port_(server_port),
// Instantiate the correct socket type based on the platform
#ifdef _WIN32
      server_socket_(std::make_unique<WinsockSocket>()),
#else
      server_socket_(std::make_unique<PosixSocket>()),
#endif
      sending_(false), receiving_(false), client_id_(-1) // Initialize client ID to -1
{
  // Create the file transfer handler and inject dependencies (references to queue, mutex, CV, ID)
  file_transfer_handler_ =
      std::make_unique<ClientFileTransferHandler>(send_queue_, send_queue_mutex_, send_queue_cv_, client_id_);
}

/**
 * @brief Destroys the Client object. Disconnects from the server.
 */
Client::~Client() {
  Disconnect();
}

/**
 * @brief Connects the client to the server.
 * @return True if connection is successful, false otherwise.
 */
bool Client::Connect() {
  if (server_socket_->Connect(server_address_, server_port_)) {
    std::cout << "Connected to server at " << server_address_ << ":" << server_port_ << std::endl;
    // Start the send and receive threads after successful connection
    StartSendThread();
    StartReceiveThread();
    return true;
  } else {
    std::cerr << "Failed to connect to server." << std::endl;
    return false;
  }
}

/**
 * @brief Disconnects the client from the server.
 */
void Client::Disconnect() {
  StopSendThread();
  StopReceiveThread();
  if (server_socket_ && server_socket_->IsValid()) {
    server_socket_->Close();
    std::cout << "Disconnected from server." << std::endl;
  }
}

/**
 * @brief Sends a chat message to the server.
 * @param message The message string to send.
 * @return True if the message was sent successfully, false otherwise.
 */
bool Client::SendChatMessage(const std::string &message) {
  if (!server_socket_ || !server_socket_->IsValid() || client_id_.load() == -1) {
    std::cerr << "Error: Not connected to server or client ID not assigned." << std::endl;
    return false;
  }

  Message chat_msg;
  chat_msg.header.type = MessageType::BROADCAST_MESSAGE; // Default to broadcast for simplicity
  chat_msg.header.sender_id = client_id_.load();
  chat_msg.header.recipient_id = -1; // Broadcast recipient
  chat_msg.payload.assign(message.begin(), message.end());
  chat_msg.header.payload_size = chat_msg.payload.size();

  // Add the message to the send queue
  {
    std::lock_guard<std::mutex> lock(send_queue_mutex_);
    send_queue_.push(chat_msg);
  }
  send_queue_cv_.notify_one(); // Notify the send thread

  return true;
}

/**
 * @brief Initiates a file transfer request to a recipient.
 *
 * Delegates the request to the file transfer handler.
 *
 * @param recipient_id The ID of the client to send the file to.
 * @param file_path The path to the file to send.
 * @return True if the request was successfully initiated, false otherwise.
 */
bool Client::RequestFileTransfer(int recipient_id, const std::string &file_path) {
  if (!file_transfer_handler_) {
    std::cerr << "Error: File transfer handler not initialized." << std::endl;
    return false;
  }
  // Delegate the request to the file transfer handler
  return file_transfer_handler_->RequestFileTransfer(recipient_id, file_path);
}

/**
 * @brief Starts the thread for sending messages to the server.
 */
void Client::StartSendThread() {
  if (!sending_.load()) {
    sending_.store(true);
    send_thread_ = std::thread(&Client::SendMessages, this);
  }
}

/**
 * @brief Stops the send thread.
 */
void Client::StopSendThread() {
  if (sending_.load()) {
    sending_.store(false);
    send_queue_cv_.notify_one(); // Notify the send thread to wake up and exit
    if (send_thread_.joinable()) {
      send_thread_.join();
    }
  }
}

/**
 * @brief Starts the thread for receiving messages from the server.
 */
void Client::StartReceiveThread() {
  if (!receiving_.load() && server_socket_ && server_socket_->IsValid()) {
    receiving_.store(true);
    receive_thread_ = std::thread(&Client::ReceiveMessages, this);
  }
}

/**
 * @brief Stops the receive thread.
 */
void Client::StopReceiveThread() {
  if (receiving_.load()) {
    receiving_.store(false);
    // Close the socket to unblock the Receive call in the thread
    if (server_socket_ && server_socket_->IsValid()) {
      server_socket_->Close();
    }
    if (receive_thread_.joinable()) {
      receive_thread_.join();
    }
  }
}

/**
 * @brief Gets the client's assigned ID from the server.
 * @return The client ID, or -1 if not yet assigned.
 */
int Client::GetClientId() const {
  return client_id_.load();
}

/**
 * @brief The main loop for the send thread.
 *
 * This function sends messages from a queue and handles sending file chunks.
 */
void Client::SendMessages() {
  std::cout << "Send thread started." << std::endl;

  while (sending_.load() && server_socket_ && server_socket_->IsValid()) {
    Message message_to_send;
    bool has_message = false;

    // Wait for a message in the queue or for the thread to stop
    {
      std::unique_lock<std::mutex> lock(send_queue_mutex_);
      // Wait for a message or for the sending flag to become false
      send_queue_cv_.wait(lock, [&] { return !send_queue_.empty() || !sending_.load(); });

      if (!sending_.load()) {
        break; // Exit the loop if the thread is stopping
      }

      if (!send_queue_.empty()) {
        message_to_send = send_queue_.front();
        send_queue_.pop();
        has_message = true;
      }
    }

    if (has_message) {
      // Send the message
      if (!SendMessage(message_to_send)) {
        std::cerr << "Failed to send message of type " << static_cast<int>(message_to_send.header.type) << std::endl;
        // In a real application, you might want to retry sending or handle the error.
      }
    }
  }

  // Ensure socket is closed when the thread exits
  if (server_socket_ && server_socket_->IsValid()) {
    server_socket_->Close();
  }

  std::cout << "Send thread stopped." << std::endl;
}

/**
 * @brief The main loop for the receive thread.
 *
 * This function continuously receives data from the server socket,
 * reassembles messages, and processes them.
 */
void Client::ReceiveMessages() {
  std::cout << "Receive thread started." << std::endl;

  std::vector<char> buffer(1024); // Temporary buffer for receiving data

  while (receiving_.load() && server_socket_ && server_socket_->IsValid()) {
    int bytes_received = server_socket_->Receive(buffer.data(), buffer.size());

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

          // Process the received message
          ProcessReceivedMessage(received_message);

        } else {
          // Not enough data for a complete message yet, wait for more
          break;
        }
      }
    } else if (bytes_received == 0) {
      // Connection closed by server
      std::cout << "Server disconnected." << std::endl;
      receiving_.store(false); // Stop the thread loop
      // No need to call Disconnect() here as it would try to join the
      // very thread we are in, leading to a deadlock.
      // The main thread should handle the client shutdown.
    } else {
      // Error occurred
      // Check errno/WSAGetLastError to see if it's a non-fatal error like EAGAIN/EWOULDBLOCK
      // For simplicity, we treat any receive error as connection closure here
#ifdef _WIN32
      int error_code = WSAGetLastError();
      // Check for non-blocking errors if implementing non-blocking sockets
      // if (error_code == WSAEWOULDBLOCK) { continue; }
      std::cerr << "Error receiving data from server: " << error_code << ". Disconnecting." << std::endl;
#else
      // Check for non-blocking errors if implementing non-blocking sockets
      // if (errno == EAGAIN || errno == EWOULDBLOCK) { continue; }
      std::cerr << "Error receiving data from server: " << strerror(errno) << ". Disconnecting." << std::endl;
#endif
      receiving_.store(false); // Stop the thread loop
      // Similar to bytes_received == 0, avoid calling Disconnect() here.
    }
  }

  // Ensure socket is closed when the thread exits
  if (server_socket_ && server_socket_->IsValid()) {
    server_socket_->Close();
  }

  std::cout << "Receive thread stopped." << std::endl;
}

/**
 * @brief Processes a received message based on its type.
 *
 * Delegates handling of specific message types to appropriate handlers.
 *
 * @param message The deserialized Message object.
 */
void Client::ProcessReceivedMessage(const Message &message) {
  // Check if the message is for file transfer and delegate if it is
  if (message.header.type == MessageType::FILE_TRANSFER_REQUEST ||
      message.header.type == MessageType::FILE_DATA_CHUNK ||
      message.header.type == MessageType::FILE_TRANSFER_COMPLETE ||
      message.header.type == MessageType::FILE_TRANSFER_ERROR) {

    if (file_transfer_handler_) {
      file_transfer_handler_->HandleMessage(message);
    } else {
      std::cerr << "Received file transfer message but no handler is available." << std::endl;
    }
    return; // Message handled by file transfer handler
  }

  switch (message.header.type) {
  case MessageType::CLIENT_ID_ASSIGNMENT: { // Handle client ID assignment
    if (!message.payload.empty()) {
      std::string id_str(message.payload.begin(), message.payload.end());
      try {
        int assigned_id = std::stoi(id_str);
        client_id_.store(assigned_id);
        std::cout << "Assigned Client ID: " << assigned_id << std::endl;
      } catch (const std::exception &e) {
        std::cerr << "Error processing client ID assignment message: " << e.what() << std::endl;
      }
    } else {
      std::cerr << "Received empty payload for client ID assignment." << std::endl;
    }
    break;
  }
  case MessageType::BROADCAST_MESSAGE: {
    // Assuming broadcast message payload is a string
    std::string chat_message(message.payload.begin(), message.payload.end());
    std::cout << chat_message << std::endl;
    break;
  }
  case MessageType::PRIVATE_MESSAGE: {
    // Not implemented yet, but would handle private messages here
    std::string private_msg_content(message.payload.begin(), message.payload.end());
    std::cout << "Private message from Client " << message.header.sender_id << ": " << private_msg_content << std::endl;
    break;
  }
  default:
    // Handle unknown or unhandled message types
    std::cerr << "Received unknown message type: " << static_cast<int>(message.header.type) << std::endl;
    break;
  }
}

/**
 * @brief Sends a Message object to the server.
 * @param message The Message object to send.
 * @return True if the message was sent successfully, false otherwise.
 */
bool Client::SendMessage(const Message &message) {
  if (!server_socket_ || !server_socket_->IsValid()) {
    std::cerr << "Error: Not connected to server." << std::endl;
    return false;
  }

  // Serialize the message into a byte vector
  std::vector<char> data_to_send = SerializeMessage(message);

  // Send the serialized data. In a real application, you would handle partial sends.
  int bytes_sent = server_socket_->Send(data_to_send.data(), data_to_send.size());

  if (bytes_sent < 0) {
    std::cerr << "Error sending message." << std::endl;
    // Disconnect() is called by the receive thread on socket error
    return false;
  }
  // Note: bytes_sent could be less than the data_to_send size in case of partial
  // sends. For simplicity, we assume full send or error for now.

  return true;
}
