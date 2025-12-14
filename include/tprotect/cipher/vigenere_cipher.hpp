// vigenere_cipher.hpp: The Vigenère Cipher Implementation

#pragma once

#include <cctype>
#include <expected>
#include <string>
#include <string_view>

namespace tprotect::cipher
{
class vigenere_cipher
{
  public:
    explicit vigenere_cipher(const std::string_view key) noexcept
    {
        set_key(key);
    }

    [[nodiscard]] std::expected<std::string, std::string> encrypt(const std::string_view input) const noexcept
    {
        if (key_.empty())
        {
            return std::unexpected{"Key is empty"};
        }

        std::string result;
        result.reserve(input.size());
        size_t key_index{};

        for (const char ch : input)
        {
            if (std::isalpha(static_cast<unsigned char>(ch)))
            {
                const char base{std::isupper(static_cast<unsigned char>(ch)) ? 'A' : 'a'};
                const char key_char{static_cast<char>(std::toupper(key_[key_index % key_.size()]))};
                const int shift{key_char - 'A'};
                result.push_back(static_cast<char>(static_cast<unsigned char>((ch - base + shift) % 26 + base)));
                key_index++;
            }
            else
            {
                result.push_back(ch);
            }
        }

        return result;
    }

    [[nodiscard]] std::expected<std::string, std::string> decrypt(const std::string_view input) const noexcept
    {
        if (key_.empty())
        {
            return std::unexpected{"Key is empty"};
        }

        std::string result;
        result.reserve(input.size());
        size_t key_index{};

        for (const char ch : input)
        {
            if (std::isalpha(static_cast<unsigned char>(ch)))
            {
                const char base{std::isupper(static_cast<unsigned char>(ch)) ? 'A' : 'a'};
                const char key_char{static_cast<char>(std::toupper(key_[key_index % key_.size()]))};
                const int shift{key_char - 'A'};
                int decrypted{(ch - base - shift) % 26};
                if (decrypted < 0)
                {
                    decrypted += 26;
                }
                result.push_back(static_cast<char>(static_cast<unsigned char>(decrypted + base)));
                key_index++;
            }
            else
            {
                result.push_back(ch);
            }
        }

        return result;
    }

    void set_key(const std::string_view key) noexcept
    {
        key_ = std::string{key};
    }

    [[nodiscard]] const std::string &get_key() const noexcept
    {
        return key_;
    }

  private:
    std::string key_;
};
} // namespace tprotect::cipher
