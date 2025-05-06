#ifndef CLIENT_FILE_TRANSFER_HANDLER_H_
#define CLIENT_FILE_TRANSFER_HANDLER_H_

#include "IClientFileTransferHandler.h"

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "Message.h"
#include "MessageSerialization.h"

/**
 * @brief Handles file transfer operations on the client side.
 *
 * This class implements the IClientFileTransferHandler interface and manages
 * the state and logic for initiating, sending, and receiving file transfers.
 * It interacts with the Client's send queue to send file-related messages.
 */
class ClientFileTransferHandler : public IClientFileTransferHandler {
public:
  /**
   * @brief Constructs a new ClientFileTransferHandler.
   * @param send_queue A reference to the client's message send queue.
   * @param send_queue_mutex A reference to the mutex protecting the send queue.
   * @param send_queue_cv A reference to the condition variable for the send queue.
   * @param client_id A reference to the client's atomic ID.
   */
  ClientFileTransferHandler(std::queue<Message> &send_queue, std::mutex &send_queue_mutex,
                            std::condition_variable &send_queue_cv, std::atomic<int> &client_id);

  /**
   * @brief Destroys the ClientFileTransferHandler. Closes any open file streams.
   */
  ~ClientFileTransferHandler() override;

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
  bool RequestFileTransfer(int recipient_id, const std::string &file_path) override;

  /**
   * @brief Handles an incoming message related to file transfer.
   *
   * This method is called by the Client's receive logic when a file-transfer-related
   * message is received. It processes the message based on its type
   * (request, data chunk, complete, error).
   *
   * @param message The deserialized Message object.
   */
  void HandleMessage(const Message &message) override;

private:
  /**
   * @brief Handles an incoming file transfer request.
   * @param message The file transfer request message.
   */
  void HandleFileTransferRequest(const Message &message);

  /**
   * @brief Handles an incoming file data chunk.
   * @param message The file data chunk message.
   */
  void HandleFileDataChunk(const Message &message);

  /**
   * @brief Handles a file transfer complete message.
   * @param message The file transfer complete message.
   */
  void HandleFileTransferComplete(const Message &message);

  /**
   * @brief Handles a file transfer error message.
   * @param message The file transfer error message.
   */
  void HandleFileTransferError(const Message &message);

  /**
   * @brief Sends the next file data chunk by adding it to the send queue.
   *
   * Called internally when the outgoing transfer is ready.
   * @return True if a chunk was successfully added to the queue, false otherwise.
   */
  bool SendNextFileChunkToQueue();

  /**
   * @brief Adds a Message object to the client's send queue.
   * @param message The Message object to add to the queue.
   * @return True if the message was added to the queue, false otherwise.
   */
  bool AddMessageToSendQueue(const Message &message);

  // State for outgoing file transfers (client sending a file)
  struct OutgoingFileTransfer {
    std::string file_path;
    size_t total_size;
    size_t sent_size;
    std::ifstream file_stream;
    int recipient_id;

    OutgoingFileTransfer() : total_size(0), sent_size(0), recipient_id(-1) {}
  };
  std::unique_ptr<OutgoingFileTransfer> outgoing_transfer_;
  std::mutex outgoing_transfer_mutex_;

  // State for incoming file transfers (client receiving a file)
  struct IncomingFileTransfer {
    std::string file_name;
    size_t total_size;
    size_t received_size;
    std::ofstream file_stream;
    int sender_id;

    IncomingFileTransfer() : total_size(0), received_size(0), sender_id(-1) {}
  };
  std::unique_ptr<IncomingFileTransfer> incoming_transfer_;
  std::mutex incoming_transfer_mutex_;

  const size_t kFileChunkSize = 4096;

  std::queue<Message> &send_queue_;
  std::mutex &send_queue_mutex_;
  std::condition_variable &send_queue_cv_;
  std::atomic<int> &client_id_;
};

#endif // CLIENT_FILE_TRANSFER_HANDLER_H_
