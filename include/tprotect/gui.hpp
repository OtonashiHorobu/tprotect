// gui.hpp: Dear ImGui User Interface Manager

#pragma once

#include <atomic>
#include <mutex>
#include <string>

#include <tprotect/cipher/advanced_analyzer.hpp>
#include <tprotect/cipher/frequency_analyzer.hpp>
#include <tprotect/cipher/rail_fence_cipher.hpp>
#include <tprotect/cipher/substitution_cipher.hpp>
#include <tprotect/cipher/transposition_cipher.hpp>
#include <tprotect/cipher/vigenere_cipher.hpp>
#include <tprotect/global.hpp>
#include <tprotect/imgui_histogram.hpp>

struct SDL_Window;
struct SDL_Renderer;
struct ImFont;

namespace tprotect
{
/**
 * @brief The GUI Manager
 *
 * The singleton should be acquired by `gui::instance()`
 *
 */
class gui final
{
  public:
    /**
     * @brief Acquire the singleton
     *
     * @return gui& the singleton
     */
    static gui &instance() noexcept;

    /**
     * @brief Initialize the singleton
     *
     * @param[in] title is intentionally made `std::string`, which could prevent a potential copy
     *
     * @return result_type<void> the result
     */
    [[nodiscard]] static eresult<void> create(int width, int height, std::string title) noexcept;

    /**
     * @brief Destroy the singleton
     */
    static void destroy() noexcept;

    /**
     * @brief Get whether the singleton is initialized
     *
     * This function is thread-safe
     *
     */
    bool is_initialized() const noexcept;

    [[nodiscard]] eresult<void> main_loop() noexcept;

    ~gui();

    // Disable copying and moving
    gui(const gui &) noexcept = delete;
    gui &operator=(const gui &) noexcept = delete;
    gui(gui &&) noexcept = delete;
    gui &operator=(gui &&) noexcept = delete;

  private:
    gui() noexcept = default; // keep the constructor private

    [[nodiscard]] eresult<void> initialize(int width, int height,
                                           std::string title) noexcept; // the actual initializer
    void shutdown() noexcept;
    void render_window() noexcept;                       // render the gui
    void render_stats_window() noexcept;                 // render statistics window
    [[nodiscard]] eresult<void> process_file() noexcept; // display dialogs

    std::mutex main_loop_mutex_;
    std::string title_; // save it to ensure its validity
    SDL_Window *window_{};
    SDL_Renderer *renderer_{};

    // UI state
    ImFont *futura_medium_{};
    ImFont *jetbrains_mono_regular_{};
    enum class cipher_type
    {
        substitution,
        transposition,
        vigenere,
        rail_fence,
    };
    std::string encrypted_text_;
    std::string decrypted_text_;
    cipher_type selected_cipher_{cipher_type::substitution};
    tprotect::cipher::substitution_cipher substitution_cipher_{initial_substitution};
    tprotect::cipher::transposition_cipher transposition_cipher_{initial_key};
    tprotect::cipher::vigenere_cipher vigenere_cipher_{initial_vigenere};
    tprotect::cipher::rail_fence_cipher rail_fence_cipher_{initial_key};
    int transposition_key_{initial_key};
    std::string vigenere_key_{initial_vigenere};
    int rail_fence_rails_{initial_key};
    bool show_stats_window_{false};
    int stats_window_tab_{0};
    double fps_idle_{10.};
    bool is_idling_{};
    std::atomic<bool> is_initialized_; // `std::atomic<bool>` for thread safety
    bool should_exit_{};
};
} // namespace tprotect
