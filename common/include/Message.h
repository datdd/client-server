#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <cstddef> // For size_t
#include <cstring> // For memcpy
#include <string>
#include <vector>

#include "MessageType.h"

/**
 * @brief Structure representing the header of a message.
 */
struct MessageHeader {
  MessageType type;    /**< The type of the message. */
  int sender_id;       /**< The ID of the client sending the message. */
  int recipient_id;    /**< The ID of the target client (or a special value for broadcast). */
  size_t payload_size; /**< The size of the message payload in bytes. */
};

// Define a fixed-size header for all messages
const size_t kMessageHeaderSize = sizeof(MessageHeader);

/**
 * @brief Structure representing a complete message (header + payload).
 */
struct Message {
  MessageHeader header;      /**< The message header. */
  std::vector<char> payload; /**< The message payload data. */

  /**
   * @brief Default constructor.
   */
  Message() : header({MessageType::UNKNOWN, -1, -1, 0}) {}

  /**
   * @brief Constructor with header and payload.
   * @param msg_header The message header.
   * @param msg_payload The message payload data.
   */
  Message(const MessageHeader &msg_header, const std::vector<char> &msg_payload)
      : header(msg_header), payload(msg_payload) {}
};

#endif // MESSAGE_H_
