// rail_fence_cipher.hpp: The Rail Fence Cipher Implementation

#pragma once

#include <cctype>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace tprotect::cipher
{
class rail_fence_cipher
{
  public:
    explicit rail_fence_cipher(const int rails) noexcept
    {
        set_rails(rails);
    }

    [[nodiscard]] std::expected<std::string, std::string> encrypt(const std::string_view input) const noexcept
    {
        if (rails_ < 2)
        {
            return std::unexpected{"Number of rails must be at least 2"};
        }

        if (input.empty())
        {
            return std::string{};
        }

        std::vector<std::string> fence(rails_);
        int rail{};
        int direction{1};

        for (const char ch : input)
        {
            fence[rail] += ch;

            if (rail == 0)
            {
                direction = 1;
            }
            else if (rail == rails_ - 1)
            {
                direction = -1;
            }

            rail += direction;
        }

        std::string result;
        for (const auto &rail_str : fence)
        {
            result += rail_str;
        }

        return result;
    }

    [[nodiscard]] std::expected<std::string, std::string> decrypt(const std::string_view input) const noexcept
    {
        if (rails_ < 2)
        {
            return std::unexpected{"Number of rails must be at least 2"};
        }

        if (input.empty())
        {
            return std::string{};
        }

        // Calculate the length of each rail
        std::vector<int> rail_lengths(rails_, 0);
        int rail{};
        int direction{1};

        for (size_t i{}; i < input.size(); ++i)
        {
            rail_lengths[rail]++;

            if (rail == 0)
            {
                direction = 1;
            }
            else if (rail == rails_ - 1)
            {
                direction = -1;
            }

            rail += direction;
        }

        // Split the input into rails
        std::vector<std::string> fence(rails_);
        size_t pos{};
        for (int i{}; i < rails_; ++i)
        {
            fence[i] = std::string{input.substr(pos, rail_lengths[i])};
            pos += rail_lengths[i];
        }

        // Reconstruct the original message
        std::string result;
        result.reserve(input.size());
        std::vector<size_t> rail_indices(rails_, 0);
        rail = 0;
        direction = 1;

        for (size_t i{}; i < input.size(); ++i)
        {
            result += fence[rail][rail_indices[rail]++];

            if (rail == 0)
            {
                direction = 1;
            }
            else if (rail == rails_ - 1)
            {
                direction = -1;
            }

            rail += direction;
        }

        return result;
    }

    void set_rails(const int rails) noexcept
    {
        rails_ = rails < 2 ? 2 : rails;
    }

    [[nodiscard]] int get_rails() const noexcept
    {
        return rails_;
    }

  private:
    int rails_{2};
};
} // namespace tprotect::cipher
