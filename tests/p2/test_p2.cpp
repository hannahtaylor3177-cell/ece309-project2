// tests/p2/test_p2.cpp
//
// YOUR test suite goes here. At least 12 assert-based test cases — see
// spec §5 for the required categories and the sample test for the
// expected level of rigor.
//
// This file is a stub so the project builds out of the box; replace the
// body of main() with your own tests.

#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define TEST(name) void name(); int main_##name = (tests.push_back({#name, name}), 0); void name()

struct TestCase {
    std::string name;
    void (*fn)();
};
static std::vector<TestCase> tests;

class StringInputSource : public InputSource {
public:
    explicit StringInputSource(std::vector<std::string> inputs)
        : inputs_(std::move(inputs)) {}

    std::string read_line() override {
        if (index_ < inputs_.size()) {
            return inputs_[index_++];
        }
        eof_ = true;
        return "";
    }

    bool is_eof() const override { return eof_; }

private:
    std::vector<std::string> inputs_;
    std::size_t index_ = 0;
    bool eof_ = false;
};

class StringOutputSink : public OutputSink {
public:
    void write(std::string_view text) override {
        output_ << text;
    }

    std::string str() const { return output_.str(); }

private:
    std::ostringstream output_;
};

// 1. Empty Conversation Bounds
TEST(EmptyConversationBounds) {
    Conversation conv;
    assert(conv.size() == 0);
    assert(conv.begin() == conv.end());

    bool exception_thrown = false;
    try {
        [[maybe_unused]] const Message& msg = conv.at(0);
    } catch (const std::out_of_range&) {
        exception_thrown = true;
    }
    assert(exception_thrown && "conv.at(0) on empty conversation must throw std::out_of_range");
}

// 2. System Message Ordering
TEST(SystemMessageOrdering) {
    Conversation conv;
    conv.append(Message(Role::System, "You are a helpful assistant."));
    conv.append(Message(Role::User, "Hello"));
    conv.append(Message(Role::Assistant, "Hi there!"));

    assert(conv.size() == 3);
    assert(conv.at(0).role() == Role::System);
    assert(conv.at(0).content() == "You are a helpful assistant.");
    assert(conv.at(1).role() == Role::User);
    assert(conv.at(2).role() == Role::Assistant);
}

// 3. Rule of Five (Copy)
TEST(RuleOfFiveCopy) {
    Conversation conv1;
    conv1.append(Message(Role::User, "Original Message"));

    Conversation conv2 = conv1; // Copy constructor
    assert(conv1.size() == conv2.size());
    assert(conv1.begin() != conv2.begin() && "Copy must allocate distinct heap buffer");
    assert(conv2.at(0).content() == "Original Message");

    Conversation conv3;
    conv3 = conv1; // Copy assignment
    assert(conv3.begin() != conv1.begin() && "Assignment must allocate distinct heap buffer");
    assert(conv3.at(0).content() == "Original Message");
}

// 4. Rule of Five (Move)
TEST(RuleOfFiveMove) {
    Conversation conv1;
    conv1.append(Message(Role::User, "Move Message"));
    const Message* original_ptr = conv1.begin();

    Conversation conv2 = std::move(conv1); // Move constructor
    assert(conv2.begin() == original_ptr && "Move must steal buffer address");
    assert(conv2.size() == 1);
    assert(conv1.size() == 0 && "Source must be reset to size 0");
    assert(conv1.begin() == nullptr && "Source data pointer must be zeroed");

    Conversation conv3;
    conv3 = std::move(conv2); // Move assignment
    assert(conv3.begin() == original_ptr && "Move assignment must steal buffer");
    assert(conv2.size() == 0);
    assert(conv2.begin() == nullptr);
}

// 5. Growth Behavior
TEST(GrowthBehavior) {
    Conversation conv;
    constexpr std::size_t N = 100;

    for (std::size_t i = 0; i < N; ++i) {
        conv.append(Message(Role::User, "Msg " + std::to_string(i)));
        assert(conv.size() == i + 1);
    }

    for (std::size_t i = 0; i < N; ++i) {
        assert(conv.at(i).content() == "Msg " + std::to_string(i));
    }
}

// 6. Scanner (Clean Text)
TEST(ScannerCleanText) {
    SentinelScanner scanner("<|end_conversation|>");
    auto out = scanner.feed("Hello world! How are you?");
    
    assert(!out.sentinel_found);
    
    auto flushed = scanner.flush();
    assert(!flushed.sentinel_found);
    assert(out.safe_text + flushed.safe_text == "Hello world! How are you?");
}

// 7. Scanner (Split Sentinel Across All Split Points)
TEST(ScannerCatchesSentinelAtEveryBoundary) {
    const std::string sentinel = "<|end_conversation|>";
    const std::string text = "Goodbye." + sentinel;

    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner scanner(sentinel);
        auto out1 = scanner.feed(text.substr(0, split));
        auto out2 = scanner.feed(text.substr(split));
        auto flushed = scanner.flush();

        bool found = out1.sentinel_found || out2.sentinel_found || flushed.sentinel_found;
        assert(found && "sentinel must be caught regardless of split point");
        
        std::string full_safe = out1.safe_text + out2.safe_text + flushed.safe_text;
        assert(full_safe == "Goodbye.");
    }
}

// 8. Scanner (False Alarms)
TEST(ScannerFalseAlarms) {
    SentinelScanner scanner("<|end_conversation|>");
    auto out1 = scanner.feed("Testing <|end_world|> partial match");
    auto out2 = scanner.flush();

    assert(!out1.sentinel_found);
    assert(!out2.sentinel_found);
    assert(out1.safe_text + out2.safe_text == "Testing <|end_world|> partial match");
}

// 9. Scanner (Bounded Memory Under 4MB Adversarial Stream)
TEST(ScannerBoundedMemory) {
    const std::string sentinel = "<|end_conversation|>";
    SentinelScanner scanner(sentinel);

    // Feed a 4MB stream one byte at a time repeating "<|end_" without completing sentinel
    const std::string pattern = "<|end_";
    std::string safe_total;

    for (std::size_t i = 0; i < 4 * 1024 * 1024; ++i) {
        char ch = pattern[i % pattern.size()];
        auto out = scanner.feed(std::string_view(&ch, 1));
        assert(!out.sentinel_found);
        safe_total += out.safe_text;
    }

    auto flushed = scanner.flush();
    safe_total += flushed.safe_text;

    assert(safe_total.size() == 4 * 1024 * 1024);
    assert(safe_total.find(sentinel) == std::string::npos);
}

// 10. Harness Integration (Turn Limit)
TEST(HarnessTurnLimit) {
    // Create temporary script file for ScriptedModelClient
    const std::string script_path = "test_turn_limit.script";
    std::ofstream script_file(script_path);
    for (int i = 0; i < 5; ++i) {
        script_file << "role: assistant\nTurn " << i << "\n---\n";
    }
    script_file.close();

    auto model = std::make_unique<ScriptedModelClient>(script_path);
    HarnessConfig cfg;
    cfg.max_turns = 2; // Limit to 2 turns

    Harness harness(std::move(model), cfg);
    StringInputSource input({"hello", "hello again", "hello third"});
    StringOutputSink output;

    StopReason result = harness.run(input, output);
    assert(result.kind == StopReason::Kind::TurnLimit);

    std::remove(script_path.c_str());
}

// 11. Harness Integration (Sentinel Halt)
TEST(HarnessSentinelHalt) {
    const std::string script_path = "test_sentinel_halt.script";
    std::ofstream script_file(script_path);
    script_file << "role: assistant\nGoodbye.<|end_conversation|>\n---\n";
    script_file.close();

    auto model = std::make_unique<ScriptedModelClient>(script_path);
    HarnessConfig cfg;
    cfg.max_turns = 10;

    Harness harness(std::move(model), cfg);
    StringInputSource input({"bye"});
    StringOutputSink output;

    StopReason result = harness.run(input, output);
    assert(result.kind == StopReason::Kind::Sentinel);

    std::remove(script_path.c_str());
}

// 12. Transcript Round-Trip
TEST(TranscriptRoundTrip) {
    const std::string transcript_path = "test_transcript.txt";
    std::ofstream script_file(transcript_path);
    script_file << "role: system\nBe concise.\n---\n"
                << "role: user\nhello\n---\n"
                << "role: assistant\nHi there!\n---\n";
    script_file.close();

    ReplayModelClient replay(transcript_path);
    Conversation conv;
    conv.append(Message(Role::System, "Be concise."));
    conv.append(Message(Role::User, "hello"));

    Message reply = replay.generate(conv);
    assert(reply.role() == Role::Assistant);
    assert(reply.content() == "Hi there!");

    std::remove(transcript_path.c_str());
}

// Test Runner
int main() {
    std::cout << "Running Project 2 Test Suite...\n";

    std::size_t passed = 0;

    for (const auto& test : tests) {
        std::cout << "  [RUN] " << test.name << " ... ";
        test.fn();
        std::cout << "PASSED\n";
        passed++;
    }

    std::cout << "\nAll " << passed
              << " tests passed successfully!\n";

    return 0;
}