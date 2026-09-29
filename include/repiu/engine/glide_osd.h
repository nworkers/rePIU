#ifndef REPIU_ENGINE_GLIDE_OSD_H_
#define REPIU_ENGINE_GLIDE_OSD_H_

#include <atomic>
#include <string>

namespace repiu::engine
{

// Task 761. The in-game on-screen display the launcher section of
// ARCHITECTURE.md promised: the same Dear ImGui layer, drawn inside the Glide
// backend's SDL window while the guest runs. It currently holds one control,
// the LFB high-precision presentation toggle.
//
// Threading: every method runs on the backend's host thread, the one that owns
// the SDL window and the GL context. The toggle the checkbox flips is an
// atomic the guest thread reads, which is why Render takes it as such.
//
// SDL and ImGui types stay out of this header the way the backend keeps them
// out of its own: the window, context, and event parameters are opaque
// pointers the implementation casts back.
class GlideOsd
{
public:
    GlideOsd();
    ~GlideOsd();

    GlideOsd(const GlideOsd&) = delete;
    GlideOsd& operator=(const GlideOsd&) = delete;

    // Creates the ImGui context against an SDL window and its current GL
    // context. False leaves the OSD inert and `message` says why.
    bool Initialize(void* sdl_window, void* gl_context, std::string* message);
    void Shutdown();
    bool initialized() const { return initialized_; }

    bool visible() const { return visible_; }
    void ToggleVisible() { visible_ = !visible_; }

    // Forwards one SDL_Event to ImGui. Called for every event while the OSD is
    // visible so the checkbox can be clicked; game input keeps flowing
    // regardless, because the cabinet's controls must never go dead.
    void ProcessEvent(const void* sdl_event);

    // Draws the overlay when visible. Call on the host thread with the game's
    // GL context current, immediately before the buffer swap; the ImGui GL3
    // backend saves and restores the GL state it touches.
    void Render(std::atomic<bool>* lfb_high_precision);

private:
    bool initialized_ = false;
    bool visible_ = false;
};

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_GLIDE_OSD_H_
