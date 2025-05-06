#include "ClientFileTransferHandler.h"

#include <filesystem> // For file size (C++17)
#include <iostream>
#include <limits>  // For numeric_limits
#include <sstream> // For stringstream
#include <string>  // For std::string
#include <vector>


// Define a directory to store incoming files on the client side
const std::string kClientIncomingFilesDir = "client_incoming_files";

/**
 * @brief Constructs a new ClientFileTransferHandler.
 * @param send_queue A reference to the client's message send queue.
 * @param send_queue_mutex A reference to the mutex protecting the send queue.
 * @param send_queue_cv A reference to the condition variable for the send queue.
 * @param client_id A reference to the client's atomic ID.
 */
ClientFileTransferHandler::ClientFileTransferHandler(std::queue<Message> &send_queue, std::mutex &send_queue_mutex,
                                                     std::condition_variable &send_queue_cv,
                                                     std::atomic<int> &client_id)
    : send_queue_(send_queue), send_queue_mutex_(send_queue_mutex), send_queue_cv_(send_queue_cv),
      client_id_(client_id) {}

/**
 * @brief Destroys the ClientFileTransferHandler. Closes any open file streams.
 */
ClientFileTransferHandler::~ClientFileTransferHandler() {
  // Close any open file streams for ongoing transfers
  std::lock_guard<std::mutex> outgoing_lock(outgoing_transfer_mutex_);
  if (outgoing_transfer_ && outgoing_transfer_->file_stream.is_open()) {
    outgoing_transfer_->file_stream.close();
  }

  std::lock_guard<std::mutex> incoming_lock(incoming_transfer_mutex_);
  if (incoming_transfer_ && incoming_transfer_->file_stream.is_open()) {
    incoming_transfer_->file_stream.close();
  }
}

/**
 * @brief Initiates a file transfer request to a recipient.
 *
 * This method is called by the Client when the user requests a file transfer.
 * It prepares the file transfer request message and adds it to the send queue.
 *
 * @param recipient_id The ID of the client to send the file to.
 * @param file_path The path to the file to send.
 * @return True if the request was successfully initiated, false otherwise.
 */
bool ClientFileTransferHandler::RequestFileTransfer(int recipient_id, const std::string &file_path) {
  if (client_id_.load() == -1) {
    std::cerr << "Error: Client ID not assigned. Cannot initiate file transfer." << std::endl;
    return false;
  }

  // Check if an outgoing transfer is already in progress
  {
    std::lock_guard<std::mutex> lock(outgoing_transfer_mutex_);
    if (outgoing_transfer_) {
      std::cerr << "Error: An outgoing file transfer is already in progress." << std::endl;
      return false;
    }
  }

  // Check if the file exists and get its size
  std::filesystem::path file_system_path(file_path);
  if (!std::filesystem::exists(file_system_path)) {
    std::cerr << "Error: File not found: " << file_path << std::endl;
    return false;
  }
  if (!std::filesystem::is_regular_file(file_system_path)) {
    std::cerr << "Error: Path is not a regular file: " << file_path << std::endl;
    return false;
  }

  size_t file_size = std::filesystem::file_size(file_system_path);
  std::string file_name = file_system_path.filename().string();

  // Prepare the file transfer request message payload: "recipient_id:file_name:file_size"
  std::stringstream payload_stream;
  payload_stream << recipient_id << ":" << file_name << ":" << file_size;
  std::string payload_str = payload_stream.str();

  Message request_msg;
  request_msg.header.type = MessageType::FILE_TRANSFER_REQUEST;
  request_msg.header.sender_id = client_id_.load();
  request_msg.header.recipient_id = recipient_id; // The intended recipient
  request_msg.payload.assign(payload_str.begin(), payload_str.end());
  request_msg.header.payload_size = request_msg.payload.size();

  // Add the request message to the send queue
  if (AddMessageToSendQueue(request_msg)) {
    std::cout << "Sent file transfer request for '" << file_name << "' to client " << recipient_id << std::endl;

    // Store the state for the outgoing transfer (will be fully set up on READY signal)
    {
      std::lock_guard<std::mutex> lock(outgoing_transfer_mutex_);
      outgoing_transfer_ = std::make_unique<OutgoingFileTransfer>();
      outgoing_transfer_->file_path = file_path;
      outgoing_transfer_->total_size = file_size;
      outgoing_transfer_->recipient_id = recipient_id;
      // File stream will be opened when the server sends the READY signal
    }
    return true;
  } else {
    std::cerr << "Failed to add file transfer request to send queue." << std::endl;
    return false;
  }
}

/**
 * @brief Handles an incoming message related to file transfer.
 *
 * This method is called by the Client's receive logic when a file-transfer-related
 * message is received. It processes the message based on its type
 * (request, data chunk, complete, error).
 *
 * @param message The deserialized Message object.
 */
void ClientFileTransferHandler::HandleMessage(const Message &message) {
  // Ensure the message is actually for this client (either recipient or sender of error)
  // or a server-sent message (sender_id -1)
  if (message.header.recipient_id != client_id_.load() && message.header.recipient_id != -1 &&
      message.header.sender_id != -1) {
    // Message is not for this client, ignore
    return;
  }

  switch (message.header.type) {
  case MessageType::FILE_TRANSFER_REQUEST:
    HandleFileTransferRequest(message);
    break;
  case MessageType::FILE_DATA_CHUNK:
    HandleFileDataChunk(message);
    break;
  case MessageType::FILE_TRANSFER_COMPLETE:
    HandleFileTransferComplete(message);
    break;
  case MessageType::FILE_TRANSFER_ERROR:
    HandleFileTransferError(message);
    break;
  default:
    // Not a file transfer message type handled by this handler
    // The Client::ProcessReceivedMessage will handle other types
    break;
  }
}

/**
 * @brief Sends the next file data chunk by adding it to the send queue.
 *
 * Called internally when the outgoing transfer is ready.
 * @return True if a chunk was successfully added to the queue, false otherwise.
 */
bool ClientFileTransferHandler::SendNextFileChunkToQueue() {
  std::lock_guard<std::mutex> lock(outgoing_transfer_mutex_);
  if (outgoing_transfer_ && outgoing_transfer_->file_stream.is_open() &&
      outgoing_transfer_->sent_size < outgoing_transfer_->total_size) {

    std::vector<char> chunk_data(kFileChunkSize);
    size_t bytes_to_read = std::min(kFileChunkSize, outgoing_transfer_->total_size - outgoing_transfer_->sent_size);

    outgoing_transfer_->file_stream.read(chunk_data.data(), bytes_to_read);
    size_t bytes_read = outgoing_transfer_->file_stream.gcount();

    if (bytes_read > 0) {
      Message chunk_msg;
      chunk_msg.header.type = MessageType::FILE_DATA_CHUNK;
      chunk_msg.header.sender_id = client_id_.load();
      chunk_msg.header.recipient_id = outgoing_transfer_->recipient_id;
      chunk_msg.payload.assign(chunk_data.begin(), chunk_data.begin() + bytes_read);
      chunk_msg.header.payload_size = chunk_msg.payload.size();

      // Add the data chunk message to the send queue
      if (AddMessageToSendQueue(chunk_msg)) {
        outgoing_transfer_->sent_size += bytes_read;
        // std::cout << "Queued chunk: " << outgoing_transfer_->sent_size << "/"
        //           << outgoing_transfer_->total_size << std::endl;

        if (outgoing_transfer_->sent_size == outgoing_transfer_->total_size) {
          // File transfer complete
          std::cout << "File transfer complete for '" << outgoing_transfer_->file_path << "'" << std::endl;

          Message complete_msg;
          complete_msg.header.type = MessageType::FILE_TRANSFER_COMPLETE;
          complete_msg.header.sender_id = client_id_.load();
          complete_msg.header.recipient_id = outgoing_transfer_->recipient_id;
          complete_msg.header.payload_size = 0; // No payload for completion message

          // Add the completion message to the queue to be sent
          AddMessageToSendQueue(complete_msg);

          // Clean up outgoing transfer state
          outgoing_transfer_->file_stream.close();
          outgoing_transfer_.reset();
        }
        return true; // A chunk was successfully queued
      } else {
        std::cerr << "Failed to add file data chunk to send queue." << std::endl;
        // Handle send error during file transfer (e.g., send error message)
        Message error_msg;
        error_msg.header.type = MessageType::FILE_TRANSFER_ERROR;
        error_msg.header.sender_id = client_id_.load();
        error_msg.header.recipient_id = outgoing_transfer_->recipient_id;      // Error related to this recipient
        std::string error_payload = "Client failed to queue file data chunk."; // Declare string variable
        error_msg.payload.assign(error_payload.begin(), error_payload.end());
        error_msg.header.payload_size = error_msg.payload.size();
        AddMessageToSendQueue(error_msg);

        outgoing_transfer_->file_stream.close();
        outgoing_transfer_.reset(); // Clean up state
        return false;               // Failed to queue chunk
      }
    } else if (outgoing_transfer_->file_stream.eof()) {
      // Reached end of file unexpectedly (should be caught by sent_size == total_size)
      std::cerr << "Unexpected end of file while reading for transfer." << std::endl;
      // Handle error
      Message error_msg;
      error_msg.header.type = MessageType::FILE_TRANSFER_ERROR;
      error_msg.header.sender_id = client_id_.load();
      error_msg.header.recipient_id = outgoing_transfer_->recipient_id;      // Error related to this recipient
      std::string error_payload = "Unexpected end of file during transfer."; // Declare string variable
      error_msg.payload.assign(error_payload.begin(), error_payload.end());
      error_msg.header.payload_size = error_msg.payload.size();
      AddMessageToSendQueue(error_msg);

      outgoing_transfer_->file_stream.close();
      outgoing_transfer_.reset(); // Clean up state
      return false;               // Failed to queue chunk
    } else if (outgoing_transfer_->file_stream.fail()) {
      std::cerr << "File stream failed while reading for transfer." << std::endl;
      // Handle error
      Message error_msg;
      error_msg.header.type = MessageType::FILE_TRANSFER_ERROR;
      error_msg.header.sender_id = client_id_.load();
      error_msg.header.recipient_id = outgoing_transfer_->recipient_id;  // Error related to this recipient
      std::string error_payload = "File stream failed during transfer."; // Declare string variable
      error_msg.payload.assign(error_payload.begin(), error_payload.end());
      error_msg.header.payload_size = error_msg.payload.size();
      AddMessageToSendQueue(error_msg);

      outgoing_transfer_->file_stream.close();
      outgoing_transfer_.reset(); // Clean up state
      return false;               // Failed to queue chunk
    }
  }
  return false; // No chunk was queued
}

/**
 * @brief Handles an incoming file transfer request.
 * @param message The file transfer request message.
 */
void ClientFileTransferHandler::HandleFileTransferRequest(const Message &message) {
  // This is a file transfer request received by the intended recipient client
  // The check for recipient_id is done in HandleMessage, but double-check here
  if (message.header.recipient_id != client_id_.load()) {
    // This request is not for this client, ignore it
    return;
  }

  if (message.payload.empty()) {
    std::cerr << "Invalid incoming file transfer request: empty payload." << std::endl;
    // Send error back to sender via server?
    return;
  }

  // Payload is expected to be "recipient_id:file_name:file_size" from the original sender
  std::string payload_str(message.payload.begin(), message.payload.end());
  size_t first_colon = payload_str.find(':');
  size_t second_colon = payload_str.find(':', first_colon + 1);

  if (first_colon == std::string::npos || second_colon == std::string::npos) {
    std::cerr << "Invalid incoming file transfer request format." << std::endl;
    // Send error back to sender via server?
    return;
  }

  try {
    // We already know the recipient_id (this client's ID)
    std::string file_name = payload_str.substr(first_colon + 1, second_colon - first_colon - 1);
    size_t file_size = std::stoull(payload_str.substr(second_colon + 1));

    std::cout << "Received file transfer request from Client " << message.header.sender_id << " for file: " << file_name
              << " (" << file_size << " bytes)" << std::endl;

    // Check if an incoming transfer is already in progress
    {
      std::lock_guard<std::mutex> lock(incoming_transfer_mutex_);
      if (incoming_transfer_) {
        std::cerr << "Error: An incoming file transfer is already in progress. Cannot accept request for '" << file_name
                  << "'." << std::endl;
        // Send error back to sender via server
        Message error_msg;
        error_msg.header.type = MessageType::FILE_TRANSFER_ERROR;
        error_msg.header.sender_id = client_id_.load();                         // This client is sending the error
        error_msg.header.recipient_id = message.header.sender_id;               // Error goes to the original sender
        std::string error_payload = "Recipient is busy with another transfer."; // Declare string variable
        error_msg.payload.assign(error_payload.begin(), error_payload.end());
        error_msg.header.payload_size = error_msg.payload.size();
        AddMessageToSendQueue(error_msg);
        return; // Handled by sending error
      }

      // Create the incoming files directory if it doesn't exist
      std::filesystem::create_directories(kClientIncomingFilesDir); // Use client-specific constant

      // Create a unique filename to save the incoming file
      std::string unique_file_name = kClientIncomingFilesDir + "/" + // Use client-specific constant
                                     std::to_string(message.header.sender_id) + "_" + file_name;
      std::ofstream output_file(unique_file_name, std::ios::binary);

      if (!output_file.is_open()) {
        std::cerr << "Failed to open file for writing: " << unique_file_name << std::endl;
        // Send error back to sender via server
        Message error_msg;
        error_msg.header.type = MessageType::FILE_TRANSFER_ERROR;
        error_msg.header.sender_id = client_id_.load();                           // This client is sending the error
        error_msg.header.recipient_id = message.header.sender_id;                 // Error goes to the original sender
        std::string error_payload = "Recipient failed to open file for writing."; // Declare string variable
        error_msg.payload.assign(error_payload.begin(), error_payload.end());
        error_msg.header.payload_size = error_msg.payload.size();
        AddMessageToSendQueue(error_msg);
        return; // Handled by sending error
      }

      // Store the state of the incoming transfer
      incoming_transfer_ = std::make_unique<IncomingFileTransfer>();
      incoming_transfer_->file_name = file_name;
      incoming_transfer_->total_size = file_size;
      incoming_transfer_->sender_id = message.header.sender_id;
      incoming_transfer_->file_stream = std::move(output_file);

      std::cout << "Ready to receive file '" << file_name << "' from Client " << message.header.sender_id << std::endl;

      // Send an acknowledgment back to the sender via the server
      Message ack_msg;
      ack_msg.header.type = MessageType::FILE_TRANSFER_REQUEST; // Reuse type, perhaps add a status field later
      ack_msg.header.sender_id = client_id_.load();             // This client is sending the ack
      ack_msg.header.recipient_id = message.header.sender_id;   // Ack goes to the original sender
      std::string ready_payload = "READY";                      // Declare string variable
      ack_msg.payload.assign(ready_payload.begin(), ready_payload.end()); // Simple payload indicating readiness
      ack_msg.header.payload_size = ack_msg.payload.size();
      AddMessageToSendQueue(ack_msg);
    }
  } catch (const std::exception &e) {
    std::cerr << "Error processing incoming file transfer request: " << e.what() << std::endl;
    // Send error back to sender via server
    Message error_msg;
    error_msg.header.type = MessageType::FILE_TRANSFER_ERROR;
    error_msg.header.sender_id = client_id_.load();                        // This client is sending the error
    error_msg.header.recipient_id = message.header.sender_id;              // Error goes to the original sender
    std::string error_payload = "Error processing file transfer request."; // Declare string variable
    error_msg.payload.assign(error_payload.begin(), error_payload.end());
    error_msg.header.payload_size = error_msg.payload.size();
    AddMessageToSendQueue(error_msg);
  }
}

/**
 * @brief Handles an incoming file data chunk.
 * @param message The file data chunk message.
 */
void ClientFileTransferHandler::HandleFileDataChunk(const Message &message) {
  // This client is the recipient of the file data chunk
  std::lock_guard<std::mutex> lock(incoming_transfer_mutex_);
  if (incoming_transfer_ && incoming_transfer_->sender_id == message.header.sender_id) {
    if (incoming_transfer_->file_stream.is_open()) {
      incoming_transfer_->file_stream.write(message.payload.data(), message.payload.size());
      incoming_transfer_->received_size += message.payload.size();

      // Optional: Provide progress updates
      // if (incoming_transfer_->received_size % 102400 == 0 || incoming_transfer_->received_size ==
      // incoming_transfer_->total_size) {
      //     std::cout << "Received " << incoming_transfer_->received_size << "/" << incoming_transfer_->total_size
      //               << " bytes for file " << incoming_transfer_->file_name << " from Client "
      //               << incoming_transfer_->sender_id << std::endl;
      // }

    } else {
      std::cerr << "File stream not open for incoming transfer from Client " << message.header.sender_id << std::endl;
      // Send error back to sender via server
      Message error_msg;
      error_msg.header.type = MessageType::FILE_TRANSFER_ERROR;
      error_msg.header.sender_id = client_id_.load();                // This client is sending the error
      error_msg.header.recipient_id = message.header.sender_id;      // Error goes to the original sender
      std::string error_payload = "Recipient file stream not open."; // Declare string variable
      error_msg.payload.assign(error_payload.begin(), error_payload.end());
      error_msg.header.payload_size = error_msg.payload.size();
      AddMessageToSendQueue(error_msg);

      incoming_transfer_.reset(); // Clean up state
    }
  } else {
    std::cerr << "Received file data chunk for unknown or mismatched transfer from Client " << message.header.sender_id
              << std::endl;
    // Ignore or send an error back
  }
}

/**
 * @brief Handles a file transfer complete message.
 *
 * Finalizes a file transfer and cleans up state.
 *
 * @param message The file transfer complete message.
 */
void ClientFileTransferHandler::HandleFileTransferComplete(const Message &message) {
  // This client is the recipient of the completion message
  std::lock_guard<std::mutex> lock(incoming_transfer_mutex_);
  if (incoming_transfer_ && incoming_transfer_->sender_id == message.header.sender_id) {
    if (incoming_transfer_->file_stream.is_open()) {
      incoming_transfer_->file_stream.close();
    }

    std::cout << "File transfer complete for '" << incoming_transfer_->file_name << "' from Client "
              << incoming_transfer_->sender_id << std::endl;

    // Optional: Verify total size received matches expected total size
    if (incoming_transfer_->received_size != incoming_transfer_->total_size) {
      std::cerr << "Warning: Received size (" << incoming_transfer_->received_size << ") does not match expected size ("
                << incoming_transfer_->total_size << ") for file '" << incoming_transfer_->file_name << "'"
                << std::endl;
      // Send error back to sender via server
      Message error_msg;
      error_msg.header.type = MessageType::FILE_TRANSFER_ERROR;
      error_msg.header.sender_id = client_id_.load();             // This client is sending the error
      error_msg.header.recipient_id = message.header.sender_id;   // Error goes to the original sender
      std::string error_payload = "Received file size mismatch."; // Declare string variable
      error_msg.payload.assign(error_payload.begin(), error_payload.end());
      error_msg.header.payload_size = error_msg.payload.size();
      AddMessageToSendQueue(error_msg);
    }

    incoming_transfer_.reset(); // Clean up state

  } else {
    std::cerr << "Received file transfer complete for unknown or mismatched transfer from Client "
              << message.header.sender_id << std::endl;
    // Ignore
  }
}

/**
 * @brief Handles a file transfer error message.
 * @param message The file transfer error message.
 */
void ClientFileTransferHandler::HandleFileTransferError(const Message &message) {
  // This client is the recipient of the error message
  std::string error_msg_content(message.payload.begin(), message.payload.end());
  std::cerr << "File transfer error from Client " << message.header.sender_id << ": " << error_msg_content << std::endl;

  // Clean up any related transfer state
  {
    std::lock_guard<std::mutex> lock(outgoing_transfer_mutex_);
    if (outgoing_transfer_ && outgoing_transfer_->recipient_id == message.header.sender_id) {
      if (outgoing_transfer_->file_stream.is_open()) {
        outgoing_transfer_->file_stream.close();
      }
      outgoing_transfer_.reset();
      std::cout << "Outgoing file transfer cancelled due to error." << std::endl;
    }
  }
  {
    std::lock_guard<std::mutex> lock(incoming_transfer_mutex_);
    if (incoming_transfer_ && incoming_transfer_->sender_id == message.header.sender_id) {
      if (incoming_transfer_->file_stream.is_open()) {
        incoming_transfer_->file_stream.close();
        // Optional: Delete partially received file
        // std::filesystem::remove(...);
      }
      incoming_transfer_.reset();
      std::cout << "Incoming file transfer cancelled due to error." << std::endl;
    }
  }
}

/**
 * @brief Adds a Message object to the client's send queue.
 * @param message The Message object to add to the queue.
 * @return True if the message was added to the queue, false otherwise.
 */
bool ClientFileTransferHandler::AddMessageToSendQueue(const Message &message) {
  {
    std::lock_guard<std::mutex> lock(send_queue_mutex_);
    send_queue_.push(message);
  }
  send_queue_cv_.notify_one(); // Notify the send thread
  return true;
}
