#ifndef MESSAGE_TYPE_H_
#define MESSAGE_TYPE_H_

/**
 * @brief Enum defining the different types of messages exchanged between client and server.
 */
enum class MessageType : int {
  UNKNOWN = 0,
  CLIENT_ID_ASSIGNMENT,
  BROADCAST_MESSAGE,
  PRIVATE_MESSAGE,
  FILE_TRANSFER_REQUEST,
  FILE_DATA_CHUNK,
  FILE_TRANSFER_COMPLETE,
  FILE_TRANSFER_ERROR,
  // Add other message types as needed
};

#endif // MESSAGE_TYPE_H_
