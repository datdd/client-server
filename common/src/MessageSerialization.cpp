#include "MessageSerialization.h"

#include <iostream>

/**
 * @brief Serializes a Message object into a byte vector.
 *
 * The serialized format is: MessageHeader (binary) followed by Payload
 * (binary).
 *
 * @param message The Message object to serialize.
 * @return A vector of bytes representing the serialized message.
 */
std::vector<char> SerializeMessage(const Message &message) {
  std::vector<char> data;
  data.resize(kMessageHeaderSize + message.payload.size());

  // Copy header data
  std::memcpy(data.data(), &message.header, kMessageHeaderSize);

  // Copy payload data
  if (!message.payload.empty()) {
    std::memcpy(data.data() + kMessageHeaderSize, message.payload.data(), message.payload.size());
  }

  return data;
}

/**
 * @brief Deserializes a byte vector into a Message object.
 *
 * Assumes the byte vector starts with a valid MessageHeader followed by the
 * payload.
 *
 * @param data The byte vector to deserialize.
 * @return The deserialized Message object. Returns a Message with type UNKNOWN
 * if deserialization fails (e.g., data size is less than header size).
 */
Message DeserializeMessage(const std::vector<char> &data) {
  Message message;

  if (data.size() < kMessageHeaderSize) {
    std::cerr << "Error deserializing message: Data size less than header size." << std::endl;
    return message;
  }

  // Copy header data
  std::memcpy(&message.header, data.data(), kMessageHeaderSize);

  // Check if the reported payload size matches the remaining data size
  if (data.size() < kMessageHeaderSize + message.header.payload_size) {
    std::cerr << "Error deserializing message: Reported payload size exceeds "
                 "remaining data."
              << std::endl;
    return Message();
  }

  // Copy payload data
  message.payload.resize(message.header.payload_size);
  if (!message.payload.empty()) {
    std::memcpy(message.payload.data(), data.data() + kMessageHeaderSize, message.header.payload_size);
  }

  return message;
}
