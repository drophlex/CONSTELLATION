// CONSTELLATION — shared in-process registry.
// Every instance of the plugin loaded by the host (same binary / same format)
// shares this singleton. Stars publish their state and spectrum here; any open
// window can read every star and send it commands. No sends/returns needed.
#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <array>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>

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
    std::atomic<int>   recArmed { 0 };    // SKY surround recorder armed: stars render the surround bed into the bus
    std::atomic<int>   recFormat { 2 };   // 0 = 5.1, 1 = 7.1, 2 = 7.1.4
};

// Surround export bus. Ableton tracks are stereo only, so each star also renders the chosen
// speaker bed (+ a binaural pair) here, indexed by timeline sample. The SKY on the Master runs
// after every track in the same audio cycle, reads the same positions, clears them and writes a WAV.
constexpr int kRecMaxCh = 14;              // up to 12 bed channels + 2 binaural
constexpr int kBusFrames = 1 << 16;
struct SurroundBus
{
    std::vector<std::atomic<float>> data;  // [ch * kBusFrames + frame]
    SurroundBus() : data ((size_t) (kRecMaxCh * kBusFrames)) { clear(); }
    void clear() { for (auto& d : data) d.store (0.0f, std::memory_order_relaxed); }
    static int frame (juce::int64 pos) noexcept { return (int) (((pos % kBusFrames) + kBusFrames) % kBusFrames); }
    inline void add (int ch, int fr, float v) noexcept
    {
        if (v == 0.0f) return;
        auto& a = data[(size_t) (ch * kBusFrames + fr)];
        float old = a.load (std::memory_order_relaxed);
        while (! a.compare_exchange_weak (old, old + v, std::memory_order_relaxed)) {}
    }
    inline float take (int ch, int fr) noexcept { return data[(size_t) (ch * kBusFrames + fr)].exchange (0.0f, std::memory_order_relaxed); }
};

inline const char* recFormatName (int f) { return f == 0 ? "5.1" : (f == 1 ? "7.1" : "7.1.4"); }
// WAV-canonical channel order (WAVE_FORMAT_EXTENSIBLE mask): L R C LFE Lb Rb (Ls Rs) (TFL TFR TBL TBR)
inline juce::AudioChannelSet recFormatSet (int f)
{
    using CS = juce::AudioChannelSet;
    if (f == 0) return CS::create5point1();
    juce::Array<CS::ChannelType> t { CS::left, CS::right, CS::centre, CS::LFE, CS::leftSurround, CS::rightSurround,
                                     CS::leftSurroundSide, CS::rightSurroundSide };
    if (f == 2) t.addArray (juce::Array<CS::ChannelType> { CS::topFrontLeft, CS::topFrontRight, CS::topRearLeft, CS::topRearRight });
    return CS::channelSetWithChannels (t);
}

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
    SurroundBus bus;

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
