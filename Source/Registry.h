// CONSTELLATION — shared in-process registry.
// Every instance of the plugin loaded by the host (same binary / same format)
// shares this singleton. Stars publish their state and spectrum here; any open
// window can read every star and send it commands. No sends/returns needed.
#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <array>

namespace cst
{
constexpr int kMaxStars = 64;
constexpr int kBands    = 10;
inline constexpr float kBandHz[kBands] = { 50, 100, 200, 400, 800, 1600, 3150, 6300, 10000, 15000 };

enum Param : int
{
    pX = 0, pY, pZ, pWidth, pGain, pTone, pLowCut, pSpace, pPriority, pUnmask, pMute, pSolo, pRole,
    kNumStarParams
};

struct AF
{
    std::atomic<float> v { 0.0f };
    float get() const noexcept      { return v.load (std::memory_order_relaxed); }
    void  set (float x) noexcept    { v.store (x, std::memory_order_relaxed); }
};

struct Command { int param; float value; };

struct StarSlot
{
    std::atomic<int>      active { 0 };          // 1 = owned by a live instance
    std::atomic<uint32_t> heartbeat { 0 };       // ms counter of last processed block
    std::atomic<int>      isSky { 0 };
    void*                 owner = nullptr;       // message-thread only

    std::array<AF, kNumStarParams> val;          // published values (real units)
    std::array<AF, kBands> bandDb;               // pre-unmask band energy (dB)
    std::array<AF, kBands> cutDb;                // current unmask reduction (dB, <= 0)
    AF levelDb, peakDb;

    // message-thread only data
    juce::String name { "Track" };
    juce::String uuid;
    uint32_t     colour = 0xff58e1ff;
    juce::SpinLock cmdLock;
    std::array<Command, 64> cmds;
    int numCmds = 0;

    void reset()
    {
        for (auto& b : bandDb) b.set (-100.0f);
        for (auto& c : cutDb)  c.set (0.0f);
        levelDb.set (-100.0f); peakDb.set (-100.0f);
        numCmds = 0;
    }

    bool fresh (uint32_t nowMs) const noexcept
    {
        return active.load() == 1 && (nowMs - heartbeat.load()) < 1500u;
    }

    void push (int param, float value)
    {
        const juce::SpinLock::ScopedLockType sl (cmdLock);
        for (int i = 0; i < numCmds; ++i)
            if (cmds[(size_t) i].param == param) { cmds[(size_t) i].value = value; return; }
        if (numCmds < (int) cmds.size()) cmds[(size_t) numCmds++] = { param, value };
    }
};

enum RenderMode { renderSpeakers = 0, renderBinaural = 1 };

struct Globals
{
    std::atomic<int>   room { 1 };        // index in room table
    AF                 wrap;              // 0 = front stage, 1 = full 360° around listener
    std::atomic<int>   binaural { 0 };    // 1 = headphone 3D render on stereo buses
    std::atomic<int>   lang { 0 };        // 0 EN, 1 ES
    std::atomic<int>   monoCheck { 0 };
    std::atomic<int>   version { 0 };     // bumps when a UI changes globals
};

class Registry
{
public:
    static Registry& get() { static Registry r; return r; }

    int claim (void* owner, bool sky)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        for (int i = 0; i < kMaxStars; ++i)
        {
            if (slots[(size_t) i].active.load() == 0)
            {
                auto& s = slots[(size_t) i];
                s.reset();
                s.owner = owner;
                s.isSky.store (sky ? 1 : 0);
                s.heartbeat.store (juce::Time::getMillisecondCounter());
                s.active.store (1);
                return i;
            }
        }
        return -1;
    }

    void release (int index)
    {
        if (index < 0) return;
        const juce::SpinLock::ScopedLockType sl (lock);
        slots[(size_t) index].active.store (0);
        slots[(size_t) index].owner = nullptr;
    }

    StarSlot& slot (int i) noexcept { return slots[(size_t) i]; }
    Globals globals;

private:
    Registry()
    {
        globals.wrap.set (0.0f);
        for (auto& s : slots) s.reset();
    }
    juce::SpinLock lock;
    std::array<StarSlot, kMaxStars> slots;
};
} // namespace cst
