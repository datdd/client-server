#include "BroadcastMessageHandler.h"
#include "CompositeMessageHandler.h" 
#include "FileTransferHandler.h"     
#include "Server.h"

#ifdef _WIN32
#include "WinsockSocket.h"
#else
#include "PosixSocket.h"
#endif

#include <iostream>
#include <memory> 
#include <string> 

/**
 * @brief Main entry point for the server application.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line argument strings.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
    return 1;
  }

  int port = std::stoi(argv[1]);
  if (port <= 0 || port > 65535) {
    std::cerr << "Invalid port number: " << port << std::endl;
    return 1;
  }

  // --- Dependency Creation (Composition Root) ---

  // Create the concrete ISocket implementation based on the platform
  std::unique_ptr<ISocket> server_socket;
#ifdef _WIN32
  server_socket = std::make_unique<WinsockSocket>();
#else
  server_socket = std::make_unique<PosixSocket>();
#endif

  if (!server_socket || !server_socket->IsValid()) {
    std::cerr << "Failed to create server socket." << std::endl;
    return 1;
  }

  // Create the composite message handler
  auto composite_message_handler = std::make_unique<CompositeMessageHandler>();

  // Add specific message handlers to the composite
  composite_message_handler->AddHandler(std::make_unique<BroadcastMessageHandler>());
  composite_message_handler->AddHandler(std::make_unique<FileTransferHandler>());
  // Add other message handlers here as needed

  // --- Dependency Injection ---
  // Create the server instance, injecting the dependencies
  Server server_instance(port, std::move(server_socket), std::move(composite_message_handler));
  // ---------------------------------------------

  // Start the server and accept connections
  if (!server_instance.Start()) {
    std::cerr << "Failed to start server." << std::endl;
    return 1;
  }

  // Keep the server running until interrupted (e.g., by Ctrl+C)
  // Signal handling would be needed for a graceful shutdown in a real app.
  server_instance.AcceptConnections();

  // Server stopped (e.g., by signal or error)
  server_instance.Stop();

  return 0;
}
