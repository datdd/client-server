#ifndef CLIENT_HANDLER_H_
#define CLIENT_HANDLER_H_

#include "IClientHandler.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "IMessageHandler.h"
#include "ISocket.h"
#include "Message.h"
#include "MessageSerialization.h"
#include "Server.h"

/**
 * @brief Handles communication with a single client connection.
 *
 * This class runs in a separate thread and is responsible for receiving
 * messages from a client, reassembling them if necessary, and passing
 * complete messages to the message handler. It implements the IClientHandler
 * interface.
 */
class ClientHandler : public IClientHandler {
public:
  /**
   * @brief Constructs a new ClientHandler.
   *
   * @param client_id A unique ID for this client.
   * @param client_socket The socket connected to the client.
   * @param server A pointer to the Server instance.
   * @param message_handler The message handler to process received messages.
   */
  ClientHandler(int client_id, std::unique_ptr<ISocket> client_socket,
                Server *server, IMessageHandler *message_handler);

  /**
   * @brief Destroys the ClientHandler object. Stops the thread if running.
   */
  ~ClientHandler() override;

  /**
   * @brief Starts the client handler thread.
   */
  void Start() override;

  /**
   * @brief Stops the client handler and closes the connection.
   */
  void Stop() override;

  /**
   * @brief Sends a message to the connected client.
   *
   * @param message The message structure to send.
   * @return True if the message was sent successfully, false otherwise.
   */
  bool SendMessage(const Message &message);

  /**
   * @brief Gets the unique identifier for this client handler.
   *
   * @return The client ID.
   */
  int GetClientId() const override;

  /**
   * @brief Gets a pointer to the underlying socket.
   *
   * @return A pointer to the ISocket instance.
   */
  ISocket *GetSocket() const override;

private:
  /**
   * @brief The main loop for the client handler thread.
   *
   * This function continuously receives data from the client socket,
   * reassembles messages, and passes them to the message handler.
   */
  void Run();

  int client_id_;
  std::unique_ptr<ISocket> client_socket_;
  Server *server_;
  IMessageHandler *message_handler_;
  std::thread handler_thread_;
  std::atomic<bool> running_;

  std::vector<char> receive_buffer_;
};

#endif // CLIENT_HANDLER_H_
