#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace git_diff_pivot {

// Stable identifier for an interned, normalized diff line.
// Fixed width so it can later feed directly into the suffix-array layer.
using TokenId = std::uint32_t;

// Maps normalized diff lines to dense integer token ids and back.
// The future suffix-array construction operates on the resulting integer
// stream instead of comparing strings directly.
class TokenInterner {
public:
    TokenInterner() = default;

    // Returns the existing token id for `normalizedLine`, or creates a new one.
    [[nodiscard]] TokenId Intern(std::string_view normalizedLine);

    // Returns the normalized text for a previously interned token id.
    [[nodiscard]] std::string_view TextFor(TokenId token) const;

    [[nodiscard]] std::size_t Size() const noexcept { return tokenText_.size(); }

private:
    std::unordered_map<std::string, TokenId> tokenIdByText_;
    std::vector<std::string> tokenText_;
};

}  // namespace git_diff_pivot
