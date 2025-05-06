#ifndef MESSAGE_SERIALIZATION_H_
#define MESSAGE_SERIALIZATION_H_

#include "Message.h"

#include <string>
#include <vector>

/**
 * @brief Serializes a Message object into a byte vector.
 *
 * The serialized format is: MessageHeader (binary) followed by Payload (binary).
 *
 * @param message The Message object to serialize.
 * @return A vector of bytes representing the serialized message.
 */
std::vector<char> SerializeMessage(const Message &message);

/**
 * @brief Deserializes a byte vector into a Message object.
 *
 * Assumes the byte vector starts with a valid MessageHeader followed by the payload.
 *
 * @param data The byte vector to deserialize.
 * @return The deserialized Message object. Returns a Message with type UNKNOWN
 * if deserialization fails (e.g., data size is less than header size).
 */
Message DeserializeMessage(const std::vector<char> &data);

#endif // MESSAGE_SERIALIZATION_H_
