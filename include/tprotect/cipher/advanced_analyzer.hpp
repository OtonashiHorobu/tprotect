// advanced_analyzer.hpp: Advanced Cryptanalysis Tools

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace tprotect::cipher
{
struct bigram_frequency
{
    std::string bigram;
    int count;
    float percentage;

    bool operator>(const bigram_frequency &other) const
    {
        return count > other.count;
    }
};

struct trigram_frequency
{
    std::string trigram;
    int count;
    float percentage;

    bool operator>(const trigram_frequency &other) const
    {
        return count > other.count;
    }
};

struct advanced_statistics
{
    float index_of_coincidence; // IC value (0-1)
    float entropy;              // Shannon entropy (0-8)
    float chi_squared;          // Chi-squared statistic
    int total_letters;
    int unique_letters;
};

class advanced_analyzer
{
  public:
    /**
     * @brief Calculate Index of Coincidence (IC)
     * IC ≈ 0.065 for English, ≈ 0.038 for random text
     *
     * @param text The text to analyze
     * @return float The IC value
     */
    [[nodiscard]] static float calculate_ic(std::string_view text) noexcept
    {
        std::array<int, 26> counts{};
        int total_letters{};

        for (const char ch : text)
        {
            if (ch >= 'A' && ch <= 'Z')
            {
                counts[ch - 'A']++;
                total_letters++;
            }
            else if (ch >= 'a' && ch <= 'z')
            {
                counts[ch - 'a']++;
                total_letters++;
            }
        }

        if (total_letters < 2)
            return 0.0f;

        float sum{};
        for (const int count : counts)
        {
            sum += static_cast<float>(count) * (count - 1);
        }

        return sum / (static_cast<float>(total_letters) * (total_letters - 1));
    }

    /**
     * @brief Calculate Shannon Entropy
     * Higher entropy = more random, lower = more structured
     *
     * @param text The text to analyze
     * @return float The entropy value (0-8 for text)
     */
    [[nodiscard]] static float calculate_entropy(std::string_view text) noexcept
    {
        std::array<int, 26> counts{};
        int total_letters{};

        for (const char ch : text)
        {
            if (ch >= 'A' && ch <= 'Z')
            {
                counts[ch - 'A']++;
                total_letters++;
            }
            else if (ch >= 'a' && ch <= 'z')
            {
                counts[ch - 'a']++;
                total_letters++;
            }
        }

        if (total_letters == 0)
            return 0.0f;

        float entropy{};
        for (const int count : counts)
        {
            if (count > 0)
            {
                const float probability{static_cast<float>(count) / total_letters};
                entropy -= probability * std::log2(probability);
            }
        }

        return entropy;
    }

    /**
     * @brief Calculate Chi-Squared statistic against English frequencies
     * Lower χ² = closer to English, higher = more different
     *
     * @param text The text to analyze
     * @return float The chi-squared value
     */
    [[nodiscard]] static float calculate_chi_squared(std::string_view text) noexcept
    {
        constexpr std::array<float, 26> english_freq{
            8.17f,  // A
            1.49f,  // B
            2.78f,  // C
            4.25f,  // D
            12.70f, // E
            2.23f,  // F
            2.02f,  // G
            6.09f,  // H
            6.97f,  // I
            0.15f,  // J
            0.77f,  // K
            4.03f,  // L
            2.41f,  // M
            6.75f,  // N
            7.51f,  // O
            1.93f,  // P
            0.10f,  // Q
            5.99f,  // R
            6.33f,  // S
            9.06f,  // T
            2.76f,  // U
            0.98f,  // V
            2.36f,  // W
            0.15f,  // X
            1.97f,  // Y
            0.07f   // Z
        };

        std::array<int, 26> counts{};
        int total_letters{};

        for (const char ch : text)
        {
            if (ch >= 'A' && ch <= 'Z')
            {
                counts[ch - 'A']++;
                total_letters++;
            }
            else if (ch >= 'a' && ch <= 'z')
            {
                counts[ch - 'a']++;
                total_letters++;
            }
        }

        if (total_letters == 0)
            return 0.0f;

        float chi_squared{};
        for (size_t i{}; i < 26; ++i)
        {
            const float expected{english_freq[i] * total_letters / 100.0f};
            if (expected > 0)
            {
                const float observed{static_cast<float>(counts[i])};
                chi_squared += (observed - expected) * (observed - expected) / expected;
            }
        }

        return chi_squared;
    }

    /**
     * @brief Analyze bigram (2-letter) frequencies
     *
     * @param text The text to analyze
     * @return std::vector<bigram_frequency> Sorted by frequency (descending)
     */
    [[nodiscard]] static std::vector<bigram_frequency> analyze_bigrams(std::string_view text) noexcept
    {
        std::map<std::string, int> bigram_counts;
        int total_bigrams{};

        for (size_t i{}; i + 1 < text.size(); ++i)
        {
            char ch1{text[i]};
            char ch2{text[i + 1]};

            // Normalize to uppercase
            if (ch1 >= 'a' && ch1 <= 'z')
                ch1 -= 32;
            if (ch2 >= 'a' && ch2 <= 'z')
                ch2 -= 32;

            if ((ch1 >= 'A' && ch1 <= 'Z') && (ch2 >= 'A' && ch2 <= 'Z'))
            {
                bigram_counts[std::string{ch1, ch2}]++;
                total_bigrams++;
            }
        }

        std::vector<bigram_frequency> result;
        result.reserve(bigram_counts.size());

        for (const auto &[bigram, count] : bigram_counts)
        {
            const float percentage{total_bigrams > 0 ? (static_cast<float>(count) * 100.0f / total_bigrams) : 0.0f};
            result.push_back({bigram, count, percentage});
        }

        std::sort(result.begin(), result.end(), std::greater<>());
        return result;
    }

    /**
     * @brief Analyze trigram (3-letter) frequencies
     *
     * @param text The text to analyze
     * @return std::vector<trigram_frequency> Sorted by frequency (descending)
     */
    [[nodiscard]] static std::vector<trigram_frequency> analyze_trigrams(std::string_view text) noexcept
    {
        std::map<std::string, int> trigram_counts;
        int total_trigrams{};

        for (size_t i{}; i + 2 < text.size(); ++i)
        {
            char ch1{text[i]};
            char ch2{text[i + 1]};
            char ch3{text[i + 2]};

            // Normalize to uppercase
            if (ch1 >= 'a' && ch1 <= 'z')
                ch1 -= 32;
            if (ch2 >= 'a' && ch2 <= 'z')
                ch2 -= 32;
            if (ch3 >= 'a' && ch3 <= 'z')
                ch3 -= 32;

            if ((ch1 >= 'A' && ch1 <= 'Z') && (ch2 >= 'A' && ch2 <= 'Z') && (ch3 >= 'A' && ch3 <= 'Z'))
            {
                trigram_counts[std::string{ch1, ch2, ch3}]++;
                total_trigrams++;
            }
        }

        std::vector<trigram_frequency> result;
        result.reserve(trigram_counts.size());

        for (const auto &[trigram, count] : trigram_counts)
        {
            const float percentage{total_trigrams > 0 ? (static_cast<float>(count) * 100.0f / total_trigrams) : 0.0f};
            result.push_back({trigram, count, percentage});
        }

        std::sort(result.begin(), result.end(), std::greater<>());
        return result;
    }

    /**
     * @brief Get comprehensive advanced statistics
     *
     * @param text The text to analyze
     * @return advanced_statistics The statistics
     */
    [[nodiscard]] static advanced_statistics get_statistics(std::string_view text) noexcept
    {
        std::array<int, 26> counts{};
        int total_letters{};

        for (const char ch : text)
        {
            if (ch >= 'A' && ch <= 'Z')
            {
                counts[ch - 'A']++;
                total_letters++;
            }
            else if (ch >= 'a' && ch <= 'z')
            {
                counts[ch - 'a']++;
                total_letters++;
            }
        }

        int unique_letters{};
        for (const int count : counts)
        {
            if (count > 0)
                unique_letters++;
        }

        return {
            .index_of_coincidence = calculate_ic(text),
            .entropy = calculate_entropy(text),
            .chi_squared = calculate_chi_squared(text),
            .total_letters = total_letters,
            .unique_letters = unique_letters,
        };
    }
};
} // namespace tprotect::cipher
