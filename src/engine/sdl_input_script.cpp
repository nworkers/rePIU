#include "sdl_input_script.h"

#include "repiu/input/host_key_names.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace repiu::engine
{

std::vector<SdlInputScriptStep> ParseSdlInputScript(const std::string& text,
                                                    std::uint32_t* skipped)
{
    std::vector<SdlInputScriptStep> steps;
    std::uint32_t skipped_lines = 0U;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        std::istringstream fields(line);
        std::string first;
        if (!(fields >> first) || first[0] == '#')
        {
            continue;
        }
        SdlInputScriptStep step;
        char* end = nullptr;
        const unsigned long at = std::strtoul(first.c_str(), &end, 10);
        std::string name;
        if (end == first.c_str() || *end != '\0' || !(fields >> name) ||
            !repiu::input::FindHostKeyByName(name, &step.key))
        {
            ++skipped_lines;
            continue;
        }
        step.at_ms = static_cast<std::uint32_t>(at);
        step.name = name;
        unsigned long hold = 0UL;
        if (fields >> hold)
        {
            step.hold_ms = static_cast<std::uint32_t>(hold);
        }
        steps.push_back(step);
    }
    if (skipped != nullptr)
    {
        *skipped = skipped_lines;
    }
    return steps;
}

SdlInputScriptPlayer::~SdlInputScriptPlayer()
{
    Stop();
}

bool SdlInputScriptPlayer::StartFromEnvironment(const SDL_WindowID window)
{
    const char* const path = std::getenv("REPIU_INPUT_SCRIPT");
    if (path == nullptr || *path == '\0' || thread_.joinable())
    {
        return false;
    }
    std::ifstream file(path);
    if (!file)
    {
        std::fprintf(stderr, "[repiu-input-script] cannot read %s\n", path);
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::uint32_t skipped = 0U;
    steps_ = ParseSdlInputScript(buffer.str(), &skipped);
    std::fprintf(stderr,
                 "[repiu-input-script] %s: %zu steps, %u lines skipped\n",
                 path, steps_.size(), static_cast<unsigned>(skipped));
    if (steps_.empty())
    {
        return false;
    }
    window_ = window;
    stop_.store(false, std::memory_order_relaxed);
    thread_ = std::thread([this]() { Run(); });
    return true;
}

void SdlInputScriptPlayer::Stop()
{
    stop_.store(true, std::memory_order_relaxed);
    if (thread_.joinable())
    {
        thread_.join();
    }
}

void SdlInputScriptPlayer::Run()
{
    const auto origin = std::chrono::steady_clock::now();
    const auto push = [this](const SdlInputScriptStep& step, const bool down) {
        SDL_Event event{};
        event.key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.timestamp = SDL_GetTicksNS();
        event.key.windowID = window_;
        event.key.which = 0U;
        event.key.scancode = SDL_GetScancodeFromKey(step.key, nullptr);
        event.key.key = step.key;
        event.key.mod = SDL_KMOD_NONE;
        event.key.raw = 0U;
        event.key.down = down;
        event.key.repeat = false;
        if (SDL_PushEvent(&event))
        {
            pushed_.fetch_add(1U, std::memory_order_relaxed);
        }
    };
    const auto wait_until = [this](std::chrono::steady_clock::time_point when) {
        while (!stop_.load(std::memory_order_relaxed))
        {
            const auto now = std::chrono::steady_clock::now();
            if (now >= when)
            {
                return true;
            }
            const auto remaining = when - now;
            std::this_thread::sleep_for(
                remaining > std::chrono::milliseconds(20)
                    ? std::chrono::milliseconds(20)
                    : std::chrono::duration_cast<std::chrono::milliseconds>(
                          remaining) + std::chrono::milliseconds(1));
        }
        return false;
    };
    for (const SdlInputScriptStep& step : steps_)
    {
        if (!wait_until(origin + std::chrono::milliseconds(step.at_ms)))
        {
            return;
        }
        push(step, true);
        std::fprintf(stderr, "[repiu-input-script] t=%u key=%s hold=%u\n",
                     static_cast<unsigned>(step.at_ms), step.name.c_str(),
                     static_cast<unsigned>(step.hold_ms));
        if (!wait_until(std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(step.hold_ms)))
        {
            push(step, false);
            return;
        }
        push(step, false);
    }
}

}  // namespace repiu::engine
