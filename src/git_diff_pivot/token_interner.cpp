#include "git_diff_pivot/token_interner.hpp"

#include <stdexcept>

#include "git_diff_pivot/line_normalization.hpp"

namespace git_diff_pivot {

TokenId TokenInterner::Intern(std::string_view content) {
    const auto normalizedLine = NormalizeDiffLine(content);
    if (const auto it = tokenIdByText_.find(std::string(normalizedLine)); it != tokenIdByText_.end()) {
        return it->second;
    }

    const auto newId = static_cast<TokenId>(tokenText_.size());
    tokenText_.emplace_back(content);
    tokenIdByText_.emplace(normalizedLine, newId);
    return newId;
}

std::string_view TokenInterner::TextFor(TokenId token) const {
    if (token >= tokenText_.size()) {
        throw std::out_of_range("TokenInterner::TextFor: unknown token id");
    }
    return tokenText_[token];
}

}  // namespace git_diff_pivot
