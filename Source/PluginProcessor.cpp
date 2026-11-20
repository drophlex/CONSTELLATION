// CONSTELLATION — by Polaris Sound Lab (Mérida, Yucatán). Design: Dark Funk Durcal. GPL-3.0
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "LicenceKey.h"
#include <juce_cryptography/juce_cryptography.h>

using namespace cst;

namespace
{
const char* kIds[kNumStarParams] = { "x", "y", "z", "width", "gain", "tone", "lowcut", "space", "priority", "unmask", "mute", "solo", "role" };

void direction (float x, float y, float z, float wrap, float& az, float& el, float& dist)
{
    const float fwd  = 1.0f - 2.0f * z * wrap;
    const float side = x * (1.0f + 0.8f * wrap);
    float a = (std::abs (side) < 1.0e-4f && std::abs (fwd) < 1.0e-4f) ? 0.0f : std::atan2 (side, fwd);
    a *= 180.0f / kPi;
    az   = juce::jlimit (-180.0f, 180.0f, a * (1.333f - 0.333f * wrap));
    el   = y >= 0.0f ? y * 60.0f : y * 30.0f;
    dist = z * (1.0f - 0.6f * wrap);
}
void computeRevGains (const SpeakerLayout& lay, std::vector<float>& rg)
{
    const int nc = lay.numChannels;
    rg.assign ((size_t) (2 * nc), 0.0f);
    for (auto& sp : lay.ear) { const int ch = sp.channel; rg[(size_t) (ch * 2)] = sp.az < 0 ? 1.f : (sp.az == 0 ? .5f : 0.f); rg[(size_t) (ch * 2 + 1)] = sp.az > 0 ? 1.f : (sp.az == 0 ? .5f : 0.f); }
    for (auto& sp : lay.top) { const int ch = sp.channel; rg[(size_t) (ch * 2)] = sp.az <= 0 ? .8f : 0.f; rg[(size_t) (ch * 2 + 1)] = sp.az >= 0 ? .8f : 0.f; }
    float pl = 0, pr = 0; for (int c = 0; c < nc; ++c) { pl += rg[(size_t) (c * 2)] * rg[(size_t) (c * 2)]; pr += rg[(size_t) (c * 2 + 1)] * rg[(size_t) (c * 2 + 1)]; }
    for (int c = 0; c < nc; ++c) { rg[(size_t) (c * 2)] /= std::sqrt (std::max (pl, 1e-6f)); rg[(size_t) (c * 2 + 1)] /= std::sqrt (std::max (pr, 1e-6f)); }
}
} // namespace

juce::StringArray ConstellationProcessor::starParamIds()
{
    juce::StringArray s; for (auto* id : kIds) s.add (id); return s;
}

juce::AudioProcessorValueTreeState::ParameterLayout ConstellationProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> v;
    auto fl = [&] (const char* id, const char* name, float lo, float hi, float def, const char* label = "")
    {
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.0001f), def,
                                                             AudioParameterFloatAttributes().withLabel (label)));
    };
    fl ("x", "Pan (X)", -1.0f, 1.0f, 0.0f);
    fl ("y", "Height (Y)", -1.0f, 1.0f, 0.0f);
    fl ("z", "Depth (Z)", 0.0f, 1.0f, 0.25f);
    fl ("width", "Width", 0.0f, 2.0f, 1.0f);
    fl ("gain", "Gain", -36.0f, 12.0f, 0.0f, "dB");
    fl ("tone", "Tone (dark/bright)", -1.0f, 1.0f, 0.0f);
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "lowcut", 1 }, "Low Cut",
                                                        NormalisableRange<float> (20.0f, 800.0f, 0.1f, 0.35f), 20.0f,
                                                        AudioParameterFloatAttributes().withLabel ("Hz")));
    fl ("space", "Space", 0.0f, 1.0f, 0.25f);
    v.push_back (std::make_unique<AudioParameterInt> (ParameterID { "priority", 1 }, "Priority", 1, 10, 5));
    fl ("unmask", "Unmask", 0.0f, 1.0f, 0.5f);
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "mute", 1 }, "Mute", false));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "solo", 1 }, "Solo", false));
    StringArray roles; for (int i = 0; i < kNumRoles; ++i) roles.add (roleNameEn[i]);
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "role", 1 }, "Role", roles, 0));
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Star", "Sky" }, 0,
                                                         AudioParameterChoiceAttributes().withAutomatable (false)));
    fl ("morph", "Scene Morph", 0.0f, 1.0f, 0.0f);
    return { v.begin(), v.end() };
}

ConstellationProcessor::ConstellationProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CONSTELLATION", createLayout())
{
    for (int i = 0; i < kNumStarParams; ++i) p[(size_t) i] = apvts.getRawParameterValue (kIds[i]);
    modeParam  = apvts.getRawParameterValue ("mode");
    morphParam = apvts.getRawParameterValue ("morph");
    lastMorph  = morphParam->load();

    uuid = juce::Uuid().toString();
    slotIndex = Registry::get().claim (this, false);
    if (slotIndex >= 0) { auto& s = Registry::get().slot (slotIndex); s.uuid = uuid; s.name = trackName; }
    publishParams();
    startTimerHz (30);
}

ConstellationProcessor::~ConstellationProcessor()
{
    stopTimer();
    if (recState.load() != recIdle) stopRecorder();
    if (writerThread != nullptr) writerThread->stopThread (2000);
    Registry::get().release (slotIndex);
}

void ConstellationProcessor::setSkyMode (bool sky)
{
    if (auto* prm = apvts.getParameter ("mode")) prm->setValueNotifyingHost (sky ? 1.0f : 0.0f);
    if (slotIndex >= 0) Registry::get().slot (slotIndex).isSky.store (sky ? 1 : 0);
}

bool ConstellationProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    using CS = juce::AudioChannelSet;
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != CS::mono() && in != CS::stereo()) return false;
    for (auto& s : { CS::stereo(), CS::quadraphonic(), CS::create5point0(), CS::create5point1(), CS::create7point0(), CS::create7point1(),
                     CS::create5point1point2(), CS::create5point1point4(), CS::create7point1point2(), CS::create7point1point4() })
        if (out == s) return true;
    return false;
}

void ConstellationProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; maxBlock = juce::jmax (1, samplesPerBlock);
    scratch.setSize (2, maxBlock * 2);
    reverb.prepare (sr); reverb.clear(); lastRoom = -1;
    for (auto& b : bin) { b.prepare (sr); b.clear(); }
    for (int b = 0; b < kBands; ++b)
    {
        det[b].bandpass (sr, kBandHz[b], 1.4f); det[b].clear(); detEnv[b] = 0;
        cutDb[b] = cutApplied[b] = 0;
        for (int c = 0; c < 2; ++c) { cut[c][b].peak (sr, kBandHz[b], 1.2f, 0.0f); cut[c][b].clear(); }
    }
    for (int c = 0; c < 2; ++c) { hpf[c].clear(); hpf2[c].clear(); lowSh[c].clear(); highSh[c].clear(); hPeak[c].clear(); hShelf[c].clear(); air[c].clear(); }
    for (auto* s : { &gainSm, &muteSm, &widthSm, &directSm, &sendSm }) s->reset (sr, 0.03);
    gainSm.setCurrentAndTargetValue (dbToGain (p[pGain]->load()));
    muteSm.setCurrentAndTargetValue (1.0f);
    widthSm.setCurrentAndTargetValue (p[pWidth]->load());
    directSm.setCurrentAndTargetValue (1.0f);
    sendSm.setCurrentAndTargetValue (0.0f);
    layoutChannels = 0;
    recScratch.setSize (kRecMaxCh, maxBlock * 2);
    for (auto& b : recBin) { b.prepare (sr); b.clear(); }
    recLayoutFmt = -1;
    updateFilters (true);
}

void ConstellationProcessor::updateFilters (bool force)
{
    auto changed = [&] (int i) { const float v = p[(size_t) i]->load(); const bool c = force || std::abs (v - lastVals[i]) > 1.0e-4f; lastVals[i] = v; return c; };
    const bool cTone = changed (pTone), cY = changed (pY), cLow = changed (pLowCut), cZ = changed (pZ);
    const float wrap = Registry::get().globals.wrap.get();
    const bool cWrap = force || std::abs (wrap - lastWrap) > 1.0e-4f; lastWrap = wrap;
    for (int c = 0; c < 2; ++c)
    {
        if (cTone) { lowSh[c].lowShelf (sr, 250.0f, -p[pTone]->load() * 6.0f); highSh[c].highShelf (sr, 4000.0f, p[pTone]->load() * 6.0f); }
        if (cY)    { hPeak[c].peak (sr, 8000.0f, 0.9f, p[pY]->load() * 5.0f); hShelf[c].lowShelf (sr, 200.0f, -p[pY]->load() * 2.5f); }
        if (cLow)  { hpf[c].highpass (sr, p[pLowCut]->load(), 0.54f); hpf2[c].highpass (sr, p[pLowCut]->load(), 1.31f); }
        if (cZ || cWrap)
        {
            float az, el, dist; direction (p[pX]->load(), p[pY]->load(), p[pZ]->load(), wrap, az, el, dist);
            air[c].lowpass (sr, 20000.0f * std::pow (0.2f, dist));
        }
    }
}

void ConstellationProcessor::publishParams()
{
    if (slotIndex < 0) return;
    auto& s = Registry::get().slot (slotIndex);
    for (int i = 0; i < kNumStarParams; ++i) s.val[(size_t) i].set (p[(size_t) i]->load());
}

void ConstellationProcessor::computeUnmask (int numSamples)
{
    auto& reg = Registry::get();
    const float amt = p[pUnmask]->load();
    const float blockSec = (float) numSamples / (float) sr;
    float target[kBands] {};
    if (amt > 0.001f && slotIndex >= 0)
    {
        const uint32_t now = juce::Time::getMillisecondCounter();
        const float prio = p[pPriority]->load(), x = p[pX]->load(), y = p[pY]->load(), z = p[pZ]->load();
        float maxM[kBands] {};
        float own[kBands];
        float ownMax = -100.0f;
        for (int b = 0; b < kBands; ++b) { own[b] = gainToDb (std::sqrt (detEnv[b])) + 3.0f; ownMax = std::max (ownMax, own[b]); }
        float ownW[kBands];
        for (int b = 0; b < kBands; ++b) ownW[b] = sat01 ((own[b] + 56.0f) / 30.0f) * sat01 ((own[b] - ownMax + 24.0f) / 18.0f);
        for (int t = 0; t < kMaxStars; ++t)
        {
            if (t == slotIndex) continue;
            auto& o = reg.slot (t);
            if (! o.fresh (now) || o.isSky.load()) continue;
            const float op = o.val[pPriority].get();
            if (op <= prio || o.val[pMute].get() > 0.5f) continue;
            const float prox = 1.0f - juce::jlimit (0.0f, 0.85f, 0.55f * std::abs (o.val[pX].get() - x) + 0.35f * std::abs (o.val[pZ].get() - z)
                                                               + 0.25f * std::abs (o.val[pY].get() - y));
            const float w = prox * (1.0f + 0.08f * (op - prio));
            float oMax = -100.0f;
            for (int b = 0; b < kBands; ++b) oMax = std::max (oMax, o.bandDb[(size_t) b].get());
            for (int b = 0; b < kBands; ++b)
            {
                const float ob = o.bandDb[(size_t) b].get();
                const float m = sat01 ((ob + 50.0f) / 30.0f) * sat01 ((ob - oMax + 24.0f) / 18.0f) * ownW[b] * w;
                maxM[b] = std::max (maxM[b], m);
            }
        }
        for (int b = 0; b < kBands; ++b) target[b] = -amt * 9.0f * std::min (1.0f, maxM[b]) * (b >= 9 ? 0.5f : 1.0f);
    }
    const float ka = 1.0f - std::exp (-blockSec / 0.025f), kr = 1.0f - std::exp (-blockSec / 0.22f);
    for (int b = 0; b < kBands; ++b)
    {
        cutDb[b] += (target[b] < cutDb[b] ? ka : kr) * (target[b] - cutDb[b]);
        if (std::abs (cutDb[b] - cutApplied[b]) > 0.05f)
        {
            cutApplied[b] = cutDb[b];
            for (int c = 0; c < 2; ++c) cut[c][b].peak (sr, kBandHz[b], 1.2f, cutApplied[b]);
        }
        if (slotIndex >= 0) reg.slot (slotIndex).cutDb[(size_t) b].set (cutDb[b]);
    }
}

void ConstellationProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    if (slotIndex >= 0)
    {
        auto& s = Registry::get().slot (slotIndex);
        s.heartbeat.store (juce::Time::getMillisecondCounter());
        s.isSky.store (isSkyMode() ? 1 : 0);
    }
    publishParams();
    if (licence().expired())
    {
        // trial over and no licence: the stars stay visible, audio passes through untouched
        for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, buffer.getNumSamples());
        return;
    }
    if (isSkyMode()) processSky (buffer); else processStar (buffer);
}

void ConstellationProcessor::processSky (juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int nc = buffer.getNumChannels();
    if (nc < 1 || n == 0) return;
    auto* L = buffer.getWritePointer (0);
    auto* R = nc > 1 ? buffer.getWritePointer (1) : L;
    const bool mono = Registry::get().globals.monoCheck.load() != 0;
    const float envA = std::exp (-1.0f / (0.05f * (float) sr));
    float pkL = 0, pkR = 0;
    for (int i = 0; i < n; ++i)
    {
        const float l = L[i], r = R[i], m = 0.5f * (l + r);
        for (int b = 0; b < kBands; ++b) { const float d = det[b].process (m); detEnv[b] = d * d + envA * (detEnv[b] - d * d); }
        corrNum  = l * r + 0.9995f * (corrNum - l * r);
        corrDen1 = l * l + 0.9995f * (corrDen1 - l * l);
        corrDen2 = r * r + 0.9995f * (corrDen2 - r * r);
        pkL = std::max (pkL, std::abs (l)); pkR = std::max (pkR, std::abs (r));
        if (mono) { L[i] = m; if (nc > 1) R[i] = m; }
    }
    correlation.store (corrNum / std::sqrt (corrDen1 * corrDen2 + 1.0e-12f));

    const int st = recState.load();
    if (st == recWaiting || st == recRunning)
    {
        juce::int64 tpos = 0; bool playing = false;
        const bool ok = readPlayhead (tpos, playing);
        if (ok && playing && Registry::get().globals.recArmed.load() != 0 && recChannels > 2 && recScratch.getNumSamples() >= n)
        {
            recState.store (recRunning);
            auto& bus = Registry::get().bus;
            float* rp[kRecMaxCh] {};
            for (int c = 0; c < recChannels; ++c) rp[c] = recScratch.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                const int fr = SurroundBus::frame (tpos + i);
                for (int c = 0; c < recChannels; ++c) rp[c][i] = bus.take (c, fr);
            }
            const juce::SpinLock::ScopedTryLockType tl (writerLock);
            if (tl.isLocked() && bedWriter != nullptr && binWriter != nullptr)
            {
                const float* bed[kRecMaxCh] {};
                for (int c = 0; c < recChannels - 2; ++c) bed[c] = rp[c];
                const float* bi[2] = { rp[recChannels - 2], rp[recChannels - 1] };
                auto push = [&] (juce::AudioFormatWriter::ThreadedWriter& w, const float* const* d)
                {
                    int guard = 0;
                    while (! w.write (d, n) && isNonRealtime() && ++guard < 5000) juce::Thread::sleep (1); // offline export: wait for the disk
                };
                push (*bedWriter, bed); push (*binWriter, bi);
            }
            recSamples.fetch_add (n);
        }
        else if (st == recRunning && ! playing)
            recState.store (recDone);
    }
    masterL.store (std::max (gainToDb (pkL), masterL.load() - 1.5f));
    masterR.store (std::max (gainToDb (pkR), masterR.load() - 1.5f));
    if (slotIndex >= 0)
    {
        auto& s = Registry::get().slot (slotIndex);
        for (int b = 0; b < kBands; ++b) s.bandDb[(size_t) b].set (gainToDb (std::sqrt (detEnv[b])) + 3.0f);
        s.levelDb.set (gainToDb (std::max (pkL, pkR)));
    }
}

void ConstellationProcessor::processStar (juce::AudioBuffer<float>& buffer)
{
    auto& reg = Registry::get();
    const int n   = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels();
    const int nOut = buffer.getNumChannels();
    if (n == 0 || nOut == 0) return;
    if (scratch.getNumSamples() < n) scratch.setSize (2, n, false, false, true);

    // ── layout / globals ──
    const auto outSet = getBus (false, 0) != nullptr ? getBus (false, 0)->getCurrentLayout() : juce::AudioChannelSet::stereo();
    if (layoutChannels != outSet.size())
    {
        layout.build (outSet); layoutChannels = outSet.size();
        outGains.assign ((size_t) (2 * layoutChannels), 0.0f); outGainsPrev = outGains;
        computeRevGains (layout, revGains);
    }
    const int room = reg.globals.room.load();
    if (room != lastRoom) { reverb.setRoom (room); lastRoom = room; }
    const float wrap = reg.globals.wrap.get();
    const bool binaural = nOut == 2 && reg.globals.binaural.load() != 0;
    if ((int) binaural != lastBin) { lastBin = (int) binaural; for (auto& b : bin) b.clear(); }

    // ── solo / mute ──
    bool anySolo = false;
    const uint32_t now = juce::Time::getMillisecondCounter();
    for (int t = 0; t < kMaxStars; ++t)
    {
        auto& o = reg.slot (t);
        if (o.fresh (now) && ! o.isSky.load() && o.val[pSolo].get() > 0.5f) { anySolo = true; break; }
    }
    const bool audible = p[pMute]->load() < 0.5f && (! anySolo || p[pSolo]->load() > 0.5f);

    updateFilters (false);
    computeUnmask (n);

    // ── copy input ──
    auto* sL = scratch.getWritePointer (0); auto* sR = scratch.getWritePointer (1);
    juce::FloatVectorOperations::copy (sL, buffer.getReadPointer (0), n);
    juce::FloatVectorOperations::copy (sR, buffer.getReadPointer (nIn > 1 ? 1 : 0), n);
    buffer.clear();

    // ── spatial targets ──
    const float x = p[pX]->load(), y = p[pY]->load(), z = p[pZ]->load();
    float az, el, dist; direction (x, y, z, wrap, az, el, dist);
    gainSm.setTargetValue (dbToGain (p[pGain]->load()));
    muteSm.setTargetValue (audible ? 1.0f : 0.0f);
    widthSm.setTargetValue (nIn > 1 ? p[pWidth]->load() : 0.0f);
    directSm.setTargetValue (dbToGain (-dist * 11.0f));
    sendSm.setTargetValue (p[pSpace]->load() * (0.25f + 0.75f * juce::jmin (1.0f, z)) * 0.55f);
    const float closeness = 1.0f - z;
    const bool lowcutOn = p[pLowCut]->load() > 21.0f;
    const bool airOn = dist > 0.02f;

    const bool speakers = nOut > 2;
    if (! speakers && ! binaural)
    {
        const float s = 1.0f - std::abs (x);
        const float pls[2] = { juce::jlimit (-1.0f, 1.0f, x - s), juce::jlimit (-1.0f, 1.0f, x + s) };
        for (int k = 0; k < 2; ++k) { panTGL[k] = std::cos ((pls[k] + 1.0f) * kPi * 0.25f); panTGR[k] = std::sin ((pls[k] + 1.0f) * kPi * 0.25f); }
    }
    else if (binaural)
    {
        const float sp = 30.0f * (1.0f - 0.5f * std::abs (x));
        bin[0].setDirection (az - sp, el); bin[1].setDirection (az + sp, el);
    }
    else
    {
        outGainsPrev = outGains;
        const float spread = 0.08f + 0.35f * dist + 0.1f * juce::jmax (0.0f, p[pWidth]->load() - 1.0f);
        const float sp = 25.0f + 10.0f * juce::jmin (1.0f, p[pWidth]->load());
        float tmp[32] {};
        layout.compute (az - sp, el, spread, tmp);
        for (int c = 0; c < layoutChannels && c < 32; ++c) outGains[(size_t) (c * 2)] = tmp[c];
        layout.compute (az + sp, el, spread, tmp);
        for (int c = 0; c < layoutChannels && c < 32; ++c) outGains[(size_t) (c * 2 + 1)] = tmp[c];
    }

    // ── surround recorder: render the chosen bed + a binaural pair into the shared bus ──
    juce::int64 tpos = 0; bool playing = false;
    const bool recOn = reg.globals.recArmed.load() != 0 && readPlayhead (tpos, playing) && playing && ! licence().expired();
    int recN = 0;
    float* rp[kRecMaxCh] {};
    if (recOn)
    {
        const int fmt = juce::jlimit (0, 2, reg.globals.recFormat.load());
        if (fmt != recLayoutFmt)
        {
            recLayout.build (recFormatSet (fmt)); recLayoutFmt = fmt;
            recG.assign ((size_t) (2 * recLayout.numChannels), 0.0f); recGPrev = recG;
            computeRevGains (recLayout, recRev);
            for (auto& b : recBin) b.clear();
        }
        recN = recLayout.numChannels;
        recGPrev = recG;
        const float spread = 0.08f + 0.35f * dist + 0.1f * juce::jmax (0.0f, p[pWidth]->load() - 1.0f);
        const float sp = 25.0f + 10.0f * juce::jmin (1.0f, p[pWidth]->load());
        float tmp[32] {};
        recLayout.compute (az - sp, el, spread, tmp);
        for (int c = 0; c < recN; ++c) recG[(size_t) (c * 2)] = tmp[c];
        recLayout.compute (az + sp, el, spread, tmp);
        for (int c = 0; c < recN; ++c) recG[(size_t) (c * 2 + 1)] = tmp[c];
        const float bsp = 30.0f * (1.0f - 0.5f * std::abs (x));
        recBin[0].setDirection (az - bsp, el); recBin[1].setDirection (az + bsp, el);
        for (int c = 0; c < recN + 2; ++c) rp[c] = recScratch.getWritePointer (c);
    }

    const float envA = std::exp (-1.0f / (0.04f * (float) sr));
    bool cutActive[kBands]; for (int b = 0; b < kBands; ++b) cutActive[b] = std::abs (cutApplied[b]) > 0.05f;
    float pk = 0.0f;
    float* out[16] {}; for (int c = 0; c < juce::jmin (16, nOut); ++c) out[c] = buffer.getWritePointer (c);
    const float invN = 1.0f / (float) n;

    for (int i = 0; i < n; ++i)
    {
        const float g = gainSm.getNextValue() * muteSm.getNextValue();
        float l = sL[i] * g, r = sR[i] * g;
        if (lowcutOn) { l = hpf2[0].process (hpf[0].process (l)); r = hpf2[1].process (hpf[1].process (r)); }
        l = highSh[0].process (lowSh[0].process (l)); r = highSh[1].process (lowSh[1].process (r));
        l = hShelf[0].process (hPeak[0].process (l)); r = hShelf[1].process (hPeak[1].process (r));

        const float m = 0.5f * (l + r);
        for (int b = 0; b < kBands; ++b) { const float d = det[b].process (m); detEnv[b] = d * d + envA * (detEnv[b] - d * d); }
        for (int b = 0; b < kBands; ++b) if (cutActive[b]) { l = cut[0][b].process (l); r = cut[1][b].process (r); }

        // width (M/S)
        const float w = widthSm.getNextValue();
        const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * w;
        l = mid + side; r = mid - side;

        // depth
        const float dg = directSm.getNextValue();
        if (airOn) { l = air[0].process (l); r = air[1].process (r); }
        float rvL, rvR; reverb.process ((l + r) * 0.5f * sendSm.getNextValue(), closeness, rvL, rvR);
        l *= dg; r *= dg;

        if (recOn)
        {
            const float t = (float) i * invN;
            for (int c = 0; c < recN; ++c)
            {
                const size_t a = (size_t) (c * 2);
                const float ga = recGPrev[a] + t * (recG[a] - recGPrev[a]);
                const float gb = recGPrev[a + 1] + t * (recG[a + 1] - recGPrev[a + 1]);
                rp[c][i] = ga * l + gb * r + recRev[a] * rvL + recRev[a + 1] * rvR;
            }
            float bl = 0, br = 0;
            recBin[0].process (l, bl, br); recBin[1].process (r, bl, br);
            rp[recN][i] = bl * 0.85f + rvL; rp[recN + 1][i] = br * 0.85f + rvR;
        }

        if (speakers)
        {
            const float t = (float) i * invN;
            for (int c = 0; c < layoutChannels && c < 16; ++c)
            {
                const size_t a = (size_t) (c * 2);
                const float ga = outGainsPrev[a] + t * (outGains[a] - outGainsPrev[a]);
                const float gb = outGainsPrev[a + 1] + t * (outGains[a + 1] - outGainsPrev[a + 1]);
                out[c][i] = ga * l + gb * r + revGains[a] * rvL + revGains[a + 1] * rvR;
            }
        }
        else if (binaural)
        {
            float ol = 0, orr = 0;
            bin[0].process (l, ol, orr); bin[1].process (r, ol, orr);
            out[0][i] = ol * 0.85f + rvL; out[1][i] = orr * 0.85f + rvR;
        }
        else
        {
            for (int k = 0; k < 2; ++k) { panGL[k] += 0.002f * (panTGL[k] - panGL[k]); panGR[k] += 0.002f * (panTGR[k] - panGR[k]); }
            out[0][i] = l * panGL[0] + r * panGL[1] + rvL;
            out[1][i] = l * panGR[0] + r * panGR[1] + rvR;
        }
        pk = std::max (pk, std::abs (out[0][i]));
        if (nOut > 1) pk = std::max (pk, std::abs (out[1][i]));
    }

    if (recOn)
        for (int i = 0; i < n; ++i)
        {
            const int fr = SurroundBus::frame (tpos + i);
            for (int c = 0; c < recN + 2; ++c) reg.bus.add (c, fr, rp[c][i]);
        }

    if (slotIndex >= 0)
    {
        auto& s = reg.slot (slotIndex);
        for (int b = 0; b < kBands; ++b) s.bandDb[(size_t) b].set (gainToDb (std::sqrt (detEnv[b])) + 3.0f);
        peakHold = std::max (gainToDb (pk), peakHold - 0.8f);
        s.levelDb.set (peakHold);
    }
}

// ───────────────────────────── message-thread API ─────────────────────────────
void ConstellationProcessor::applyOwnParam (int param, float v)
{
    if (auto* prm = apvts.getParameter (kIds[param]))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        prm->endChangeGesture();
    }
    if (slotIndex >= 0) Registry::get().slot (slotIndex).val[(size_t) param].set (p[(size_t) param]->load());
}

void ConstellationProcessor::setStarParam (int slot, int param, float v)
{
    if (slot < 0 || slot >= kMaxStars) return;
    auto& s = Registry::get().slot (slot);
    if (s.active.load() && s.owner != nullptr) static_cast<ConstellationProcessor*> (s.owner)->applyOwnParam (param, v);
}

int ConstellationProcessor::effectiveRole (int slot) const
{
    auto& s = Registry::get().slot (slot);
    const int r = (int) std::round (s.val[pRole].get());
    return r == rAuto ? detectRole (s.name) : r;
}

float ConstellationProcessor::pairMasking (int a, int b)
{
    auto& A = Registry::get().slot (a); auto& B = Registry::get().slot (b);
    float mx = 0, sum = 0, aMax = -100.0f, bMax = -100.0f;
    for (int k = 0; k < kBands; ++k) { aMax = std::max (aMax, A.bandDb[(size_t) k].get()); bMax = std::max (bMax, B.bandDb[(size_t) k].get()); }
    for (int k = 0; k < kBands - 1; ++k)
    {
        const float a = A.bandDb[(size_t) k].get(), b = B.bandDb[(size_t) k].get();
        const float m = sat01 ((a + 48.0f) / 30.0f) * sat01 ((a - aMax + 24.0f) / 18.0f)
                      * sat01 ((b + 48.0f) / 30.0f) * sat01 ((b - bMax + 24.0f) / 18.0f);
        mx = std::max (mx, m); sum += m;
    }
    const float raw = 0.6f * mx + 0.4f * sum / (float) (kBands - 1);
    const float prox = 1.0f - juce::jlimit (0.0f, 1.0f, 0.9f * std::abs (A.val[pX].get() - B.val[pX].get()) + 0.6f * std::abs (A.val[pZ].get() - B.val[pZ].get())
                                                       + 0.4f * std::abs (A.val[pY].get() - B.val[pY].get()));
    return raw * prox;
}

void ConstellationProcessor::applyStyle (int style)
{
    const uint32_t now = juce::Time::getMillisecondCounter();
    std::vector<int> stars;
    for (int t = 0; t < kMaxStars; ++t) { auto& o = Registry::get().slot (t); if (o.fresh (now) && ! o.isSky.load()) stars.push_back (t); }
    int count[kNumRoles] {}, seen[kNumRoles] {};
    for (int t : stars) count[effectiveRole (t)]++;
    for (int t : stars)
    {
        const int role = effectiveRole (t);
        auto pl = stylePlace (style, role);
        const int k = seen[role]++, nr = count[role];
        if (nr > 1)
        {
            const float side = (k % 2 == 0 ? -1.0f : 1.0f) * (pl.x >= 0 ? 1.0f : -1.0f);
            const float off = 0.22f + 0.18f * (float) (k / 2);
            pl.x = juce::jlimit (-1.0f, 1.0f, (std::abs (pl.x) < 0.1f ? 0.0f : pl.x) + side * off);
            pl.z = juce::jlimit (0.0f, 1.0f, pl.z + 0.05f * (float) k);
            pl.priority = juce::jmax (1.0f, pl.priority - (float) (k > 0));
        }
        setStarParam (t, pX, pl.x); setStarParam (t, pY, pl.y); setStarParam (t, pZ, pl.z);
        setStarParam (t, pWidth, pl.width); setStarParam (t, pPriority, pl.priority);
        setStarParam (t, pSpace, pl.space); setStarParam (t, pTone, pl.tone);
    }
}

void ConstellationProcessor::untangle()
{
    const uint32_t now = juce::Time::getMillisecondCounter();
    std::vector<int> stars;
    for (int t = 0; t < kMaxStars; ++t) { auto& o = Registry::get().slot (t); if (o.fresh (now) && ! o.isSky.load()) stars.push_back (t); }
    auto& R = Registry::get();
    for (int it = 0; it < 3; ++it)
        for (size_t i = 0; i < stars.size(); ++i)
            for (size_t j = i + 1; j < stars.size(); ++j)
            {
                const int a = stars[i], b = stars[j];
                const float m = pairMasking (a, b);
                if (m < 0.3f) continue;
                const bool aWins = R.slot (a).val[pPriority].get() >= R.slot (b).val[pPriority].get();
                const int lo = aWins ? b : a, hi = aWins ? a : b;
                auto& L = R.slot (lo); auto& H = R.slot (hi);
                const int role = effectiveRole (lo);
                const bool centreRole = role == rKick || role == rBass || role == rLeadVox || role == rSnare;
                if (centreRole)
                    setStarParam (lo, pZ, juce::jlimit (0.0f, 1.0f, L.val[pZ].get() + 0.12f * m));
                else
                {
                    float dx = L.val[pX].get() - H.val[pX].get();
                    const float dir = std::abs (dx) < 0.02f ? ((lo & 1) ? 1.0f : -1.0f) : (dx > 0 ? 1.0f : -1.0f);
                    setStarParam (lo, pX, juce::jlimit (-1.0f, 1.0f, L.val[pX].get() + dir * 0.22f * m));
                    setStarParam (lo, pZ, juce::jlimit (0.0f, 1.0f, L.val[pZ].get() + 0.05f * m));
                }
            }
}

void ConstellationProcessor::storeScene (int i)
{
    juce::ValueTree sc ("Scene");
    const uint32_t now = juce::Time::getMillisecondCounter();
    for (int t = 0; t < kMaxStars; ++t)
    {
        auto& o = Registry::get().slot (t);
        if (! o.fresh (now) || o.isSky.load()) continue;
        juce::ValueTree st ("Star");
        st.setProperty ("uuid", o.uuid, nullptr);
        for (int k : { (int) pX, (int) pY, (int) pZ, (int) pWidth, (int) pGain, (int) pSpace, (int) pTone })
            st.setProperty (kIds[k], o.val[(size_t) k].get(), nullptr);
        sc.addChild (st, -1, nullptr);
    }
    scenes[(size_t) i] = sc;
    lastSceneTouched = i;
}

void ConstellationProcessor::recallScene (int i)
{
    if (! sceneStored (i)) return;
    for (auto st : scenes[(size_t) i])
    {
        const auto id = st["uuid"].toString();
        for (int t = 0; t < kMaxStars; ++t)
        {
            auto& o = Registry::get().slot (t);
            if (o.active.load() && o.uuid == id)
                for (int k : { (int) pX, (int) pY, (int) pZ, (int) pWidth, (int) pGain, (int) pSpace, (int) pTone })
                    if (st.hasProperty (kIds[k])) setStarParam (t, k, (float) st[kIds[k]]);
        }
    }
    lastSceneTouched = i;
}

void ConstellationProcessor::applyMorph (float m)
{
    std::vector<int> used; for (int i = 0; i < 4; ++i) if (sceneStored (i)) used.push_back (i);
    if (used.size() < 2) return;
    const float pos = m * (float) (used.size() - 1);
    const int a = juce::jmin ((int) pos, (int) used.size() - 2);
    const float f = pos - (float) a;
    const auto& A = scenes[(size_t) used[(size_t) a]]; const auto& B = scenes[(size_t) used[(size_t) a + 1]];
    for (auto sa : A)
    {
        const auto id = sa["uuid"].toString();
        auto sb = B.getChildWithProperty ("uuid", id);
        for (int t = 0; t < kMaxStars; ++t)
        {
            auto& o = Registry::get().slot (t);
            if (! o.active.load() || o.uuid != id) continue;
            for (int k : { (int) pX, (int) pY, (int) pZ, (int) pWidth, (int) pGain, (int) pSpace, (int) pTone })
            {
                const float va = (float) sa[kIds[k]];
                const float vb = sb.isValid() ? (float) sb[kIds[k]] : va;
                setStarParam (t, k, va + f * (vb - va));
            }
        }
    }
}

void ConstellationProcessor::timerCallback()
{
    if (slotIndex < 0) return;
    auto& s = Registry::get().slot (slotIndex);
    {
        const juce::SpinLock::ScopedLockType sl (trackLock);
        if (trackDirty)
        {
            // Live sends name and colour in separate calls (the missing one comes empty), so keep what we have
            if (pendingName.isNotEmpty()) trackName = pendingName;
            if (pendingColour.getAlpha() > 0) trackColour = pendingColour.withAlpha ((juce::uint8) 255).getARGB();
            trackDirty = false;
        }
    }
    s.name = trackName;
    s.uuid = uuid;
    s.colour = trackColour != 0 ? trackColour : roleColour[effectiveRole (slotIndex)];

    if (recState.load() == recDone) finishRecorder();

    if (isSkyMode())
    {
        const float m = morphParam->load();
        if (std::abs (m - lastMorph) > 1.0e-4f) { lastMorph = m; applyMorph (m); }
    }
}

void ConstellationProcessor::updateTrackProperties (const TrackProperties& props)
{
    const juce::SpinLock::ScopedLockType sl (trackLock);
    if (props.name.isNotEmpty()) pendingName = props.name;
    if (props.colour.getAlpha() > 0) pendingColour = props.colour;
    trackDirty = true;
}

void ConstellationProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("uuid", uuid, nullptr);
    auto& g = Registry::get().globals;
    juce::ValueTree gl ("Globals");
    gl.setProperty ("room", g.room.load(), nullptr); gl.setProperty ("wrap", g.wrap.get(), nullptr);
    gl.setProperty ("binaural", g.binaural.load(), nullptr); gl.setProperty ("lang", g.lang.load(), nullptr);
    state.removeChild (state.getChildWithName ("Globals"), nullptr);
    state.addChild (gl, -1, nullptr);
    state.removeChild (state.getChildWithName ("Scenes"), nullptr);
    juce::ValueTree sc ("Scenes");
    for (int i = 0; i < 4; ++i) { auto c = scenes[(size_t) i].isValid() ? scenes[(size_t) i].createCopy() : juce::ValueTree ("Scene"); c.setProperty ("index", i, nullptr); sc.addChild (c, -1, nullptr); }
    state.addChild (sc, -1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ConstellationProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    if (state.hasProperty ("uuid")) uuid = state["uuid"].toString();
    auto gl = state.getChildWithName ("Globals");
    if (gl.isValid())
    {
        auto& g = Registry::get().globals;
        g.room.store ((int) gl["room"]); g.wrap.set ((float) gl["wrap"]);
        g.binaural.store ((int) gl["binaural"]); g.lang.store ((int) gl["lang"]);
        g.version++;
    }
    auto sc = state.getChildWithName ("Scenes");
    for (int i = 0; i < 4; ++i) scenes[(size_t) i] = juce::ValueTree();
    if (sc.isValid())
        for (auto c : sc) { const int i = (int) c["index"]; if (i >= 0 && i < 4 && c.getNumChildren() > 0) scenes[(size_t) i] = c.createCopy(); }
    state.removeChild (state.getChildWithName ("Globals"), nullptr);
    state.removeChild (state.getChildWithName ("Scenes"), nullptr);
    apvts.replaceState (state);
    lastMorph = morphParam->load();
    if (slotIndex >= 0) { auto& s = Registry::get().slot (slotIndex); s.uuid = uuid; s.isSky.store (isSkyMode() ? 1 : 0); }
    publishParams();
}

// ───────────────────────────── surround recorder ─────────────────────────────
bool ConstellationProcessor::readPlayhead (juce::int64& pos, bool& playing)
{
    playing = false; pos = 0;
    if (auto* ph = getPlayHead())
        if (auto info = ph->getPosition())
        {
            playing = info->getIsPlaying();
            if (auto t = info->getTimeInSamples()) { pos = *t; return true; }
        }
    return false;
}

bool ConstellationProcessor::armRecorder (int format)
{
    if (! isSkyMode() || licence().expired()) return false;
    if (recState.load() != recIdle) stopRecorder();
    format = juce::jlimit (0, 2, format);
    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Constellation Surround");
    dir.createDirectory();
    const auto base = "Constellation_" + juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S");
    const auto set = recFormatSet (format);
    lastRecording    = dir.getChildFile (base + "_" + recFormatName (format) + ".wav");
    lastRecordingBin = dir.getChildFile (base + "_Binaural.wav");
    if (writerThread == nullptr) { writerThread = std::make_unique<juce::TimeSliceThread> ("Constellation recorder"); writerThread->startThread(); }
    auto make = [this] (const juce::File& f, const juce::AudioChannelSet& cs) -> std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter>
    {
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
        if (os == nullptr) return {};
        juce::WavAudioFormat wav;
        auto* w = wav.createWriterFor (os.get(), sr, cs, 24, {}, 0);
        if (w == nullptr) return {};
        os.release();
        return std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (w, *writerThread, 1 << 18);
    };
    auto bed = make (lastRecording, set);
    auto bi2 = make (lastRecordingBin, juce::AudioChannelSet::stereo());
    if (bed == nullptr || bi2 == nullptr) { lastRecording.deleteFile(); lastRecordingBin.deleteFile(); return false; }
    {
        const juce::SpinLock::ScopedLockType sl (writerLock);
        bedWriter = std::move (bed); binWriter = std::move (bi2);
        recChannels = set.size() + 2;
    }
    recSamples.store (0);
    auto& reg = Registry::get();
    reg.bus.clear();
    reg.globals.recFormat.store (format);
    reg.globals.recArmed.store (1);
    recState.store (recWaiting);
    return true;
}

void ConstellationProcessor::finishRecorder()
{
    Registry::get().globals.recArmed.store (0);
    {
        const juce::SpinLock::ScopedLockType sl (writerLock);
        bedWriter.reset(); binWriter.reset();   // flushes and closes the files
    }
    if (recSamples.load() == 0) { lastRecording.deleteFile(); lastRecordingBin.deleteFile(); }
    recState.store (recIdle);
}

void ConstellationProcessor::stopRecorder() { finishRecorder(); }

// ───────────────────────────── licence / trial ─────────────────────────────
// 30-day full trial, then a licence key (RSA signature of the buyer's e-mail, verified offline).
namespace
{
const char* kPubN = CST_LICENCE_N;
juce::File licenceFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Polaris Sound Lab").getChildFile ("Constellation").getChildFile ("licence.xml");
}
juce::String normEmail (const juce::String& e) { return e.trim().toLowerCase(); }
bool verifyKey (const juce::String& email, const juce::String& key)
{
    const auto hex = key.retainCharacters ("0123456789abcdefABCDEF");
    if (hex.length() < 64 || normEmail (email).isEmpty()) return false;
    juce::BigInteger n, sgn, e (65537);
    n.parseString (kPubN, 16); sgn.parseString (hex, 16);
    if (sgn >= n) return false;
    const auto msg = juce::String ("CONSTELLATION|1|") + normEmail (email);
    juce::BigInteger m; m.parseString (juce::SHA256 (msg.toUTF8()).toHexString(), 16);
    sgn.exponentModulo (e, n);
    return sgn == m;
}
} // namespace

ConstellationProcessor::Licence& ConstellationProcessor::licence()
{
    static Licence L = []
    {
        Licence l;
        auto f = licenceFile();
        std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (f));
        const juce::int64 now = juce::Time::currentTimeMillis();
        juce::int64 first = now;
        if (xml != nullptr && xml->hasAttribute ("t"))
            first = xml->getStringAttribute ("t").getHexValue64() ^ (juce::int64) 0x5ca1ab1e0ddba11LL;
        else
        {
            f.getParentDirectory().createDirectory();
            juce::XmlElement x ("licence");
            x.setAttribute ("t", juce::String::toHexString (now ^ (juce::int64) 0x5ca1ab1e0ddba11LL));
            x.writeTo (f);
        }
        if (first > now) first = now;
        const double days = (double) (now - first) / 86400000.0;
        l.daysLeft = juce::jmax (0, 30 - (int) std::floor (days));
        if (xml != nullptr && verifyKey (xml->getStringAttribute ("email"), xml->getStringAttribute ("key")))
        { l.licensed = true; l.owner = normEmail (xml->getStringAttribute ("email")); }
        return l;
    }();
    return L;
}

bool ConstellationProcessor::activate (const juce::String& email, const juce::String& key)
{
    if (! verifyKey (email, key)) return false;
    auto f = licenceFile();
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (f));
    if (xml == nullptr) xml = std::make_unique<juce::XmlElement> ("licence");
    xml->setAttribute ("email", normEmail (email));
    xml->setAttribute ("key", key.retainCharacters ("0123456789abcdefABCDEF"));
    f.getParentDirectory().createDirectory();
    xml->writeTo (f);
    auto& l = licence(); l.licensed = true; l.owner = normEmail (email);
    return true;
}

juce::AudioProcessorEditor* ConstellationProcessor::createEditor() { return new ConstellationEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ConstellationProcessor(); }
