#include "core/sentinel_scanner.h"
#include <algorithm>

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)), pending_("") {}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    std::string combined = pending_ + std::string(chunk);

    // Check if the complete sentinel appears.
    std::size_t pos = combined.find(sentinel_);

    if (pos != std::string::npos) {
        std::string safe_text = combined.substr(0, pos);
        pending_.clear();

        return Out{safe_text, true};
    }

    // Find the longest suffix of combined that could still
    // become the beginning of the sentinel.
    std::size_t keep = 0;

    if (!sentinel_.empty()) {
        std::size_t max_check =
            std::min(combined.size(), sentinel_.size() - 1);

        for (std::size_t len = max_check; len > 0; --len) {
            if (combined.compare(
                    combined.size() - len,
                    len,
                    sentinel_,
                    0,
                    len) == 0) {

                keep = len;
                break;
            }
        }
    }

    // Everything before the possible sentinel prefix is safe to emit.
    std::size_t safe_length = combined.size() - keep;

    std::string safe_text = combined.substr(0, safe_length);

    // Save only the possible beginning of the sentinel.
    pending_ = combined.substr(safe_length);

    return Out{safe_text, false};
}

SentinelScanner::Out SentinelScanner::flush() {
    std::string safe_text = std::move(pending_);
    pending_.clear();

    return Out{safe_text, false};
}