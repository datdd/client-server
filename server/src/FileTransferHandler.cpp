#include "FileTransferHandler.h"

#include <filesystem> // For creating directories (C++17)
#include <iostream>

// Define a directory to store incoming files
const std::string kIncomingFilesDir = "incoming_files";

/**
 * @brief Destroys the FileTransferHandler. Closes any open file streams.
 */
FileTransferHandler::~FileTransferHandler() {
  std::lock_guard<std::mutex> lock(transfers_mutex_);
  for (auto &pair : incoming_transfers_) {
    IncomingFileTransfer &transfer = pair.second;
    if (transfer.file_stream.is_open()) {
      transfer.file_stream.close();
    }
  }
}

/**
 * @brief Handles an incoming message related to file transfer.
 *
 * Processes messages based on their type (request, data chunk, complete).
 *
 * @param message The deserialized Message object received.
 * @param sender The client handler that received the message.
 * @param server A pointer to the Server instance for interactions.
 * @return True if the message was handled by this handler, false otherwise.
 */
bool FileTransferHandler::HandleMessage(const Message &message, IClientHandler *sender, Server *server) {
  switch (message.header.type) {
  case MessageType::FILE_TRANSFER_REQUEST:
    return HandleFileTransferRequest(message, sender, server);
  case MessageType::FILE_DATA_CHUNK:
    return HandleFileDataChunk(message, sender, server);
  case MessageType::FILE_TRANSFER_COMPLETE:
    return HandleFileTransferComplete(message, sender, server);
  case MessageType::FILE_TRANSFER_ERROR: {
    return HandleFileTransferError(message, sender, server);
  }
  default:
    return false;
  }
}

/**
 * @brief Handles a file transfer request message.
 *
 * Initiates a new file transfer state on the server.
 *
 * @param message The file transfer request message.
 * @param sender The client handler that sent the request.
 * @param server A pointer to the Server instance.
 * @return True if the request was processed successfully, false otherwise.
 */
bool FileTransferHandler::HandleFileTransferRequest(const Message &message, IClientHandler *sender, Server *server) {
  if (message.payload.empty()) {
    std::cerr << "File transfer request received with empty payload from client " << sender->GetClientId() << std::endl;
    SendFileTransferError(sender->GetClientId(), "Invalid file transfer request.", server);
    return true;
  }

  // Payload is expected to be "recipient_id:file_name:file_size"
  std::string payload_str(message.payload.begin(), message.payload.end());
  size_t first_colon = payload_str.find(':');
  size_t second_colon = payload_str.find(':', first_colon + 1);

  if (first_colon == std::string::npos || second_colon == std::string::npos) {
    std::cerr << "Invalid file transfer request format from client " << sender->GetClientId() << std::endl;
    SendFileTransferError(sender->GetClientId(), "Invalid file transfer request format.", server);
    return true;
  }

  try {
    int recipient_id = std::stoi(payload_str.substr(0, first_colon));
    std::string file_name = payload_str.substr(first_colon + 1, second_colon - first_colon - 1);
    size_t file_size = std::stoull(payload_str.substr(second_colon + 1));

    std::cout << "Received file transfer request from client " << sender->GetClientId() << " to client " << recipient_id
              << " for file: " << file_name << " (" << file_size << " bytes)" << std::endl;

    // Check if the recipient is the server itself (transfer to server)
    if (recipient_id == -1) { // Assuming -1 is a special ID for the server
      std::lock_guard<std::mutex> lock(transfers_mutex_);
      if (incoming_transfers_.count(sender->GetClientId())) {
        std::cerr << "Client " << sender->GetClientId() << " already has an incoming transfer to server." << std::endl;
        SendFileTransferError(sender->GetClientId(), "Another transfer is already in progress.", server);
        return true;
      }

      // Create the incoming files directory if it doesn't exist
      std::filesystem::create_directories(kIncomingFilesDir);

      // Create a unique filename on the server to avoid conflicts
      std::string unique_file_name = kIncomingFilesDir + "/" + std::to_string(sender->GetClientId()) + "_" + file_name;
      std::ofstream output_file(unique_file_name, std::ios::binary);

      if (!output_file.is_open()) {
        std::cerr << "Failed to open file for writing: " << unique_file_name << std::endl;
        SendFileTransferError(sender->GetClientId(), "Server failed to open file for writing.", server);
        return true;
      }

      // Store the state of the incoming transfer
      incoming_transfers_.emplace(sender->GetClientId(),
                                  IncomingFileTransfer(file_name, file_size, sender->GetClientId(),
                                                       -1)); // Recipient -1 for server
      incoming_transfers_.at(sender->GetClientId()).file_stream = std::move(output_file);

      std::cout << "Initiated incoming file transfer from client " << sender->GetClientId()
                << " to server for file: " << file_name << std::endl;

      // Acknowledge the request to the sender (optional, but good practice)
      // Send a message back to the sender indicating the transfer is ready
      Message ready_msg;
      ready_msg.header.type = MessageType::FILE_TRANSFER_REQUEST;
      ready_msg.header.sender_id = -1; // Server is the sender
      ready_msg.header.recipient_id = sender->GetClientId();
      std::string ready_payload = "READY";
      ready_msg.payload.assign(ready_payload.begin(), ready_payload.end());
      ready_msg.header.payload_size = ready_msg.payload.size();
      sender->SendMessage(ready_msg);
    } else {
      IClientHandler *recipient_handler = server->GetClientHandler(recipient_id);

      if (recipient_handler) {
        // Forward the file transfer request message to the recipient
        recipient_handler->SendMessage(message);
        std::cout << "Forwarded file transfer request from client " << sender->GetClientId() << " to client "
                  << recipient_id << std::endl;
      } else {
        std::cerr << "Recipient client " << recipient_id << " not found for file transfer request from client "
                  << sender->GetClientId() << std::endl;
        SendFileTransferError(sender->GetClientId(), "Recipient client not found.", server);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "Error parsing file transfer request from client " << sender->GetClientId() << ": " << e.what()
              << std::endl;
    SendFileTransferError(sender->GetClientId(), "Error processing file transfer request.", server);
  }

  return true;
}

/**
 * @brief Handles a file data chunk message.
 *
 * Appends the received data chunk to the corresponding file transfer.
 *
 * @param message The file data chunk message.
 * @param sender The client handler that sent the chunk.
 * @param server A pointer to the Server instance.
 * @return True if the chunk was processed successfully, false otherwise.
 */
bool FileTransferHandler::HandleFileDataChunk(const Message &message, IClientHandler *sender, Server *server) {
  // Check if the chunk is for a transfer to the server
  if (message.header.recipient_id == -1) {
    // Incoming transfer to the server
    std::lock_guard<std::mutex> lock(transfers_mutex_);
    auto it = incoming_transfers_.find(message.header.sender_id);

    if (it == incoming_transfers_.end()) {
      std::cerr << "Received file data chunk for unknown transfer from client " << message.header.sender_id
                << std::endl;
      SendFileTransferError(sender->GetClientId(), "Received data for unknown transfer.", server);
      return true;
    }

    IncomingFileTransfer &transfer = it->second;

    if (!transfer.file_stream.is_open()) {
      std::cerr << "File stream not open for incoming transfer from client " << sender->GetClientId() << std::endl;
      SendFileTransferError(sender->GetClientId(), "Internal server error during transfer.", server);
      incoming_transfers_.erase(it); // Clean up state
      return true;
    }

    // Write the received data chunk to the file
    transfer.file_stream.write(message.payload.data(), message.payload.size());
    transfer.received_size += message.payload.size();
  } else {
    // Client-to-client transfer, route the data chunk to the recipient
    IClientHandler *recipient_handler =
        server->GetClientHandler(message.header.recipient_id); // Need a method in Server for this

    if (recipient_handler) {
      // Forward the file data chunk message to the recipient
      recipient_handler->SendMessage(message);
      std::cout << "Forwarded file data chunk from client " << sender->GetClientId() << " to client "
                << message.header.recipient_id << std::endl;
    } else {
      std::cerr << "Recipient client " << message.header.recipient_id << " not found for file data chunk from client "
                << sender->GetClientId() << std::endl;
      SendFileTransferError(sender->GetClientId(), "Recipient client disconnected during transfer.", server);
    }
  }

  return true;
}

/**
 * @brief Handles a file transfer complete message.
 *
 * Finalizes a file transfer and cleans up state.
 *
 * @param message The file transfer complete message.
 * @param sender The client handler that sent the completion message.
 * @param server A pointer to the Server instance.
 * @return True if the completion message was processed successfully, false
 * otherwise.
 */
bool FileTransferHandler::HandleFileTransferComplete(const Message &message, IClientHandler *sender, Server *server) {
  // Check if the completion is for a transfer to the server
  if (message.header.recipient_id == -1) {
    // Incoming transfer to the server
    std::lock_guard<std::mutex> lock(transfers_mutex_);
    auto it = incoming_transfers_.find(message.header.sender_id);

    if (it == incoming_transfers_.end()) {
      std::cerr << "Received file transfer complete for unknown transfer from client " << message.header.sender_id
                << std::endl;
      SendFileTransferError(sender->GetClientId(), "Received completion for unknown transfer.", server);
      return true; // Handled, but it was for an unknown transfer
    }

    IncomingFileTransfer &transfer = it->second;

    if (transfer.file_stream.is_open()) {
      transfer.file_stream.close();
    }

    std::cout << "File transfer complete from client " << sender->GetClientId()
              << " to server for file: " << transfer.file_name << std::endl;

    // Optional: Send a completion acknowledgment back to the sender
    Message ack_msg;
    ack_msg.header.type = MessageType::FILE_TRANSFER_COMPLETE; // Reuse type
    ack_msg.header.sender_id = -1;                             // Server is the sender
    ack_msg.header.recipient_id = sender->GetClientId();
    std::string success_payload = "SUCCESS";
    ack_msg.payload.assign(success_payload.begin(), success_payload.end());
    ack_msg.header.payload_size = ack_msg.payload.size();
    sender->SendMessage(ack_msg);

    // Remove the transfer state
    incoming_transfers_.erase(it);
  } else {
    // Client-to-client transfer, route the completion message to the recipient
    IClientHandler *recipient_handler =
        server->GetClientHandler(message.header.recipient_id); // Need a method in Server for this

    if (recipient_handler) {
      // Forward the file transfer complete message to the recipient
      recipient_handler->SendMessage(message);
      std::cout << "Forwarded file transfer complete from client " << sender->GetClientId() << " to client "
                << message.header.recipient_id << std::endl;
    } else {
      std::cerr << "Recipient client " << message.header.recipient_id
                << " not found for file transfer complete from client " << sender->GetClientId() << std::endl;
    }
  }

  return true; // Handled the completion message
}

/**
 * @brief Handles a file transfer error message.
 *
 * Cleans up the state of an ongoing file transfer in case of an error.
 *
 * @param message The file transfer error message.
 * @param sender The client handler that sent the error message.
 * @param server A pointer to the Server instance.
 * @return True if the error was processed successfully, false otherwise.
 */
bool FileTransferHandler::HandleFileTransferError(const Message &message, IClientHandler *sender, Server *server) {
  std::string error_msg(message.payload.begin(), message.payload.end());
  std::cerr << "Received file transfer error from client " << message.header.sender_id << ": " << error_msg
            << std::endl;
  {
    std::lock_guard<std::mutex> lock(transfers_mutex_);
    auto it = incoming_transfers_.find(message.header.sender_id);
    if (it != incoming_transfers_.end()) {
      if (it->second.file_stream.is_open()) {
        it->second.file_stream.close();
      }
      incoming_transfers_.erase(it);
      std::cout << "Cleaned up incoming transfer state for client " << message.header.sender_id << " due to error."
                << std::endl;
    }
  }
  return true;
}

/**
 * @brief Sends a file transfer error message to a client.
 *
 * @param recipient_id The ID of the client to send the error to.
 * @param error_message The error description.
 * @param server A pointer to the Server instance.
 */
void FileTransferHandler::SendFileTransferError(int recipient_id, const std::string &error_message, Server *server) {
  if (server == nullptr) {
    std::cerr << "Error: Server pointer is null when trying to send file "
                 "transfer error."
              << std::endl;
    return;
  }

  IClientHandler *recipient_handler = server->GetClientHandler(recipient_id); // Need a method in Server for this

  if (recipient_handler) {
    Message error_msg;
    error_msg.header.type = MessageType::FILE_TRANSFER_ERROR;
    error_msg.header.sender_id = -1; // Server is the sender
    error_msg.header.recipient_id = recipient_id;
    error_msg.payload.assign(error_message.begin(), error_message.end());
    error_msg.header.payload_size = error_msg.payload.size();
    recipient_handler->SendMessage(error_msg);
    std::cerr << "Sent file transfer error to client " << recipient_id << ": " << error_message << std::endl;
  } else {
    std::cerr << "Could not find recipient client " << recipient_id << " to send file transfer error: " << error_message
              << std::endl;
  }
}
