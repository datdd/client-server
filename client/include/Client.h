#ifndef CLIENT_H_
#define CLIENT_H_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "IClientFileTransferHandler.h"
#include "ISocket.h"
#include "Message.h"
#include "MessageSerialization.h"

/**
 * @brief The client application class.
 *
 * This class manages the client's connection to the server, sending and
 * receiving messages, and dispatching message handling to specialized handlers.
 */
class Client {
public:
  /**
   * @brief Constructs a new Client object.
   * @param server_address The IP address or hostname of the server.
   * @param server_port The port number of the server.
   */
  Client(const std::string &server_address, int server_port);

  /**
   * @brief Destroys the Client object. Disconnects from the server.
   */
  ~Client();

  /**
   * @brief Connects the client to the server.
   * @return True if connection is successful, false otherwise.
   */
  bool Connect();

  /**
   * @brief Disconnects the client from the server.
   */
  void Disconnect();

  /**
   * @brief Sends a chat message to the server.
   * @param message The message string to send.
   * @return True if the message was sent successfully, false otherwise.
   */
  bool SendChatMessage(const std::string &message);

  /**
   * @brief Initiates a file transfer request to a recipient.
   *
   * Delegates the request to the file transfer handler.
   *
   * @param recipient_id The ID of the client to send the file to.
   * @param file_path The path to the file to send.
   * @return True if the request was successfully initiated, false otherwise.
   */
  bool RequestFileTransfer(int recipient_id, const std::string &file_path);

  /**
   * @brief Starts the thread for sending messages to the server.
   */
  void StartSendThread();

  /**
   * @brief Stops the send thread.
   */
  void StopSendThread();

  /**
   * @brief Starts the thread for receiving messages from the server.
   */
  void StartReceiveThread();

  /**
   * @brief Stops the receive thread.
   */
  void StopReceiveThread();

  /**
   * @brief Gets the client's assigned ID from the server.
   * @return The client ID, or -1 if not yet assigned.
   */
  int GetClientId() const;

private:
  /**
   * @brief The main loop for the send thread.
   *
   * This function sends messages from a queue and handles sending file chunks.
   */
  void SendMessages();

  /**
   * @brief The main loop for the receive thread.
   *
   * This function continuously receives data from the server socket,
   * reassembles messages, and processes them.
   */
  void ReceiveMessages();

  /**
   * @brief Processes a received message based on its type.
   *
   * Delegates handling of specific message types to appropriate handlers.
   *
   * @param message The deserialized Message object.
   */
  void ProcessReceivedMessage(const Message &message);

  /**
   * @brief Sends a Message object to the server.
   * @param message The Message object to send.
   * @return True if the message was sent successfully, false otherwise.
   */
  bool SendMessage(const Message &message);

  std::string server_address_;             /**< Server IP address or hostname. */
  int server_port_;                        /**< Server port number. */
  std::unique_ptr<ISocket> server_socket_; /**< Socket connected to the server. */

  std::thread send_thread_;               /**< Thread for sending messages. */
  std::atomic<bool> sending_;             /**< Flag to control the send thread loop. */
  std::queue<Message> send_queue_;        /**< Queue for outgoing messages. */
  std::mutex send_queue_mutex_;           /**< Mutex to protect the send queue. */
  std::condition_variable send_queue_cv_; /**< Condition variable to signal the send thread. */

  std::thread receive_thread_;       /**< Thread for receiving messages. */
  std::atomic<bool> receiving_;      /**< Flag to control the receive thread loop. */
  std::vector<char> receive_buffer_; /**< Buffer to accumulate incoming data. */

  std::atomic<int> client_id_; /**< The client's assigned ID from the server. */

  // File transfer handler
  std::unique_ptr<IClientFileTransferHandler> file_transfer_handler_;
};

#endif // CLIENT_H_
