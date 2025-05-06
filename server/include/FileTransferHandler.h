#ifndef FILE_TRANSFER_HANDLER_H_
#define FILE_TRANSFER_HANDLER_H_

#include "IClientHandler.h"
#include "IMessageHandler.h"
#include "Message.h"
#include "Server.h"

#include <fstream> // For file operations
#include <map>     // To track ongoing transfers
#include <mutex>   // For thread safety
#include <string>

/**
 * @brief Message handler for handling file transfer related messages.
 *
 * This class processes messages of type FILE_TRANSFER_REQUEST, FILE_DATA_CHUNK,
 * and FILE_TRANSFER_COMPLETE on the server side. It manages the state of
 * ongoing file transfers.
 */
class FileTransferHandler : public IMessageHandler {
public:
  /**
   * @brief Constructs a new FileTransferHandler.
   */
  FileTransferHandler() = default;

  /**
   * @brief Destroys the FileTransferHandler. Closes any open file streams.
   */
  ~FileTransferHandler() override;

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
  bool HandleMessage(const Message &message, IClientHandler *sender,
                     Server *server) override;

private:
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
  bool HandleFileTransferRequest(const Message &message, IClientHandler *sender,
                                 Server *server);

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
  bool HandleFileDataChunk(const Message &message, IClientHandler *sender,
                           Server *server);

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
  bool HandleFileTransferComplete(const Message &message,
                                  IClientHandler *sender, Server *server);

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
  bool HandleFileTransferError(const Message &message, IClientHandler *sender,
                               Server *server);

  /**
   * @brief Sends a file transfer error message to a client.
   *
   * @param recipient_id The ID of the client to send the error to.
   * @param error_message The error description.
   * @param server A pointer to the Server instance.
   */
  void SendFileTransferError(int recipient_id, const std::string &error_message,
                             Server *server);

  /**
   * @brief Structure to hold the state of an incoming file transfer on the
   * server.
   */
  struct IncomingFileTransfer {
    std::string file_name;
    size_t total_size;
    size_t received_size;
    std::ofstream file_stream;
    int sender_id;
    int recipient_id;

    // Constructor
    IncomingFileTransfer(const std::string &name, size_t size, int sender,
                         int recipient)
        : file_name(name), total_size(size), received_size(0),
          sender_id(sender), recipient_id(recipient) {}
  };

  // Map to track incoming file transfers: recipient_id -> IncomingFileTransfer
  std::map<int, IncomingFileTransfer> incoming_transfers_;
  std::mutex transfers_mutex_; // Mutex to protect access to incoming_transfers_
};

#endif // FILE_TRANSFER_HANDLER_H_
