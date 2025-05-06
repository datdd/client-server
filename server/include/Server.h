#ifndef SERVER_H_
#define SERVER_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "IClientHandler.h"
#include "IMessageHandler.h"
#include "ISocket.h"
#include "Message.h" // Include Message

/**
 * @brief The main server class.
 *
 * This class is responsible for initializing the server socket, listening
 * for incoming connections, accepting clients, and managing client handlers.
 * Adheres to the Single Responsibility Principle for managing the server
 * lifecycle and client connections. Dependencies (like ISocket and
 * IMessageHandler) are injected via the constructor.
 */
class Server {
public:
  /**
   * @brief Constructs a new Server object.
   * 
   * @param port The port number the server will listen on.
   * @param server_socket The socket to use for listening (dependency injected).
   * @param message_handler The message handler to use for processing client
   * messages (dependency injected).
   */
  Server(int port, std::unique_ptr<ISocket> server_socket,
         std::unique_ptr<IMessageHandler> message_handler);

  /**
   * @brief Destroys the Server object. Stops the server and cleans up clients.
   */
  ~Server();

  /**
   * @brief Starts the server, binds to the port, and begins listening.
   * @return True if the server starts successfully, false otherwise.
   */
  bool Start();

  /**
   * @brief Stops the server and disconnects all clients.
   */
  void Stop();

  /**
   * @brief Main loop for accepting incoming client connections.
   *
   * This method runs indefinitely until the server is stopped.
   */
  void AcceptConnections();

  /**
   * @brief Broadcasts a message to all connected clients except the sender.
   *
   * @param message The message to broadcast.
   * @param sender The client handler that sent the message (can be nullptr).
   */
  void BroadcastMessage(const Message &message, IClientHandler *sender);

  /**
   * @brief Removes a client handler from the server's list.
   *
   * @param client_handler The client handler to remove.
   */
  void RemoveClient(IClientHandler *client_handler);

  /**
   * @brief Gets a client handler by its ID.
   *
   * @param client_id The ID of the client handler to retrieve.
   * @return A pointer to the ClientHandler if found, nullptr otherwise.
   */
  IClientHandler *GetClientHandler(int client_id);

private:
  int port_;
  std::unique_ptr<ISocket> server_socket_;
  std::vector<std::unique_ptr<IClientHandler>> clients_;
  std::mutex clients_mutex_;
  std::unique_ptr<IMessageHandler> message_handler_;
  std::atomic<bool> running_;
  int next_client_id_;
};

#endif // SERVER_H_
