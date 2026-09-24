#ifndef CORE_CONVERSATION_H
#define CORE_CONVERSATION_H

#include "core/message.h"
#include <cstddef>
#include <stdexcept>
#include <utility>

class Conversation {
public:
    Conversation();
    ~Conversation();

    // Copy
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    // Move
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    void append(Message m);

    std::size_t size() const noexcept;
    const Message& at(std::size_t i) const;

const Message* begin() const noexcept;
const Message* end() const noexcept;

private:
    Message* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};

#endif