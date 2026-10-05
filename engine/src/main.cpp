#include "returnline/PixelBuffer.hpp"

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
struct Cache { float x; bool fuel; int amount; bool used{}; };
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
        window_ = SDL_CreateWindow("Линия возвращения — engine spike",
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
        if (nightPressed_ && generatorOn_ && phase_ == Phase::Day) StartNight();
        if (buildPressed_ && phase_ == Phase::Night && std::abs(playerX_ - gateX_) < 86.0f) {
            if (scrap_ >= 2 && !barricade_) { scrap_ -= 2; barricade_ = true; toast_ = "BARRICADE BUILT"; }
            else if (barricade_) toast_ = "BARRICADE ALREADY BUILT";
            else toast_ = "NEED 2 SCRAP";
        }
        if ((attackPressed_ || keys[SDL_SCANCODE_SPACE]) && phase_ == Phase::Night) Attack();

        if (phase_ == Phase::Night && nightTime_ >= nightLength_) phase_ = Phase::Won;
        if (gateHealth_ <= 0.0f) phase_ = Phase::Lost;
        cameraX_ = std::clamp(playerX_ - 205.0f, 0.0f, worldWidth_ - 640.0f);
        toastTimer_ = std::max(0.0f, toastTimer_ - static_cast<float>(dt));
        if (toastTimer_ <= 0.0f) toast_.clear();
        ClearPressed();
    }

    void ClearPressed() {
        startPressed_ = interactPressed_ = nightPressed_ = buildPressed_ = attackPressed_ = restartPressed_ = false;
    }

    void BeginDay() {
        phase_ = Phase::Day;
        playerX_ = 104.0f;
        toast_ = "FIND SCRAP AND FUEL";
        toastTimer_ = 3.0f;
    }

    void ResetGame() {
        phase_ = Phase::Title;
        playerX_ = 104.0f;
        scrap_ = 0;
        fuel_ = 0;
        gateHealth_ = 100.0f;
        generatorOn_ = false;
        barricade_ = false;
        dayTime_ = nightTime_ = spawnTime_ = 0.0f;
        cameraX_ = 0.0f;
        caches_ = {{270.0f, false, 2}, {770.0f, false, 2}, {960.0f, true, 2}};
        enemies_.clear();
        marta_ = {390.0f, 320.0f, 0.0f, 0.0f};
        ilya_ = {450.0f, 450.0f, 0.0f, 0.0f};
        toast_.clear();
    }

    void UpdateDay(const double dt) {
        dayTime_ += dt;
        MoveSurvivor(marta_, 320.0f, dt);
        MoveSurvivor(ilya_, 450.0f, dt);
        if (std::abs(marta_.x - marta_.target) < 2.0f) {
            marta_.work += static_cast<float>(dt);
            if (marta_.work >= 6.0f) {
                marta_.work -= 6.0f;
                ++scrap_;
                toast_ = "MARTA FOUND 1 SCRAP";
                toastTimer_ = 2.0f;
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
                gateHealth_ -= barricade_ ? 3.0f : 7.0f;
                toast_ = barricade_ ? "BARRICADE TAKES THE HIT" : "THE GATE IS HIT";
                toastTimer_ = 1.4f;
            }
        }
        enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [](const Enemy& e) {
            return e.health <= 0;
        }), enemies_.end());
    }

    void Interact() {
        if (phase_ != Phase::Day) return;
        for (auto& cache : caches_) {
            if (!cache.used && std::abs(playerX_ - cache.x) < 42.0f) {
                cache.used = true;
                if (cache.fuel) fuel_ += cache.amount;
                else scrap_ += cache.amount;
                toast_ = cache.fuel ? "FUEL +2" : "SCRAP +2";
                toastTimer_ = 1.8f;
                return;
            }
        }
        if (!generatorOn_ && std::abs(playerX_ - generatorX_) < 52.0f) {
            if (scrap_ >= 3 && fuel_ >= 2) {
                scrap_ -= 3;
                fuel_ -= 2;
                generatorOn_ = true;
                toast_ = "GENERATOR ONLINE - REACH THE GATE";
                toastTimer_ = 3.0f;
            } else {
                toast_ = "GENERATOR NEEDS 3 SCRAP AND 2 FUEL";
                toastTimer_ = 2.5f;
            }
            return;
        }
        toast_ = "MOVE CLOSER TO A CACHE OR GENERATOR";
        toastTimer_ = 1.8f;
    }

    void StartNight() {
        if (std::abs(playerX_ - gateX_) > 92.0f) {
            toast_ = "REACH THE GATE TO START THE NIGHT";
            toastTimer_ = 2.0f;
            return;
        }
        phase_ = Phase::Night;
        nightTime_ = spawnTime_ = 0.0f;
        toast_ = "HOLD THE GATE UNTIL DAWN";
        toastTimer_ = 2.4f;
    }

    void Attack() {
        if (attackTime_ > 0.0f) return;
        attackTime_ = 0.34f;
        for (auto& enemy : enemies_) {
            if (std::abs(enemy.x - playerX_) < 66.0f) {
                --enemy.health;
                toast_ = enemy.health > 0 ? "HIT - ONE MORE" : "MUTANT DOWN";
                toastTimer_ = 1.0f;
                return;
            }
        }
        toast_ = "NO ENEMY IN REACH";
        toastTimer_ = 1.0f;
    }

    std::array<int, 5> Glyph(const char raw) const {
        const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
        switch (c) {
        case 'A': return {2,5,7,5,5}; case 'B': return {6,5,6,5,6};
        case 'C': return {3,4,4,4,3}; case 'D': return {6,5,5,5,6};
        case 'E': return {7,4,6,4,7}; case 'F': return {7,4,6,4,4};
        case 'G': return {3,4,5,5,3}; case 'H': return {5,5,7,5,5};
        case 'I': return {7,2,2,2,7}; case 'J': return {1,1,1,5,2};
        case 'K': return {5,5,6,5,5}; case 'L': return {4,4,4,4,7};
        case 'M': return {5,7,7,5,5}; case 'N': return {5,7,7,7,5};
        case 'O': return {2,5,5,5,2}; case 'P': return {6,5,6,4,4};
        case 'Q': return {2,5,5,7,3}; case 'R': return {6,5,6,5,5};
        case 'S': return {3,4,2,1,6}; case 'T': return {7,2,2,2,2};
        case 'U': return {5,5,5,5,7}; case 'V': return {5,5,5,5,2};
        case 'W': return {5,5,7,7,5}; case 'X': return {5,5,2,5,5};
        case 'Y': return {5,5,2,2,2}; case 'Z': return {7,1,2,4,7};
        case '0': return {7,5,5,5,7}; case '1': return {2,6,2,2,7};
        case '2': return {6,1,2,4,7}; case '3': return {6,1,2,1,6};
        case '4': return {5,5,7,1,1}; case '5': return {7,4,6,1,6};
        case '6': return {3,4,6,5,2}; case '7': return {7,1,2,2,2};
        case '8': return {2,5,2,5,2}; case '9': return {2,5,3,1,6};
        case ':': return {0,2,0,2,0}; case '.': return {0,0,0,0,2};
        case '-': return {0,0,7,0,0}; case '+': return {0,2,7,2,0};
        case '/': return {1,1,2,4,4}; case '!': return {2,2,2,0,2};
        case '%': return {5,1,2,4,5}; case '>': return {4,2,1,2,4};
        default: return {0,0,0,0,0};
        }
    }

    void Text(std::string_view value, int x, const int y, const int scale,
              const std::uint32_t color) {
        for (const char c : value) {
            if (c == ' ') { x += scale * 4; continue; }
            const auto rows = Glyph(c);
            for (int row = 0; row < 5; ++row)
                for (int column = 0; column < 3; ++column)
                    if ((rows[row] & (1 << (2 - column))) != 0)
                        pixels_.FillRect(x + column * scale, y + row * scale, scale, scale, color);
            x += scale * 4;
        }
    }

    int ScreenX(const float worldX) const { return static_cast<int>(std::round(worldX - cameraX_)); }

    void Human(const float worldX, const bool scavenger, const float animation,
               const bool working = false, const int facing = 1) {
        constexpr int ground = 275;
        const int x = ScreenX(worldX);
        if (x < -32 || x > 672) return;
        const int step = working ? 0 : static_cast<int>(std::sin(animation) * 3.0f);
        pixels_.FillRect(x - 12, ground - 2, 24, 4, Color(29, 34, 31));
        pixels_.FillRect(x - 8, ground - 20 + step, 6, 20, Color(40, 48, 44));
        pixels_.FillRect(x + 2, ground - 20 - step, 6, 20, Color(35, 43, 40));
        pixels_.FillRect(x - 11, ground - 49, 22, 30,
                         scavenger ? Color(101, 108, 79) : Color(70, 94, 88));
        pixels_.FillRect(x - 8, ground - 64, 16, 16, Color(181, 153, 118));
        pixels_.FillRect(x - 10, ground - 68, 20, 7, Color(55, 47, 38));
        const int armY = working ? ground - 26 - static_cast<int>((std::sin(animation * 5.0f) + 1) * 5) : ground - 43;
        pixels_.FillRect(x + facing * 8, armY, 5, 13, Color(181, 153, 118));
        if (working) {
            pixels_.FillRect(x + 14, ground - 19, 3, 13, Color(143, 121, 82));
            pixels_.FillRect(x + 11, ground - 20, 9, 3, Color(189, 164, 111));
        }
    }

    void DrawCache(const Cache& cache) {
        if (cache.used) return;
        const int x = ScreenX(cache.x);
        constexpr int ground = 275;
        if (x < -40 || x > 680) return;
        if (!cache.fuel) {
            pixels_.FillRect(x - 20, ground - 24, 40, 21, Color(65, 76, 69));
            pixels_.FillRect(x - 18, ground - 31, 25, 8, Color(157, 139, 99));
            pixels_.FillRect(x + 2, ground - 35, 17, 10, Color(111, 101, 76));
            pixels_.FillRect(x - 5, ground - 37, 8, 4, Color(195, 177, 130));
        } else {
            pixels_.FillRect(x - 13, ground - 33, 12, 32, Color(129, 77, 44));
            pixels_.FillRect(x + 3, ground - 31, 12, 30, Color(115, 69, 41));
            pixels_.FillRect(x - 9, ground - 27, 4, 18, Color(195, 143, 66));
            pixels_.FillRect(x + 7, ground - 26, 4, 17, Color(187, 129, 58));
        }
    }

    void DrawEnemy(const Enemy& enemy) {
        constexpr int ground = 275;
        const int x = ScreenX(enemy.x);
        if (x < -36 || x > 676) return;
        const int step = static_cast<int>(std::sin(enemy.animation) * 3.0f);
        pixels_.FillRect(x - 18, ground - 2, 36, 4, Color(29, 31, 28));
        pixels_.FillRect(x - 13, ground - 17 + step, 9, 17, Color(48, 42, 36));
        pixels_.FillRect(x + 4, ground - 17 - step, 9, 17, Color(48, 42, 36));
        pixels_.FillRect(x - 18, ground - 39, 34, 24, Color(75, 63, 50));
        pixels_.FillRect(x - 20, ground - 49, 26, 17, Color(82, 69, 54));
        pixels_.FillRect(x - 18, ground - 55, 7, 12, Color(57, 48, 40));
        pixels_.FillRect(x - 4, ground - 43, 5, 4, Color(223, 150, 72));
        pixels_.FillRect(x + 12, ground - 31, 11, 5, Color(82, 69, 54));
    }

    void DrawPixelScene() {
        constexpr int ground = 275;
        const bool night = phase_ == Phase::Night || phase_ == Phase::Lost;
        pixels_.Clear(night ? Color(22, 32, 39) : Color(42, 55, 59));
        pixels_.FillRect(0, 0, 640, 110, night ? Color(21, 31, 40) : Color(48, 63, 68));

        // Distant broken depot skyline and power masts.
        pixels_.FillRect(22 - static_cast<int>(cameraX_ * .15f), 157, 190, 112, Color(36, 45, 43));
        pixels_.FillRect(13 - static_cast<int>(cameraX_ * .15f), 148, 208, 10, Color(88, 91, 75));
        pixels_.FillRect(55 - static_cast<int>(cameraX_ * .15f), 176, 34, 26, Color(19, 28, 29));
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
        const int firstTile = static_cast<int>(cameraX_) / 32 * 32;
        for (int wx = firstTile; wx < firstTile + 704; wx += 32) {
            const int x = wx - static_cast<int>(cameraX_);
            pixels_.FillRect(x, ground, 32, 85, Color(53, 59, 50));
            pixels_.FillRect(x, ground, 32, 5, Color(120, 114, 87));
            pixels_.FillRect(x + 3, ground + 15, 9, 4, Color(72, 77, 61));
            pixels_.FillRect(x + 20, ground + 32, 7, 4, Color(42, 49, 43));
        }
        pixels_.FillRect(0, ground + 48, 640, 4, Color(34, 39, 37));
        pixels_.FillRect(0, ground + 67, 640, 4, Color(34, 39, 37));
        for (int wx = firstTile; wx < firstTile + 704; wx += 36)
            pixels_.FillRect(wx - static_cast<int>(cameraX_), ground + 43, 5, 34, Color(94, 84, 62));

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

        for (const auto& cache : caches_) DrawCache(cache);
        Human(marta_.x, true, marta_.work * 5.0f, std::abs(marta_.x - marta_.target) < 2.0f, 1);
        Human(ilya_.x, false, worldTime_ * 2.0f);
        Human(playerX_, false, static_cast<float>(walkTime_), attackTime_ > 0.0f, facing_);
        for (const auto& enemy : enemies_) DrawEnemy(enemy);

        if (night) pixels_.FillRect(0, 110, 640, 165, Color(8, 12, 20));
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
        const std::string phaseLabel = phase_ == Phase::Night ? "NIGHT" : "DAY 01";
        Text(phaseLabel, 17, 15, 2, Color(224, 207, 164));
        Text("SCRAP " + std::to_string(scrap_), 160, 15, 2, Color(213, 192, 143));
        Text("FUEL " + std::to_string(fuel_), 290, 15, 2, Color(203, 186, 120));
        Text("GATE", 430, 15, 2, Color(183, 196, 180));
        pixels_.FillRect(472, 15, 90, 9, Color(53, 65, 60));
        pixels_.FillRect(472, 15, static_cast<int>(88.0f * std::clamp(gateHealth_, 0.0f, 100.0f) / 100.0f), 9,
                         gateHealth_ > 35.0f ? Color(153, 184, 121) : Color(199, 94, 70));
        Text(std::to_string(static_cast<int>(std::max(0.0f, gateHealth_))) + "%", 570, 15, 2, Color(222, 225, 205));

        std::string objective;
        if (!generatorOn_) {
            if (scrap_ >= 3 && fuel_ >= 2) objective = "REPAIR GENERATOR - PRESS E";
            else objective = "FIND 3 SCRAP AND 2 FUEL";
        } else if (phase_ == Phase::Day) {
            objective = "REACH THE GATE - PRESS N TO START NIGHT";
        } else {
            objective = "HOLD THE GATE " + std::to_string(static_cast<int>(nightLength_ - nightTime_)) + " SEC";
        }
        Text(objective, 17, 38, 2, Color(182, 194, 181));
        if (phase_ == Phase::Day && generatorOn_ && scrap_ >= 2)
            Text("Q BUILD BARRICADE AT GATE", 17, 62, 2, Color(213, 177, 119));
        if (phase_ == Phase::Night && !barricade_ && scrap_ >= 2)
            Text("Q BUILD BARRICADE", 17, 62, 2, Color(213, 177, 119));
    }

    void DrawOverlay() {
        if (phase_ == Phase::Day || phase_ == Phase::Night) return;
        pixels_.FillRect(0, 0, 640, 360, Color(9, 15, 18));
        pixels_.FillRect(75, 84, 490, 190, Color(23, 33, 34));
        pixels_.FillRect(75, 84, 490, 3, Color(176, 151, 103));
        if (phase_ == Phase::Title) {
            Text("LAST STATION", 192, 119, 4, Color(228, 207, 163));
            Text("REPAIR THE LINE", 204, 160, 2, Color(160, 187, 139));
            Text("MOVE  A D  /  ARROWS", 160, 209, 2, Color(216, 221, 204));
            Text("ACTION E   ATTACK SPACE", 151, 232, 2, Color(216, 221, 204));
            Text("PRESS ENTER", 250, 286, 2, Color(231, 178, 94));
        } else {
            Text(phase_ == Phase::Won ? "STATION RESTORED" : "GATE OVERRUN", 154, 126, 3,
                 phase_ == Phase::Won ? Color(160, 199, 137) : Color(210, 113, 83));
            Text(phase_ == Phase::Won ? "DAWN REACHES THE OLD LINE" : "THE STATION LOST POWER", 139, 176, 2,
                 Color(216, 221, 204));
            Text("PRESS R TO PLAY AGAIN", 197, 229, 2, Color(231, 178, 94));
        }
    }

    void Draw() {
        DrawPixelScene();
        if (phase_ == Phase::Day || phase_ == Phase::Night) {
            DrawHud();
            if (toastTimer_ > 0.0f && !toast_.empty()) {
                const int width = static_cast<int>(toast_.size()) * 8 + 18;
                pixels_.FillRect((640 - width) / 2, 75, width, 24, Color(18, 27, 29));
                Text(toast_, (640 - static_cast<int>(toast_.size()) * 8) / 2, 83, 2, Color(230, 220, 194));
            }
            if (phase_ == Phase::Day) {
                for (const auto& cache : caches_) {
                    if (!cache.used && std::abs(playerX_ - cache.x) < 42.0f) {
                        Text(cache.fuel ? "E PICK UP FUEL" : "E PICK UP SCRAP", 218, 318, 2, Color(240, 225, 190));
                        break;
                    }
                }
                if (!generatorOn_ && std::abs(playerX_ - generatorX_) < 52.0f)
                    Text("E REPAIR GENERATOR", 218, 318, 2, Color(240, 225, 190));
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
    static constexpr float worldWidth_ = 1240.0f;
    static constexpr float gateX_ = 520.0f;
    static constexpr float generatorX_ = 650.0f;
    static constexpr double nightLength_ = 36.0;
    int scrap_{};
    int fuel_{};
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
    bool startPressed_{};
    bool interactPressed_{};
    bool nightPressed_{};
    bool buildPressed_{};
    bool attackPressed_{};
    bool restartPressed_{};
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
