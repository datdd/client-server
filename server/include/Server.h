#ifndef SERVER_H_
#define SERVER_H_

#include <atomic>
#include <condition_variable> // For std::condition_variable
#include <memory>             // For std::unique_ptr
#include <mutex>              // For std::mutex, std::unique_lock, std::lock_guard
#include <queue>              // For std::queue
#include <thread>
#include <vector>

#include "IClientHandler.h" // Include the interface for client handlers
#include "IMessageHandler.h"
#include "ISocket.h"
#include "Message.h" // Include Message

/**
 * @brief The server application class.
 *
 * This class manages the server's lifecycle, accepts incoming client connections,
 * manages connected clients, and dispatches incoming messages to handlers.
 * It uses a dedicated thread for cleaning up disconnected clients.
 */
class Server {
public:
  /**
   * @brief Constructs a new Server object.
   * @param port The port number the server will listen on.
   * @param server_socket The socket to use for listening (dependency injected).
   * @param message_handler The message handler to use for processing client messages (dependency injected).
   */
  Server(int port, std::unique_ptr<ISocket> server_socket, std::unique_ptr<IMessageHandler> message_handler);

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
   * @param message The message to broadcast.
   * @param sender The client handler that sent the message (can be nullptr).
   */
  void BroadcastMessage(const Message &message, IClientHandler *sender);

  /**
   * @brief Gets a client handler by its ID.
   * @param client_id The ID of the client handler to retrieve.
   * @return A pointer to the ClientHandler if found, nullptr otherwise.
   */
  IClientHandler *GetClientHandler(int client_id);

  /**
   * @brief Signals that a client handler has finished and is ready for cleanup.
   * @param client_id The ID of the client that finished.
   */
  void SignalClientFinished(int client_id);

private:
  /**
   * @brief The main loop for the cleanup thread.
   *
   * This function waits for signals from finished client handlers
   * and cleans up their resources.
   */
  void CleanupClients();

  /**
   * @brief Removes a client handler from the server's list.
   *
   * This method is called by the cleanup thread to remove a finished client.
   * Assumes clients_mutex_ is already locked.
   *
   * @param client_handler_id The ID of the client handler to remove.
   */
  void RemoveClient(int client_handler_id);

  int port_;                                         /**< The port the server listens on. */
  std::unique_ptr<ISocket> server_socket_;           /**< The server's listening socket. */
  std::unique_ptr<IMessageHandler> message_handler_; /**< Handler for processing messages. */

  std::atomic<bool> running_;       /**< Flag to control server loops. */
  std::atomic<int> next_client_id_; /**< Counter for assigning client IDs. */

  std::vector<std::unique_ptr<IClientHandler>> clients_; /**< List of connected client handlers. */
  std::mutex clients_mutex_;                             /**< Mutex to protect the clients_ vector. */

  std::thread accept_thread_;         /**< Thread for accepting connections. */
  std::thread cleanup_thread_;        /**< Thread for cleaning up finished clients. */
  std::atomic<bool> cleanup_running_; /**< Flag to control the cleanup thread loop. */

  // Members for queue-based cleanup
  std::queue<int> finished_client_ids_;         /**< Queue of client IDs ready for cleanup. */
  std::mutex finished_clients_mutex_;           /**< Mutex to protect the finished_client_ids_ queue. */
  std::condition_variable finished_clients_cv_; /**< Condition variable to signal the cleanup thread. */
};

#endif // SERVER_H_
