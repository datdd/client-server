#include "Client.h"

#include <iostream>
#include <limits>  // For numeric_limits
#include <sstream> // For stringstream
#include <string>

/**
 * @brief Main entry point for the client application.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line argument strings.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <server_ip> <server_port>" << std::endl;
    return 1;
  }

  std::string server_ip = argv[1];
  int server_port = std::stoi(argv[2]);

  if (server_port <= 0 || server_port > 65535) {
    std::cerr << "Invalid server port number: " << server_port << std::endl;
    return 1;
  }

  // Create the client instance
  Client client(server_ip, server_port);

  // Connect to the server
  if (!client.Connect()) {
    return 1;
  }

  // The send and receive threads are started in Client::Connect()

  std::cout << "Enter messages to send (type 'quit' to exit, '/sendfile <recipient_id> <filepath>' to send a file):"
            << std::endl;

  std::string line;
  // Read input from standard input
  while (std::getline(std::cin, line)) {
    if (line == "quit") {
      break;
    }

    // Check for file transfer command
    if (line.rfind("/sendfile ", 0) == 0) {  // Starts with "/sendfile "
      std::stringstream ss(line.substr(10)); // Extract the part after "/sendfile "
      int recipient_id;
      std::string file_path;

      ss >> recipient_id; // Read recipient ID
      // Read the rest of the line as the file path (can contain spaces)
      std::getline(ss >> std::ws, file_path);

      if (ss.fail() || file_path.empty()) {
        std::cerr << "Invalid /sendfile command format. Usage: /sendfile <recipient_id> <filepath>" << std::endl;
      } else {
        client.RequestFileTransfer(recipient_id, file_path);
      }
    } else {
      // Assume it's a chat message
      client.SendChatMessage(line);
    }
  }

  // Disconnect from the server and stop the threads
  client.Disconnect();

  return 0;
}
