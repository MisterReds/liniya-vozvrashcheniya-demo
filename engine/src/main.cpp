#include "returnline/PixelBuffer.hpp"
#include "PixelFont.hpp"
#include "PixelArt.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr double kFixedStep = 1.0 / 60.0;
constexpr std::uint32_t Color(unsigned r, unsigned g, unsigned b) {
    return 0xff000000u | (r << 16u) | (g << 8u) | b;
}

enum class Phase { Title, Day, Night, Won, Lost };
struct Cache { float x; bool fuel; int amount; bool used{}; bool medicine{}; };
struct Enemy { float x; int health{2}; float attackTimer{0.8f}; float animation{}; };
struct Survivor { float x; float target; float work{}; float animation{}; };

class EngineApp {
public:
    ~EngineApp() { Shutdown(); }

    bool Initialize() {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            std::cerr << "SDL_Init: " << SDL_GetError() << '\n';
            return false;
        }
        window_ = SDL_CreateWindow("Линия возвращения — узел 01",
                                   kWindowWidth, kWindowHeight, SDL_WINDOW_RESIZABLE);
        if (!window_) {
            std::cerr << "SDL_CreateWindow: " << SDL_GetError() << '\n';
            return false;
        }
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (!renderer_) {
            std::cerr << "SDL_CreateRenderer: " << SDL_GetError() << '\n';
            return false;
        }
        frame_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   returnline::PixelBuffer::Width,
                                   returnline::PixelBuffer::Height);
        if (!frame_) {
            std::cerr << "SDL_CreateTexture: " << SDL_GetError() << '\n';
            return false;
        }
        SDL_SetTextureScaleMode(frame_, SDL_SCALEMODE_NEAREST);
        lastTick_ = std::chrono::steady_clock::now();
        ResetGame();
        return true;
    }

    int Run(const bool oneFrame) {
#ifdef __EMSCRIPTEN__
        if (oneFrame) {
            Frame();
            return 0;
        }
        emscripten_set_main_loop_arg(&EngineApp::MainLoopThunk, this, 0, 0);
        return 0;
#else
        while (running_) {
            Frame();
            if (oneFrame) running_ = false;
        }
        return 0;
#endif
    }

private:
#ifdef __EMSCRIPTEN__
    static void MainLoopThunk(void* context) {
        auto* app = static_cast<EngineApp*>(context);
        app->Frame();
        if (!app->running_) emscripten_cancel_main_loop();
    }
#endif

    void Frame() {
        PumpEvents();
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = now - lastTick_;
        lastTick_ = now;
        accumulator_ += std::min(elapsed.count(), 0.25);
        while (accumulator_ >= kFixedStep) {
            Update(kFixedStep);
            accumulator_ -= kFixedStep;
        }
        Draw();
    }

    void Shutdown() {
        if (gamepad_) SDL_CloseGamepad(gamepad_);
        if (frame_) SDL_DestroyTexture(frame_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
        gamepad_ = nullptr;
        frame_ = nullptr;
        renderer_ = nullptr;
        window_ = nullptr;
    }

    void PumpEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running_ = false;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) running_ = false;
                if (event.key.scancode == SDL_SCANCODE_RETURN || event.key.scancode == SDL_SCANCODE_SPACE)
                    startPressed_ = true;
                if (event.key.scancode == SDL_SCANCODE_E) interactPressed_ = true;
                if (event.key.scancode == SDL_SCANCODE_N) nightPressed_ = true;
                if (event.key.scancode == SDL_SCANCODE_Q) buildPressed_ = true;
                if (event.key.scancode == SDL_SCANCODE_1) assignment_ = 1;
                if (event.key.scancode == SDL_SCANCODE_2) assignment_ = 2;
                if (event.key.scancode == SDL_SCANCODE_3) assignment_ = 3;
                if (event.key.scancode == SDL_SCANCODE_H) healPressed_ = true;
                if (event.key.scancode == SDL_SCANCODE_SPACE) attackPressed_ = true;
                if (event.key.scancode == SDL_SCANCODE_R) restartPressed_ = true;
            }
            if (event.type == SDL_EVENT_GAMEPAD_ADDED && !gamepad_)
                gamepad_ = SDL_OpenGamepad(event.gdevice.which);
            if (event.type == SDL_EVENT_GAMEPAD_REMOVED && gamepad_ &&
                SDL_GetGamepadID(gamepad_) == event.gdevice.which) {
                SDL_CloseGamepad(gamepad_);
                gamepad_ = nullptr;
            }
        }
    }

    void Update(const double dt) {
        if (phase_ == Phase::Title) {
            if (startPressed_) BeginDay();
            ClearPressed();
            return;
        }
        if (phase_ == Phase::Won || phase_ == Phase::Lost) {
            if (restartPressed_) ResetGame();
            ClearPressed();
            return;
        }

        const bool* keys = SDL_GetKeyboardState(nullptr);
        float axis = 0.0f;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) axis -= 1.0f;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) axis += 1.0f;
        if (gamepad_) {
            const float stick = static_cast<float>(SDL_GetGamepadAxis(
                gamepad_, SDL_GAMEPAD_AXIS_LEFTX)) / 32767.0f;
            if (std::abs(stick) > 0.2f) axis = stick;
        }

        const float before = playerX_;
        playerX_ = std::clamp(playerX_ + axis * 190.0f * static_cast<float>(dt), 24.0f, worldWidth_ - 24.0f);
        if (axis != 0.0f) {
            facing_ = axis < 0.0f ? -1 : 1;
            walkTime_ += dt * 10.0;
        } else {
            walkTime_ = 0.0;
        }
        moving_ = std::abs(playerX_ - before) > 0.01f;
        attackTime_ = std::max(0.0f, attackTime_ - static_cast<float>(dt));
        worldTime_ += dt;

        if (phase_ == Phase::Day) UpdateDay(dt);
        if (phase_ == Phase::Night) UpdateNight(dt);
        if (interactPressed_) Interact();
        if (assignment_ != 0) AssignPeople(assignment_);
        if (healPressed_) Heal();
        if (nightPressed_ && generatorOn_ && phase_ == Phase::Day) StartNight();
        if (buildPressed_ && phase_ == Phase::Night && std::abs(playerX_ - gateX_) < 86.0f) {
            if (scrap_ >= 2 && !barricade_) { scrap_ -= 2; barricade_ = true; toast_ = "ВОРОТА УКРЕПЛЕНЫ"; }
            else if (barricade_) toast_ = "УКРЕПЛЕНИЕ УЖЕ ЕСТЬ";
            else toast_ = "НУЖНО 2 ЕД. ЛОМА";
        }
        if ((attackPressed_ || keys[SDL_SCANCODE_SPACE]) && phase_ == Phase::Night) Attack();

        if (phase_ == Phase::Night && nightTime_ >= nightLength_) phase_ = Phase::Won;
        if (gateHealth_ <= 0.0f) phase_ = Phase::Lost;
        if (playerHealth_ <= 0.0f) phase_ = Phase::Lost;
        cameraX_ = std::clamp(playerX_ - 205.0f, 0.0f, worldWidth_ - 640.0f);
        toastTimer_ = std::max(0.0f, toastTimer_ - static_cast<float>(dt));
        if (toastTimer_ <= 0.0f) toast_.clear();
        ClearPressed();
    }

    void ClearPressed() {
        startPressed_ = interactPressed_ = nightPressed_ = buildPressed_ = attackPressed_ = restartPressed_ = healPressed_ = false;
        assignment_ = 0;
    }

    void BeginDay() {
        phase_ = Phase::Day;
        playerX_ = 104.0f;
        toast_ = "МАРТА: ГЕНЕРАТОРУ НУЖЕН ЛОМ И ТОПЛИВО";
        toastTimer_ = 4.0f;
    }

    void ResetGame() {
        phase_ = Phase::Title;
        playerX_ = 104.0f;
        scrap_ = 0;
        fuel_ = 0;
        medicine_ = 1;
        clueFound_ = false;
        assignmentMode_ = 1;
        playerHealth_ = 100.0f;
        playerDamageTimer_ = 0.0f;
        repairWork_ = 0.0f;
        trackFixed_ = false;
        gateHealth_ = 100.0f;
        generatorOn_ = false;
        barricade_ = false;
        dayTime_ = nightTime_ = spawnTime_ = 0.0f;
        cameraX_ = 0.0f;
        caches_ = {{270.0f, false, 2}, {770.0f, false, 2}, {960.0f, true, 2},
                   {1065.0f, false, 1, false, true}};
        enemies_.clear();
        marta_ = {390.0f, 320.0f, 0.0f, 0.0f};
        ilya_ = {450.0f, 450.0f, 0.0f, 0.0f};
        toast_.clear();
    }

    void UpdateDay(const double dt) {
        dayTime_ += dt;
        MoveSurvivor(marta_, assignmentMode_ == 1 ? 320.0f : (assignmentMode_ == 2 ? generatorX_ : gateX_), dt);
        MoveSurvivor(ilya_, assignmentMode_ == 1 ? 450.0f : (assignmentMode_ == 2 ? generatorX_ + 26 : gateX_ + 24), dt);
        if (assignmentMode_ == 1 && std::abs(marta_.x - marta_.target) < 2.0f) {
            marta_.work += static_cast<float>(dt);
            if (marta_.work >= 6.0f) {
                marta_.work -= 6.0f;
                ++scrap_;
                toast_ = "МАРТА НАШЛА ЛОМ";
                toastTimer_ = 2.0f;
            }
        }
        if (assignmentMode_ == 2 && !generatorOn_ && dayTime_ > 1.0 && scrap_ >= 3 && fuel_ >= 2) {
            repairWork_ += static_cast<float>(dt);
            if (repairWork_ >= 5.0f) {
                repairWork_ = 0.0f;
                scrap_ -= 3;
                fuel_ -= 2;
                generatorOn_ = true;
                toast_ = "ИНЖЕНЕР ЗАПУСТИЛ ГЕНЕРАТОР";
                toastTimer_ = 2.5f;
            }
        } else if (assignmentMode_ == 2 && generatorOn_ && !trackFixed_ && scrap_ >= 2) {
            repairWork_ += static_cast<float>(dt);
            if (repairWork_ >= 8.0f) {
                repairWork_ = 0.0f;
                scrap_ -= 2;
                trackFixed_ = true;
                toast_ = "ИНЖЕНЕР ВОССТАНОВИЛ УЧАСТОК ПУТИ";
                toastTimer_ = 3.0f;
            }
        }
    }

    void MoveSurvivor(Survivor& person, const float target, const double dt) {
        person.target = target;
        const float delta = target - person.x;
        if (std::abs(delta) > 1.0f) {
            person.x += std::copysign(std::min(std::abs(delta), 48.0f * static_cast<float>(dt)), delta);
            person.animation += static_cast<float>(dt) * 8.0f;
        } else {
            person.x = target;
            person.animation = 0.0f;
        }
    }

    void UpdateNight(const double dt) {
        nightTime_ += dt;
        spawnTime_ += static_cast<float>(dt);
        if (spawnTime_ > 3.8f && nightTime_ < nightLength_ - 2.0f) {
            spawnTime_ = 0.0f;
            enemies_.push_back({worldWidth_ - 24.0f, 2, 0.8f, 0.0f});
        }
        for (auto& enemy : enemies_) {
            enemy.animation += static_cast<float>(dt) * 8.0f;
            enemy.attackTimer -= static_cast<float>(dt);
            if (enemy.x > gateX_ + 10.0f) {
                enemy.x -= 46.0f * static_cast<float>(dt);
            } else if (enemy.attackTimer <= 0.0f) {
                enemy.attackTimer = barricade_ ? 1.9f : 1.3f;
                const float damage = assignmentMode_ == 3 ? 1.5f : (barricade_ ? 3.0f : 7.0f);
                gateHealth_ -= damage;
                toast_ = barricade_ ? "УКРЕПЛЕНИЕ ПРИНЯЛО УДАР" : "УДАР ПО ВОРОТАМ";
                toastTimer_ = 1.4f;
            }
            if (std::abs(enemy.x - playerX_) < 42.0f && playerDamageTimer_ <= 0.0f) {
                playerHealth_ -= 24.0f;
                playerDamageTimer_ = 1.2f;
                toast_ = "ТЕБЯ РАНИЛИ — H ИСПОЛЬЗУЙ АПТЕЧКУ";
                toastTimer_ = 2.0f;
            }
        }
        playerDamageTimer_ = std::max(0.0f, playerDamageTimer_ - static_cast<float>(dt));
        enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [](const Enemy& e) {
            return e.health <= 0;
        }), enemies_.end());
    }

    void Interact() {
        if (phase_ != Phase::Day) return;
        if (!clueFound_ && std::abs(playerX_ - 1125.0f) < 48.0f) {
            clueFound_ = true;
            toast_ = "СЛЕД В ТЁПЛОМ ПЕПЛЕ. НИГДЕ НЕ РАСТАЯЛ.";
            toastTimer_ = 3.0f;
            return;
        }
        if (!trackFixed_ && std::abs(playerX_ - 875.0f) < 48.0f) {
            if (scrap_ >= 2) {
                scrap_ -= 2;
                trackFixed_ = true;
                toast_ = "ПУТЬ К ВОСТОКУ ВОССТАНОВЛЕН";
            } else {
                toast_ = "ДЛЯ РЕМОНТА ПУТИ НУЖНЫ 2 ЕД. ЛОМА";
            }
            toastTimer_ = 2.5f;
            return;
        }
        for (auto& cache : caches_) {
            if (!cache.used && std::abs(playerX_ - cache.x) < 42.0f) {
                cache.used = true;
                if (cache.medicine) medicine_ += cache.amount;
                else if (cache.fuel) fuel_ += cache.amount;
                else scrap_ += cache.amount;
                toast_ = cache.medicine ? "АПТЕЧКА +1" : (cache.fuel ? "ТОПЛИВО +2" : "ЛОМ +2");
                toastTimer_ = 1.8f;
                return;
            }
        }
        if (!generatorOn_ && std::abs(playerX_ - generatorX_) < 52.0f) {
            if (scrap_ >= 3 && fuel_ >= 2) {
                scrap_ -= 3;
                fuel_ -= 2;
                generatorOn_ = true;
                toast_ = "ГЕНЕРАТОР РАБОТАЕТ — К ВОРОТАМ";
                toastTimer_ = 3.0f;
            } else {
                toast_ = "НУЖНО 3 ЛОМА И 2 ТОПЛИВА";
                toastTimer_ = 2.5f;
            }
            return;
        }
        toast_ = "ПОДОЙДИ К ОБЪЕКТУ БЛИЖЕ";
        toastTimer_ = 1.8f;
    }

    void StartNight() {
        if (std::abs(playerX_ - gateX_) > 92.0f) {
            toast_ = "ПОДОЙДИ К ВОРОТАМ, ЧТОБЫ НАЧАТЬ НОЧЬ";
            toastTimer_ = 2.0f;
            return;
        }
        phase_ = Phase::Night;
        nightTime_ = spawnTime_ = 0.0f;
        toast_ = "ДЕРЖИ ОБОРОНУ ДО РАССВЕТА";
        toastTimer_ = 2.4f;
    }

    void AssignPeople(const int mode) {
        if (phase_ != Phase::Day) {
            toast_ = "ЛЮДЕЙ МОЖНО ПЕРЕНАЗНАЧИТЬ ДНЁМ";
            toastTimer_ = 2.0f;
            return;
        }
        assignmentMode_ = mode;
        toast_ = mode == 1 ? "ЛЮДИ ИЩУТ МАТЕРИАЛЫ" : (mode == 2 ? "ИНЖЕНЕР РЕМОНТИРУЕТ УЗЕЛ" : "ЛЮДИ УСИЛИВАЮТ ОХРАНУ");
        toastTimer_ = 2.2f;
    }

    void Heal() {
        if (medicine_ <= 0) {
            toast_ = "АПТЕЧЕК НЕ ОСТАЛОСЬ";
            toastTimer_ = 1.5f;
            return;
        }
        if (phase_ != Phase::Night) {
            toast_ = "ОСТАВЬ АПТЕЧКУ НА НОЧЬ";
            toastTimer_ = 1.5f;
            return;
        }
        --medicine_;
        playerHealth_ = std::min(100.0f, playerHealth_ + 45.0f);
        toast_ = "АПТЕЧКА ИСПОЛЬЗОВАНА: ЗДОРОВЬЕ ВОССТАНОВЛЕНО";
        toastTimer_ = 2.0f;
    }

    void Attack() {
        if (attackTime_ > 0.0f) return;
        attackTime_ = 0.34f;
        for (auto& enemy : enemies_) {
            if (std::abs(enemy.x - playerX_) < 66.0f) {
                --enemy.health;
                toast_ = enemy.health > 0 ? "ПОПАЛ — ЕЩЁ УДАР" : "ТВАРЬ ПОВАЛЕНА";
                toastTimer_ = 1.0f;
                return;
            }
        }
        toast_ = "ВРАГ СЛИШКОМ ДАЛЕКО";
        toastTimer_ = 1.0f;
    }

    std::array<int, 8> Glyph(std::uint32_t c) const {
        if (c >= 0x430 && c <= 0x44f) c -= 0x20;
        if (c == 0x451) c = 0x401;
        if (c == 0x2014) c = '-';
        if (c < 128) c = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(c)));
        for (const auto& glyph : returnline::kPixelGlyphs)
            if (glyph.codepoint == c) {
                std::array<int, 8> rows{};
                std::copy(glyph.rows.begin(), glyph.rows.end(), rows.begin());
                return rows;
            }
        return {};
    }

    void Text(std::string_view value, int x, const int y, const int scale,
              const std::uint32_t color) {
        for (std::size_t i = 0; i < value.size();) {
            const auto first = static_cast<unsigned char>(value[i++]);
            std::uint32_t codepoint = first;
            if ((first & 0xe0) == 0xc0 && i < value.size())
                codepoint = ((first & 0x1f) << 6) | (static_cast<unsigned char>(value[i++]) & 0x3f);
            else if ((first & 0xf0) == 0xe0 && i + 1 < value.size())
            {
                const auto second = static_cast<unsigned char>(value[i++]);
                const auto third = static_cast<unsigned char>(value[i++]);
                codepoint = ((first & 0x0f) << 12) | ((second & 0x3f) << 6) | (third & 0x3f);
            }
            if (codepoint == ' ') { x += scale * 6; continue; }
            const auto rows = Glyph(codepoint);
            for (int row = 0; row < 8; ++row)
                for (int column = 0; column < 5; ++column)
                    if ((rows[row] & (1 << (4 - column))) != 0)
                        pixels_.FillRect(x + column * scale, y + row * scale, scale, scale, color);
            x += scale * 6;
        }
    }

    int ScreenX(const float worldX) const { return static_cast<int>(std::round(worldX - cameraX_)); }

    template <std::size_t Rows>
    void Sprite(const std::array<std::string_view, Rows>& rows, int x, int y,
                const bool scavenger, const bool creature = false, const bool fuel = false) {
        for (std::size_t row = 0; row < rows.size(); ++row) {
            for (std::size_t column = 0; column < rows[row].size(); ++column) {
                const char pixel = rows[row][column];
                std::uint32_t color{};
                if (pixel == '.') continue;
                if (creature) {
                    if (pixel == 'o') color = Color(35, 41, 35);
                    else if (pixel == 'O') color = Color(111, 98, 73);
                    else color = Color(185, 157, 92);
                } else if (fuel) {
                    if (pixel == 'o') color = Color(49, 41, 34);
                    else if (pixel == 'h') color = Color(210, 157, 85);
                    else if (pixel == 'g') color = Color(56, 65, 57);
                    else color = Color(137, 75, 43);
                } else if (rows.size() == 14) {
                    if (pixel == 'o') color = Color(45, 47, 38);
                    else if (pixel == 'h') color = Color(172, 146, 96);
                    else if (pixel == 'g') color = Color(77, 81, 60);
                    else color = Color(102, 91, 65);
                } else {
                    if (pixel == 'o') color = Color(37, 39, 35);
                    else if (pixel == 's') color = Color(182, 151, 117);
                    else if (pixel == 'h') color = Color(72, 69, 52);
                    else if (pixel == 'p') color = Color(48, 55, 48);
                    else if (pixel == 'b') color = Color(34, 34, 30);
                    else if (pixel == 'g') color = Color(188, 150, 84);
                    else if (scavenger) color = Color(101, 107, 75);
                    else color = Color(67, 92, 85);
                }
                pixels_.FillRect(x + static_cast<int>(column), y + static_cast<int>(row), 1, 1, color);
            }
        }
    }

    template <std::size_t Rows>
    void Tile(const std::array<std::string_view, Rows>& rows, int x, int y, const bool brick) {
        for (std::size_t row = 0; row < rows.size(); ++row) {
            for (std::size_t column = 0; column < rows[row].size(); ++column) {
                const char pixel = rows[row][column];
                if (pixel == '.') continue;
                std::uint32_t color{};
                if (brick) color = Color(43, 50, 46);
                else if (pixel == 's') color = Color(42, 49, 42);
                else if (pixel == 't') color = Color(72, 74, 59);
                else if (pixel == 'l') color = Color(100, 94, 70);
                else color = Color(86, 68, 50);
                pixels_.FillRect(x + static_cast<int>(column), y + static_cast<int>(row), 1, 1, color);
            }
        }
    }

    void Human(const float worldX, const bool scavenger, const float animation,
               const bool working = false, const int facing = 1) {
        constexpr int ground = 275;
        const int x = ScreenX(worldX);
        if (x < -32 || x > 672) return;
        const int step = working ? 0 : static_cast<int>(std::sin(animation) * 1.5f);
        Sprite(returnline::art::Survivor, x - 8, ground - 24 + step, scavenger);
        if (working) {
            const int handX = x + (facing > 0 ? 8 : -8);
            pixels_.FillRect(handX, ground - 8 - static_cast<int>((std::sin(animation * 5.0f) + 1) * 2), 1, 8, Color(143, 121, 82));
            pixels_.FillRect(handX - 2, ground - 11, 5, 1, Color(189, 164, 111));
        }
    }

    void DrawCache(const Cache& cache) {
        if (cache.used) return;
        const int x = ScreenX(cache.x);
        constexpr int ground = 275;
        if (x < -40 || x > 680) return;
        if (cache.medicine) {
            Sprite(returnline::art::MedKit, x - 6, ground - 12, true);
            pixels_.FillRect(x - 1, ground - 10, 3, 7, Color(181, 76, 58));
            pixels_.FillRect(x - 3, ground - 8, 7, 3, Color(181, 76, 58));
        } else if (!cache.fuel) Sprite(returnline::art::ScrapCrate, x - 10, ground - 15, false);
        else Sprite(returnline::art::FuelCans, x - 11, ground - 14, false, false, true);
    }

    void DrawEnemy(const Enemy& enemy) {
        constexpr int ground = 275;
        const int x = ScreenX(enemy.x);
        if (x < -36 || x > 676) return;
        const int step = static_cast<int>(std::sin(enemy.animation) * 1.5f);
        Sprite(returnline::art::Mutant, x - 12, ground - 22 + step, false, true);
    }

    void DrawPixelScene() {
        constexpr int ground = 275;
        const bool night = phase_ == Phase::Night || phase_ == Phase::Lost;
        pixels_.Clear(night ? Color(22, 32, 39) : Color(42, 55, 59));
        pixels_.FillRect(0, 0, 640, 110, night ? Color(21, 31, 40) :
                        (generatorOn_ ? Color(78, 77, 62) : Color(48, 63, 68)));

        // Distant broken depot skyline and power masts.
        const int depotX = 22 - static_cast<int>(cameraX_ * .15f);
        pixels_.FillRect(depotX, 157, 190, 112, Color(55, 61, 53));
        for (int ty = 157; ty < 269; ty += 16)
            for (int tx = depotX; tx < depotX + 190; tx += 16)
                Tile(returnline::art::StationBrick, tx, ty, true);
        pixels_.FillRect(depotX - 9, 148, 208, 10, Color(88, 91, 75));
        pixels_.FillRect(depotX + 33, 176, 34, 26, Color(19, 28, 29));
        pixels_.FillRect(depotX + 30, 173, 40, 3, Color(112, 117, 99));
        pixels_.FillRect(depotX + 38, 179, 3, 20, Color(73, 82, 74));
        pixels_.FillRect(depotX + 57, 179, 3, 20, Color(73, 82, 74));
        pixels_.FillRect(depotX + 6, 216, 76, 17, Color(34, 42, 39));
        Text("УЗЕЛ 01", depotX + 10, 221, 1, Color(177, 158, 112));
        pixels_.FillRect(depotX + 112, 213, 3, 15, Color(89, 82, 64));
        pixels_.FillRect(depotX + 112, 229, 17, 3, Color(89, 82, 64));
        pixels_.FillRect(200, 115, 5, 160, Color(39, 49, 46));
        pixels_.FillRect(194, 119, 18, 5, Color(105, 109, 87));
        pixels_.FillRect(610, 91, 5, 184, Color(43, 53, 48));
        pixels_.FillRect(599, 95, 27, 5, Color(117, 119, 93));
        for (int x = 214; x < 610; x += 112) {
            const int sx = x - static_cast<int>(cameraX_ * .35f);
            pixels_.FillRect(sx, 174, 7, 101, Color(40, 51, 47));
            pixels_.FillRect(sx - 12, 170, 32, 6, Color(86, 97, 77));
        }

        // Seamless repeating ballast tiles and railway sleepers.
        pixels_.FillRect(0, ground, 640, 85, Color(53, 59, 50));
        const int firstTile = static_cast<int>(cameraX_) / 16 * 16;
        for (int wy = ground; wy < ground + 85; wy += 16)
            for (int wx = firstTile; wx < firstTile + 704; wx += 16)
                Tile(returnline::art::Ballast, wx - static_cast<int>(cameraX_), wy, false);
        pixels_.FillRect(0, ground, 640, 3, Color(120, 114, 87));
        pixels_.FillRect(0, ground + 48, 640, 4, Color(34, 39, 37));
        pixels_.FillRect(0, ground + 67, 640, 4, Color(34, 39, 37));
        for (int wx = firstTile; wx < firstTile + 704; wx += 36)
            pixels_.FillRect(wx - static_cast<int>(cameraX_), ground + 43, 5, 34, Color(94, 84, 62));
        if (trackFixed_) {
            const int track = ScreenX(875.0f);
            pixels_.FillRect(track - 39, ground + 45, 78, 5, Color(126, 116, 84));
            pixels_.FillRect(track - 39, ground + 63, 78, 5, Color(126, 116, 84));
            for (int tx = track - 34; tx < track + 36; tx += 14)
                pixels_.FillRect(tx, ground + 42, 4, 28, Color(100, 81, 58));
        } else {
            const int track = ScreenX(875.0f);
            pixels_.FillRect(track - 27, ground + 43, 54, 9, Color(53, 59, 50));
            pixels_.FillRect(track - 27, ground + 63, 54, 9, Color(53, 59, 50));
        }

        // Gate and generator are world objects; cache markers are separately drawn props.
        const int gate = ScreenX(gateX_);
        pixels_.FillRect(gate - 39, ground - 77, 7, 78, Color(41, 49, 44));
        pixels_.FillRect(gate + 39, ground - 77, 7, 78, Color(41, 49, 44));
        pixels_.FillRect(gate - 39, ground - 78, 85, 6, Color(132, 140, 121));
        for (int x = gate - 27; x <= gate + 27; x += 14)
            pixels_.FillRect(x, ground - 72, 3, 70, Color(89, 103, 91));
        if (barricade_) {
            pixels_.FillRect(gate - 40, ground - 35, 82, 34, Color(111, 93, 66));
            pixels_.FillRect(gate - 43, ground - 39, 88, 6, Color(174, 147, 98));
        }

        const int generator = ScreenX(generatorX_);
        pixels_.FillRect(generator - 27, ground - 73, 54, 73, Color(29, 38, 36));
        pixels_.FillRect(generator - 24, ground - 79, 48, 8, Color(107, 119, 105));
        pixels_.FillRect(generator - 9, ground - 61, 19, 17, generatorOn_ ? Color(234, 181, 91) : Color(155, 88, 65));
        pixels_.FillRect(generator - 4, ground - 57, 9, 7, generatorOn_ ? Color(255, 231, 153) : Color(60, 46, 39));
        if (generatorOn_) {
            const int lamp = ScreenX(generatorX_);
            pixels_.FillRect(lamp - 96, 190, 192, 2, Color(110, 103, 74));
            pixels_.FillRect(lamp - 72, 192, 144, 5, Color(128, 113, 71));
            pixels_.FillRect(lamp - 44, 197, 88, 6, Color(142, 121, 72));
        }
        if (!clueFound_) {
            const int clue = ScreenX(1125.0f);
            pixels_.FillRect(clue - 13, ground - 4, 26, 3, Color(177, 123, 79));
            pixels_.FillRect(clue - 8, ground - 9, 3, 7, Color(194, 142, 88));
            pixels_.FillRect(clue + 5, ground - 9, 3, 7, Color(194, 142, 88));
            pixels_.FillRect(clue - 18, ground - 13, 4, 4, Color(75, 87, 78));
            pixels_.FillRect(clue + 14, ground - 13, 4, 4, Color(75, 87, 78));
        }

        for (const auto& cache : caches_) DrawCache(cache);
        Human(marta_.x, true, marta_.work * 5.0f, std::abs(marta_.x - marta_.target) < 2.0f, 1);
        Human(ilya_.x, false, worldTime_ * 2.0f);
        Human(playerX_, false, static_cast<float>(walkTime_), attackTime_ > 0.0f, facing_);
        for (const auto& enemy : enemies_) DrawEnemy(enemy);

        if (night) {
            pixels_.MultiplyRect(0, 110, 640, 250, 138);
            if (generatorOn_) {
                const int lamp = ScreenX(generatorX_);
                pixels_.AddLightRect(lamp - 44, 142, 88, 28, 16, 12, 3);
                pixels_.AddLightRect(lamp - 80, 170, 160, 32, 12, 9, 2);
                pixels_.AddLightRect(lamp - 112, 202, 224, 39, 9, 7, 2);
                pixels_.AddLightRect(lamp - 138, 241, 276, 34, 5, 4, 1);
            }
        }
        // Repaint the actor silhouettes above the night tint, keeping the scene readable.
        if (night) {
            Human(ilya_.x, false, worldTime_ * 2.0f);
            Human(playerX_, false, static_cast<float>(walkTime_), attackTime_ > 0.0f, facing_);
            for (const auto& enemy : enemies_) DrawEnemy(enemy);
        }
    }

    void DrawHud() {
        pixels_.FillRect(7, 7, 626, 48, Color(18, 27, 29));
        pixels_.FillRect(7, 7, 626, 2, Color(108, 119, 99));
        const std::string phaseLabel = phase_ == Phase::Night ? "НОЧЬ" : "ДЕНЬ 01";
        Text(phaseLabel, 17, 15, 2, Color(224, 207, 164));
        Text("ЛОМ " + std::to_string(scrap_), 160, 15, 2, Color(213, 192, 143));
        Text("ТОПЛ " + std::to_string(fuel_), 260, 15, 2, Color(203, 186, 120));
        Text("АПТ " + std::to_string(medicine_), 370, 15, 2, Color(206, 183, 136));
        Text("ЗД", 438, 15, 2, Color(183, 196, 180));
        pixels_.FillRect(458, 15, 54, 9, Color(53, 65, 60));
        pixels_.FillRect(458, 15, static_cast<int>(52.0f * std::clamp(playerHealth_, 0.0f, 100.0f) / 100.0f), 9,
                         playerHealth_ > 35.0f ? Color(153, 184, 121) : Color(199, 94, 70));
        Text("КПП", 516, 15, 2, Color(183, 196, 180));
        pixels_.FillRect(554, 15, 58, 9, Color(53, 65, 60));
        pixels_.FillRect(554, 15, static_cast<int>(56.0f * std::clamp(gateHealth_, 0.0f, 100.0f) / 100.0f), 9,
                         gateHealth_ > 35.0f ? Color(153, 184, 121) : Color(199, 94, 70));
        Text(std::to_string(static_cast<int>(std::max(0.0f, gateHealth_))) + "%", 615, 15, 1, Color(222, 225, 205));

        std::string objective;
        if (!generatorOn_) {
            if (scrap_ >= 3 && fuel_ >= 2) objective = "ЗАПУСТИТЬ ГЕНЕРАТОР: E";
            else objective = "НАЙТИ ЛОМ И ТОПЛИВО";
        } else if (phase_ == Phase::Day) {
            objective = "К ВОРОТАМ: N НАЧАТЬ НОЧЬ";
        } else {
            objective = "ДО РАССВЕТА " + std::to_string(static_cast<int>(nightLength_ - nightTime_)) + " СЕК";
        }
        Text(objective, 17, 38, 2, Color(182, 194, 181));
        if (phase_ == Phase::Day && generatorOn_ && scrap_ >= 2)
            Text("Q УКРЕПИТЬ ВОРОТА", 17, 62, 2, Color(213, 177, 119));
        if (phase_ == Phase::Night && !barricade_ && scrap_ >= 2)
            Text("Q УКРЕПИТЬ ВОРОТА", 17, 62, 2, Color(213, 177, 119));
        if (phase_ == Phase::Day && generatorOn_)
            Text(assignmentMode_ == 1 ? "1 СБОР   2 РЕМОНТ   3 ОХРАНА" :
                 (assignmentMode_ == 2 ? "1 СБОР   2 РЕМОНТ*  3 ОХРАНА" : "1 СБОР   2 РЕМОНТ   3 ОХРАНА*"),
                 17, 62, 2, Color(190, 184, 153));
    }

    void DrawOverlay() {
        if (phase_ == Phase::Day || phase_ == Phase::Night) return;
        pixels_.FillRect(0, 0, 640, 360, Color(9, 15, 18));
        pixels_.FillRect(75, 84, 490, 190, Color(23, 33, 34));
        pixels_.FillRect(75, 84, 490, 3, Color(176, 151, 103));
        if (phase_ == Phase::Title) {
            Text("ЛИНИЯ ВОЗВРАЩЕНИЯ", 150, 119, 3, Color(228, 207, 163));
            Text("СТАНЦИЯ У ЧЕРНОБЫЛЯ", 180, 160, 2, Color(160, 187, 139));
            Text("A/D ИЛИ СТРЕЛКИ — ИДТИ", 160, 209, 2, Color(216, 221, 204));
            Text("E — ДЕЙСТВИЕ  SPACE — УДАР", 150, 232, 2, Color(216, 221, 204));
            Text("ENTER — НАЧАТЬ", 230, 286, 2, Color(231, 178, 94));
        } else {
            Text(phase_ == Phase::Won ? "СИГНАЛ С ВОСТОКА" : "ВОРОТА ПРОРВАНЫ", 154, 126, 3,
                 phase_ == Phase::Won ? Color(160, 199, 137) : Color(210, 113, 83));
            Text(phase_ == Phase::Won ? (clueFound_ ? "НЕ ВКЛЮЧАЙТЕ ЕГО СНОВА" : "...МЫ ВИДИМ ВАШ СВЕТ") :
                 (playerHealth_ <= 0 ? "ТЫ НЕ ПЕРЕЖИЛ ЭТУ НОЧЬ" : "СТАНЦИЯ ПОТЕРЯЛА ПИТАНИЕ"), 139, 176, 2,
                 Color(216, 221, 204));
            Text("R — НАЧАТЬ ЗАНОВО", 197, 229, 2, Color(231, 178, 94));
        }
    }

    void Draw() {
        DrawPixelScene();
        if (phase_ == Phase::Day || phase_ == Phase::Night) {
            DrawHud();
            if (toastTimer_ > 0.0f && !toast_.empty()) {
                const int width = std::min(620, static_cast<int>(toast_.size()) * 8 + 18);
                pixels_.FillRect((640 - width) / 2, 75, width, 24, Color(18, 27, 29));
                Text(toast_, std::max(10, (640 - static_cast<int>(toast_.size()) * 8) / 2), 83, 2, Color(230, 220, 194));
            }
            if (phase_ == Phase::Day) {
                for (const auto& cache : caches_) {
                    if (!cache.used && std::abs(playerX_ - cache.x) < 42.0f) {
                        Text(cache.medicine ? "E: ВЗЯТЬ АПТЕЧКУ" : (cache.fuel ? "E: ВЗЯТЬ ТОПЛИВО" : "E: ВЗЯТЬ ЛОМ"),
                             218, 318, 2, Color(240, 225, 190));
                        break;
                    }
                }
                if (!generatorOn_ && std::abs(playerX_ - generatorX_) < 52.0f)
                    Text("E: ЗАПУСТИТЬ ГЕНЕРАТОР", 218, 318, 2, Color(240, 225, 190));
                if (!trackFixed_ && std::abs(playerX_ - 875.0f) < 48.0f)
                    Text("E: ПОЧИНИТЬ ПУТЬ (2 ЛОМ)", 200, 318, 2, Color(240, 225, 190));
                if (!clueFound_ && std::abs(playerX_ - 1125.0f) < 48.0f)
                    Text("E: ОСМОТРЕТЬ СЛЕД", 218, 318, 2, Color(240, 225, 190));
            } else if (phase_ == Phase::Night && playerHealth_ < 100.0f && medicine_ > 0) {
                Text("H: ИСПОЛЬЗОВАТЬ АПТЕЧКУ", 200, 318, 2, Color(240, 225, 190));
            }
        }
        DrawOverlay();
        SDL_UpdateTexture(frame_, nullptr, pixels_.Data(), pixels_.PitchBytes());
        SDL_SetRenderDrawColor(renderer_, 14, 20, 22, 255);
        SDL_RenderClear(renderer_);
        SDL_FRect destination{};
        SDL_GetRenderOutputSize(renderer_, &outputWidth_, &outputHeight_);
        const float scale = std::min(static_cast<float>(outputWidth_) / returnline::PixelBuffer::Width,
                                     static_cast<float>(outputHeight_) / returnline::PixelBuffer::Height);
        destination.w = std::floor(returnline::PixelBuffer::Width * scale);
        destination.h = std::floor(returnline::PixelBuffer::Height * scale);
        destination.x = std::floor((outputWidth_ - destination.w) * 0.5f);
        destination.y = std::floor((outputHeight_ - destination.h) * 0.5f);
        SDL_RenderTexture(renderer_, frame_, nullptr, &destination);
        SDL_RenderPresent(renderer_);
    }

    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    SDL_Texture* frame_{};
    SDL_Gamepad* gamepad_{};
    returnline::PixelBuffer pixels_;
    std::chrono::steady_clock::time_point lastTick_{};
    double accumulator_{};
    double worldTime_{};
    double walkTime_{};
    double dayTime_{};
    double nightTime_{};
    float spawnTime_{};
    float playerX_{104.0f};
    float cameraX_{};
    float gateHealth_{100.0f};
    float attackTime_{};
    float playerHealth_{100.0f};
    float playerDamageTimer_{};
    float repairWork_{};
    static constexpr float worldWidth_ = 1240.0f;
    static constexpr float gateX_ = 520.0f;
    static constexpr float generatorX_ = 650.0f;
    static constexpr double nightLength_ = 36.0;
    int scrap_{};
    int fuel_{};
    int medicine_{};
    int assignmentMode_{1};
    Phase phase_{Phase::Title};
    std::vector<Cache> caches_;
    std::vector<Enemy> enemies_;
    Survivor marta_{390.0f, 320.0f, 0.0f, 0.0f};
    Survivor ilya_{450.0f, 450.0f, 0.0f, 0.0f};
    std::string toast_;
    float toastTimer_{};
    int outputWidth_{kWindowWidth};
    int outputHeight_{kWindowHeight};
    int facing_{1};
    bool moving_{};
    bool generatorOn_{};
    bool barricade_{};
    bool clueFound_{};
    bool trackFixed_{};
    bool startPressed_{};
    bool interactPressed_{};
    bool nightPressed_{};
    bool buildPressed_{};
    bool attackPressed_{};
    bool restartPressed_{};
    bool healPressed_{};
    int assignment_{};
    bool running_{true};
};

} // namespace

int main(int argc, char** argv) {
#ifdef __EMSCRIPTEN__
    (void)argc;
    (void)argv;
    static EngineApp app;
    if (!app.Initialize()) return 1;
    return app.Run(false);
#else
    const bool oneFrame = argc > 1 && std::string_view(argv[1]) == "--smoke-frame";
    EngineApp app;
    if (!app.Initialize()) return 1;
    return app.Run(oneFrame);
#endif
}
