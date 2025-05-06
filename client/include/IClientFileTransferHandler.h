#ifndef ICLIENT_FILE_TRANSFER_HANDLER_H_
#define ICLIENT_FILE_TRANSFER_HANDLER_H_

#include <string>

#include "Message.h"

// Forward declare Client to avoid circular dependency
class Client;

/**
 * @brief Interface for handling file transfer operations on the client side.
 *
 * This interface defines the contract for initiating, sending, and receiving
 * file transfer related messages and managing the state of transfers.
 */
class IClientFileTransferHandler {
public:
  /**
   * @brief Virtual destructor.
   */
  virtual ~IClientFileTransferHandler() = default;

  /**
   * @brief Initiates a file transfer request to a recipient.
   *
   * This method is called by the Client when the user requests a file transfer.
   * The handler should prepare and queue the necessary messages (request, chunks).
   *
   * @param recipient_id The ID of the client to send the file to.
   * @param file_path The path to the file to send.
   * @return True if the request was successfully initiated, false otherwise.
   */
  virtual bool RequestFileTransfer(int recipient_id, const std::string &file_path) = 0;

  /**
   * @brief Handles an incoming message related to file transfer.
   *
   * This method is called by the Client's receive logic when a file-transfer-related
   * message is received. The handler should process the message based on its type
   * (request, data chunk, complete, error).
   *
   * @param message The deserialized Message object.
   */
  virtual void HandleMessage(const Message &message) = 0;

  // Potentially add methods for getting transfer progress, cancelling, etc.
};

#endif // ICLIENT_FILE_TRANSFER_HANDLER_H_
