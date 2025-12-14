// gui.cpp: Dear ImGui User Interface Manager

#include <fonts.hpp>
#include <tprotect/file_dialog.hpp>
#include <tprotect/gui.hpp>

#include <filesystem>
#include <ranges>

#include <imgui_additions.hpp>

#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

using namespace std::literals;

namespace tprotect
{
gui::~gui()
{
    if (is_initialized())
    {
        shutdown();
    }
}

gui &gui::instance() noexcept
{
    static gui instance{};
    return instance;
}

[[nodiscard]] eresult<void> gui::create(const int width, const int height, std::string title) noexcept
{
    return instance().initialize(width, height, std::move(title));
}

void gui::destroy() noexcept
{
    return instance().shutdown();
}

bool gui::is_initialized() const noexcept
{
    return is_initialized_.load(std::memory_order_acquire);
}

[[nodiscard]] eresult<void> gui::initialize(const int width, const int height, std::string title) noexcept
{
    if (bool expected{};
        !is_initialized_.compare_exchange_strong(expected, true, std::memory_order_release, std::memory_order_relaxed))
    {
        return std::unexpected{"GUI has already been initialized"};
    }

    title_ = std::move(title);

    // Initialize GLFW
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        return std::unexpected{std::format("Failed to initialize SDL: {}", SDL_GetError())};
    }

    // Create window
    const float main_scale{SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay())};
    window_ =
        SDL_CreateWindow(title_.c_str(), static_cast<int>(width * main_scale), static_cast<int>(height * main_scale),
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window_ == nullptr)
    {
        return std::unexpected{std::format("Failed to create SDL window: {}", SDL_GetError())};
    }

    // Create renderer
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (renderer_ == nullptr)
    {
        return std::unexpected{std::format("Failed to create SDL renderer: {}", SDL_GetError())};
    }
    SDL_SetRenderVSync(renderer_, 1);

    // Show window
    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window_);

    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io{ImGui::GetIO()};
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;

    // Setup style
    ImGui::StyleColorsComfortableDark();

    // Setup scaling
    auto &style{ImGui::GetStyle()};
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    // Setup backends
    ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
    ImGui_ImplSDLRenderer3_Init(renderer_);

    // Setup fonts
    ImFontConfig font_cfg{};
    font_cfg.MergeMode = true;
    io.Fonts->AddFontFromMemoryCompressedTTF(futura_medium_compressed_data, sizeof futura_medium_compressed_data);
    futura_medium_ = io.Fonts->AddFontFromMemoryCompressedTTF(
        noto_sans_cjk_regular_compressed_data, sizeof noto_sans_cjk_regular_compressed_data, 0.f, &font_cfg);
    io.Fonts->AddFontFromMemoryCompressedTTF(jetbrains_mono_regular_compressed_data,
                                             sizeof jetbrains_mono_regular_compressed_data);
    jetbrains_mono_regular_ = io.Fonts->AddFontFromMemoryCompressedTTF(
        noto_sans_cjk_regular_compressed_data, sizeof noto_sans_cjk_regular_compressed_data, 0.f, &font_cfg);

    return {};
}

void gui::shutdown() noexcept
{
    if (bool expected{true};
        !is_initialized_.compare_exchange_strong(expected, false, std::memory_order_acq_rel, std::memory_order_relaxed))
    {
        return;
    }

    std::lock_guard<std::mutex> main_loop_guard_{main_loop_mutex_}; // prevent shutdown while the main loop is running

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (renderer_)
    {
        SDL_DestroyRenderer(renderer_);
        window_ = nullptr;
    }

    if (window_)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}

[[nodiscard]] eresult<void> gui::main_loop() noexcept
{
    if (!is_initialized() || !window_)
    {
        return std::unexpected{"GUI has not been initialized"};
    }
    std::lock_guard<std::mutex> main_loop_guard_{main_loop_mutex_}; // prevent shutdown while the main loop is running
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_BEGIN
#endif
    while (!should_exit_)
    {
#ifdef __EMSCRIPTEN__
        if (ShallIdleThisFrame_Emscripten(is_idling_))
        {
            continue;
        }
#else
        // Render idling
        is_idling_ = false;
        if (fps_idle_ > 0.)
        {
            const auto before_wait{std::chrono::steady_clock::now()};
            const double wait_expected{1. / fps_idle_};
            SDL_WaitEventTimeout(nullptr, wait_expected * 1000);
            const auto after_wait{std::chrono::steady_clock::now()};
            const double wait_duration{duration_cast<std::chrono::duration<double>>(after_wait - before_wait).count()};
            is_idling_ = wait_duration > wait_expected * 0.5;
        }
#endif

        // Handle events
        SDL_Event event{};
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
            {
                should_exit_ = true;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window_))
            {
                should_exit_ = true;
            }
        }

        // Halt minimized render
        if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // Start a new frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Render the user draw list
        const auto viewport{ImGui::GetMainViewport()};
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        if (ImGui::Begin("TProtect", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus))
        {
            render_window();

            // Display and process dialogs
            std::string message{};
            if (auto result{process_file()}; !result)
            {
                ImGui::OpenPopup("Error Processing File");
                message = std::move(result.error());
            }

            ImGui::InformationPopup("Error Processing File", message.c_str(), [] {});
        }
        ImGui::End();

        // Render statistics window
        if (show_stats_window_)
        {
            render_stats_window();
        }

        // Render the frame
        ImGui::Render();
        auto &io{ImGui::GetIO()};
        SDL_SetRenderScale(renderer_, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColorFloat(renderer_, 0.f, 0.f, 0.f, 0.f);
        SDL_RenderClear(renderer_);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
        SDL_RenderPresent(renderer_);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
    return {};
}

void gui::render_window() noexcept
{
    // Top title with larger font
    ImGui::PushFont(futura_medium_, ImGui::GetFontSize() * 2.f);
    ImGui::TextCentered("TProtect");
    ImGui::PopFont();
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("The Text Protector");
    }

    ImGui::Separator();

    std::string cipher_message{};

    if (ImGui::BeginTable("MainTable", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody))
    {
        // Setup column widths (2:1:2 ratio)
        ImGui::TableSetupColumn("Encrypted", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Buttons", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Decrypted", ImGuiTableColumnFlags_WidthStretch, 2.0f);

        // Row 1: Titles
        ImGui::TableNextRow();

        // Cell (1,1): Decrypted title
        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginTable("DecryptedHeader", 3,
                              ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoBordersInBody))
        {
            ImGui::TableSetupColumn("Text", ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableSetupColumn("Spacer", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Buttons", ImGuiTableColumnFlags_WidthFixed, 0.0f);

            ImGui::TableNextRow();

            // Cell 1: Text (Left Aligned)
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding(); // vertical aligned
            ImGui::Text("Decrypted");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("The decrypted text");
            }

            // Cell 2: Dummy
            ImGui::TableSetColumnIndex(1);

            // Cell 3: Buttons (Right Aligned)
            ImGui::TableSetColumnIndex(2);
            if (ImGui::ButtonPadded("Clear"))
            {
                decrypted_text_.clear();
            }
            ImGui::SameLine();
            if (ImGui::ButtonPadded("Load"))
            {
                ImGuiFileDialog::Instance()->OpenDialog("##LoadDecrypted", "Choose Decrypted Text To Load", ".txt",
                                                        {.path = "."});
            }
            ImGui::SameLine();
            if (ImGui::ButtonPadded("Save"))
            {
                ImGuiFileDialog::Instance()->OpenDialog("##SaveDecrypted", "Choose Decrypted Text To Save", ".txt",
                                                        {.path = "."});
            }

            ImGui::EndTable();
        }

        // Cell (1,2): Cipher title
        ImGui::TableSetColumnIndex(1);
        ImGui::Spacing();
        ImGui::TextCentered("Cipher");

        // Cell (1,3): Encrypted title
        ImGui::TableSetColumnIndex(2);
        if (ImGui::BeginTable("EncryptedHeader", 3,
                              ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoBordersInBody))
        {
            ImGui::TableSetupColumn("Text", ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableSetupColumn("Spacer", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Buttons", ImGuiTableColumnFlags_WidthFixed, 0.0f);

            ImGui::TableNextRow();

            // Cell 1: Text (Left Aligned)
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding(); // vertically aligned
            ImGui::Text("Encrypted");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("The encrypted text");
            }

            // Cell 2: Dummy
            ImGui::TableSetColumnIndex(1);

            // Cell 3: Buttons (Right Aligned)
            ImGui::TableSetColumnIndex(2);
            if (ImGui::ButtonPadded("Clear"))
            {
                encrypted_text_.clear();
            }
            ImGui::SameLine();
            if (ImGui::ButtonPadded("Load"))
            {
                ImGuiFileDialog::Instance()->OpenDialog("##LoadEncrypted", "Choose Encrypted Text To Load", ".txt",
                                                        {.path = "."});
            }
            ImGui::SameLine();
            if (ImGui::ButtonPadded("Save"))
            {
                ImGuiFileDialog::Instance()->OpenDialog("##SaveEncrypted", "Choose Encrypted Text To Save", ".txt",
                                                        {.path = "."});
            }

            ImGui::EndTable();
        }

        // Row 2: Content
        ImGui::TableNextRow();

        // Cell (2,1): Encrypted text input
        ImGui::TableSetColumnIndex(0);
        ImGui::PushFont(jetbrains_mono_regular_, 0.f);
        ImGui::InputTextMultiline("##Decrypted", &decrypted_text_, ImVec2{-1, -1});
        ImGui::PopFont();

        // Cell (2,2): Buttons and options
        ImGui::TableSetColumnIndex(1);

        // Stretch buttons in the column
        const auto button_width{ImGui::GetContentRegionAvail().x};
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - button_width) / 2);
        ImGui::PushItemWidth(button_width);

        ImGui::Spacing();
        ImGui::TextCentered("Cipher Type");
        const char *cipher_items[]{"Substitution", "Transposition", "Vigenère", "Rail Fence"};
        ImGui::Combo("##CipherType", reinterpret_cast<int *>(&selected_cipher_), cipher_items,
                     IM_ARRAYSIZE(cipher_items));
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Select the cipher algorithm to use");
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Encrypt", ImVec2{button_width, 0}))
        {
            [this]() -> eresult<std::string> {
                switch (selected_cipher_)
                {
                case cipher_type::substitution:
                    return substitution_cipher_.encrypt(decrypted_text_);
                case cipher_type::transposition:
                    return transposition_cipher_.encrypt(decrypted_text_);
                case cipher_type::vigenere:
                    vigenere_cipher_.set_key(vigenere_key_);
                    return vigenere_cipher_.encrypt(decrypted_text_);
                case cipher_type::rail_fence:
                    rail_fence_cipher_.set_rails(rail_fence_rails_);
                    return rail_fence_cipher_.encrypt(decrypted_text_);
                }
                return std::unexpected{"Unknown cipher type"};
            }()
                            .and_then([this](const std::string value) -> eresult<void> {
                                encrypted_text_ = std::move(value);
                                return {};
                            })
                            .or_else([this, &cipher_message](const std::string error) -> eresult<void> {
                                ImGui::OpenPopup("Error Encrypting");
                                cipher_message = std::move(error);
                                return std::unexpected{error};
                            })
                            .emplace();
        }
        if (ImGui::Button("Decrypt", ImVec2{button_width, 0}))
        {
            [this]() -> eresult<std::string> {
                switch (selected_cipher_)
                {
                case cipher_type::substitution:

                    return substitution_cipher_.decrypt(encrypted_text_);
                case cipher_type::transposition:
                    transposition_cipher_.set_key(transposition_key_);
                    return transposition_cipher_.decrypt(encrypted_text_);
                case cipher_type::vigenere:
                    vigenere_cipher_.set_key(vigenere_key_);
                    return vigenere_cipher_.decrypt(encrypted_text_);
                case cipher_type::rail_fence:
                    rail_fence_cipher_.set_rails(rail_fence_rails_);
                    return rail_fence_cipher_.decrypt(encrypted_text_);
                }
                return std::unexpected{"Unknown cipher type"};
            }()
                            .and_then([this](const std::string value) -> eresult<void> {
                                decrypted_text_ = std::move(value);
                                return {};
                            })
                            .or_else([this, &cipher_message](const std::string error) -> eresult<void> {
                                ImGui::OpenPopup("Error Decrypting");
                                cipher_message = std::move(error);
                                return std::unexpected{error};
                            })
                            .emplace();
        }

        if (selected_cipher_ == cipher_type::transposition)
        {
            if (ImGui::Button("Decrypt Brute", ImVec2{button_width, 0}))
            {
                ImGuiFileDialog::Instance()->OpenDialog("##SaveDecryptedBrute", "Choose Decrypted Texts To Save",
                                                        ".txt", {.path = "."});
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextCentered("Transposition Key");
            ImGui::InputInt("##TranspositionKey", &transposition_key_);
        }

        if (selected_cipher_ == cipher_type::vigenere)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextCentered("Vigenère Key");
            ImGui::InputText("##VignereKey", &vigenere_key_);
        }

        if (selected_cipher_ == cipher_type::rail_fence)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextCentered("Rails");
            ImGui::InputInt("##RailFenceKey", &rail_fence_rails_);
        }

        ImGui::InformationPopup("Error Encrypting", cipher_message.c_str(), [] {});
        ImGui::InformationPopup("Error Decrypting", cipher_message.c_str(), [] {});

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button(show_stats_window_ ? "Hide Statistics" : "Show Statistics", ImVec2{button_width, 0}))
        {
            show_stats_window_ = !show_stats_window_;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Toggle advanced cryptanalysis statistics window");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Exit", ImVec2{button_width, 0}))
        {
            ImGui::OpenPopup("Exit Confirmation");
        }

        ImGui::ConfirmationPopup("Exit Confirmation", "Are you sure to exit?", [this] { should_exit_ = true; });

        ImGui::PopItemWidth();

        // Cell (2,3): Decrypted text input
        ImGui::TableSetColumnIndex(2);
        ImGui::PushFont(jetbrains_mono_regular_, 0.f);
        ImGui::InputTextMultiline("##Encrypted", &encrypted_text_, ImVec2{-1, -1});
        ImGui::PopFont();

        ImGui::EndTable();
    }

    // ImGui::PopFont();
}

void gui::render_stats_window() noexcept
{
    if (ImGui::Begin("Cryptanalysis Statistics", &show_stats_window_,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
    {
        // Tab bar for different analysis views
        if (ImGui::BeginTabBar("StatsTabBar"))
        {
            // Tab 1: Frequency Analysis with Histogram
            if (ImGui::BeginTabItem("Frequency Analysis"))
            {
                ImGui::TextCentered("Letter Frequency Distribution");
                ImGui::Spacing();

                const auto frequencies{tprotect::cipher::frequency_analyzer::analyze(encrypted_text_)};

                if (frequencies.empty())
                {
                    ImGui::TextCentered("No letters found in encrypted text");
                }
                else
                {
                    // Prepare data for histogram
                    std::vector<float> values;
                    std::vector<const char *> labels;
                    for (const auto &freq : frequencies)
                    {
                        values.push_back(freq.percentage);
                        labels.push_back(std::string{freq.letter}.c_str());
                    }

                    // Draw vertical histogram
                    ImGui::HistogramBarsVertical("FrequencyHistogram", values, labels, 100.0f, ImVec2{-1, 300});
                }

                ImGui::EndTabItem();
            }

            // Tab 2: Advanced Statistics & Cipher Detection
            if (ImGui::BeginTabItem("Cipher Detection"))
            {
                ImGui::TextCentered("Automatic Cipher Type Detection");
                ImGui::Spacing();

                if (encrypted_text_.empty())
                {
                    ImGui::TextCentered("No encrypted text to analyze");
                }
                else
                {
                    const auto stats{tprotect::cipher::advanced_analyzer::get_statistics(encrypted_text_)};

                    ImGui::Separator();
                    ImGui::Text("Index of Coincidence (IC): %.4f", stats.index_of_coincidence);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(?)");
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("IC ≈ 0.065 for English, ≈ 0.038 for random text");
                    }

                    ImGui::Text("Shannon Entropy: %.4f bits", stats.entropy);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(?)");
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("Higher entropy = more random, lower = more structured");
                    }

                    ImGui::Text("Chi-Squared (χ²): %.2f", stats.chi_squared);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(?)");
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("Lower χ² = closer to English, higher = more different");
                    }

                    ImGui::Separator();
                    ImGui::Text("Total Letters: %d", stats.total_letters);
                    ImGui::Text("Unique Letters: %d", stats.unique_letters);

                    ImGui::Separator();
                    ImGui::TextWrapped("Encryption Status & Method Detection:");
                    ImGui::Spacing();

                    // Determine if text is encrypted
                    bool is_likely_encrypted{false};
                    std::string detected_cipher{"Unknown"};
                    float confidence{0.0f};

                    // Check if it's likely English (not encrypted)
                    if (stats.chi_squared < 100.0f && stats.index_of_coincidence > 0.055f)
                    {
                        ImGui::TextColored(ImVec4{1.0f, 0.5f, 0.0f, 1.0f},
                                           "⚠ Text appears to be UNENCRYPTED (English)");
                        is_likely_encrypted = false;
                    }
                    else
                    {
                        is_likely_encrypted = true;
                        ImGui::TextColored(ImVec4{1.0f, 1.0f, 0.0f, 1.0f}, "✓ Text appears to be ENCRYPTED");
                        ImGui::Spacing();

                        // Analyze which cipher type is most likely
                        const auto bigrams{tprotect::cipher::advanced_analyzer::analyze_bigrams(encrypted_text_)};
                        const auto trigrams{tprotect::cipher::advanced_analyzer::analyze_trigrams(encrypted_text_)};

                        // Substitution cipher detection
                        // - High IC (>0.06), moderate entropy, chi-squared varies
                        // - Bigrams/trigrams show patterns
                        float substitution_score{0.0f};
                        if (stats.index_of_coincidence > 0.055f)
                            substitution_score += 40.0f;
                        if (stats.entropy < 4.5f)
                            substitution_score += 30.0f;
                        if (!bigrams.empty() && bigrams[0].percentage > 2.0f)
                            substitution_score += 30.0f;

                        // Transposition cipher detection
                        // - High IC (similar to original), high entropy, chi-squared similar to English
                        // - Letter frequencies similar to English
                        float transposition_score{0.0f};
                        if (stats.index_of_coincidence > 0.06f && stats.index_of_coincidence < 0.07f)
                            transposition_score += 40.0f;
                        if (stats.chi_squared < 150.0f)
                            transposition_score += 30.0f;
                        if (stats.entropy > 4.0f && stats.entropy < 5.0f)
                            transposition_score += 30.0f;

                        // Vigenère cipher detection
                        // - Medium IC (0.04-0.06), medium entropy
                        // - Repeating patterns in bigrams/trigrams
                        float vigenere_score{0.0f};
                        if (stats.index_of_coincidence > 0.04f && stats.index_of_coincidence < 0.065f)
                            vigenere_score += 40.0f;
                        if (stats.entropy > 3.5f && stats.entropy < 4.5f)
                            vigenere_score += 30.0f;
                        if (!bigrams.empty() && bigrams.size() > 50)
                            vigenere_score += 30.0f;

                        // Rail Fence cipher detection
                        // - High IC, specific entropy pattern
                        // - Unusual bigram distribution
                        float rail_fence_score{0.0f};
                        if (stats.index_of_coincidence > 0.06f)
                            rail_fence_score += 30.0f;
                        if (stats.entropy > 4.2f && stats.entropy < 4.8f)
                            rail_fence_score += 35.0f;
                        if (stats.unique_letters > 20)
                            rail_fence_score += 35.0f;

                        // Normalize scores
                        float total_score{substitution_score + transposition_score + vigenere_score + rail_fence_score};
                        if (total_score > 0)
                        {
                            substitution_score = (substitution_score / total_score) * 100.0f;
                            transposition_score = (transposition_score / total_score) * 100.0f;
                            vigenere_score = (vigenere_score / total_score) * 100.0f;
                            rail_fence_score = (rail_fence_score / total_score) * 100.0f;
                        }

                        // Display detection results
                        ImGui::Spacing();
                        ImGui::Text("Cipher Type Probabilities:");
                        ImGui::Spacing();

                        ImGui::Text("Substitution Cipher: %.1f%%", substitution_score);
                        ImGui::ProgressBar(substitution_score / 100.0f, ImVec2{-1, 0});

                        ImGui::Text("Transposition Cipher: %.1f%%", transposition_score);
                        ImGui::ProgressBar(transposition_score / 100.0f, ImVec2{-1, 0});

                        ImGui::Text("Vigenère Cipher: %.1f%%", vigenere_score);
                        ImGui::ProgressBar(vigenere_score / 100.0f, ImVec2{-1, 0});

                        ImGui::Text("Rail Fence Cipher: %.1f%%", rail_fence_score);
                        ImGui::ProgressBar(rail_fence_score / 100.0f, ImVec2{-1, 0});

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        // Determine most likely cipher
                        if (substitution_score >= transposition_score && substitution_score >= vigenere_score &&
                            substitution_score >= rail_fence_score)
                        {
                            ImGui::TextColored(ImVec4{0.0f, 1.0f, 0.0f, 1.0f},
                                               "Most Likely: SUBSTITUTION CIPHER (%.1f%%)", substitution_score);
                        }
                        else if (transposition_score >= vigenere_score && transposition_score >= rail_fence_score)
                        {
                            ImGui::TextColored(ImVec4{0.0f, 1.0f, 0.0f, 1.0f},
                                               "Most Likely: TRANSPOSITION CIPHER (%.1f%%)", transposition_score);
                        }
                        else if (vigenere_score >= rail_fence_score)
                        {
                            ImGui::TextColored(ImVec4{0.0f, 1.0f, 0.0f, 1.0f}, "Most Likely: VIGENÈRE CIPHER (%.1f%%)",
                                               vigenere_score);
                        }
                        else
                        {
                            ImGui::TextColored(ImVec4{0.0f, 1.0f, 0.0f, 1.0f},
                                               "Most Likely: RAIL FENCE CIPHER (%.1f%%)", rail_fence_score);
                        }
                    }
                }

                ImGui::EndTabItem();
            }

            // Tab 3: Bigram/Trigram Analysis
            if (ImGui::BeginTabItem("N-gram Analysis"))
            {
                ImGui::TextCentered("Bigram and Trigram Frequencies");
                ImGui::Spacing();

                // Bigrams
                ImGui::Text("Top Bigrams:");
                const auto bigrams{tprotect::cipher::advanced_analyzer::analyze_bigrams(encrypted_text_)};
                if (bigrams.empty())
                {
                    ImGui::TextCentered("No bigrams found");
                }
                else
                {
                    if (ImGui::BeginTable("BigramTable", 3,
                                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_SizingStretchProp))
                    {
                        ImGui::TableSetupColumn("Bigram", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                        ImGui::TableSetupColumn("Frequency", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                        ImGui::TableHeadersRow();

                        for (size_t i{}; i < std::min(size_t{10}, bigrams.size()); ++i)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%s", bigrams[i].bigram.c_str());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%d", bigrams[i].count);
                            ImGui::TableSetColumnIndex(2);
                            ImGui::ProgressBar(bigrams[i].percentage / 100.0f, ImVec2{-1, 0},
                                               std::format("{:.2f}%", bigrams[i].percentage).c_str());
                        }

                        ImGui::EndTable();
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Trigrams
                ImGui::Text("Top Trigrams:");
                const auto trigrams{tprotect::cipher::advanced_analyzer::analyze_trigrams(encrypted_text_)};
                if (trigrams.empty())
                {
                    ImGui::TextCentered("No trigrams found");
                }
                else
                {
                    if (ImGui::BeginTable("TrigramTable", 3,
                                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_SizingStretchProp))
                    {
                        ImGui::TableSetupColumn("Trigram", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                        ImGui::TableSetupColumn("Frequency", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                        ImGui::TableHeadersRow();

                        for (size_t i{}; i < std::min(size_t{10}, trigrams.size()); ++i)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%s", trigrams[i].trigram.c_str());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%d", trigrams[i].count);
                            ImGui::TableSetColumnIndex(2);
                            ImGui::ProgressBar(trigrams[i].percentage / 100.0f, ImVec2{-1, 0},
                                               std::format("{:.2f}%", trigrams[i].percentage).c_str());
                        }

                        ImGui::EndTable();
                    }
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

[[nodiscard]] eresult<void> gui::process_file() noexcept
{
    return read_file_dialog("##LoadEncrypted", encrypted_text_)
        .and_then([this] { return read_file_dialog("##LoadDecrypted", decrypted_text_); })
        .and_then([this] { return write_file_dialog("##SaveEncrypted", encrypted_text_); })
        .and_then([this] { return write_file_dialog("##SaveDecrypted", decrypted_text_); })
        .and_then([this] {
            return display_file_dialog("##SaveDecryptedBrute")
                .transform([this](const std::string path) -> eresult<void> {
                    std::ranges::for_each(std::views::iota(1, 27), [&](const int i) {
                        tprotect::cipher::transposition_cipher cipher{i};
                        std::filesystem::path fs_path{path}, fs_extention{fs_path.extension()};
                        return cipher.decrypt(encrypted_text_).and_then([&](const std::string decrypted_text) {
                            return write_file(fs_path.replace_extension()
                                                  .replace_filename({std::format("{}_{}{}", fs_path.filename().string(),
                                                                                 i, fs_extention.string())})
                                                  .string(),
                                              std::move(decrypted_text));
                        });
                    });
                    ImGui::OpenPopup("Save Decrypt Brute");
                    ImGui::InformationPopup("Save Decrypt Brute", "Successfully decrypted brute-forcely", [] {});
                    return {};
                })
                .value_or({});
        });
}
} // namespace tprotect
