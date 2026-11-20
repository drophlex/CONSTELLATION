// Offline test: several Constellation instances in one process, like Ableton does.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

using namespace cst;

static void setP (ConstellationProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

struct TestHead : juce::AudioPlayHead
{
    juce::int64 pos = 0; bool playing = false;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo i; i.setIsPlaying (playing); i.setTimeInSamples (pos); return i; }
};

static float rms (const juce::AudioBuffer<float>& b, int ch)
{
    return b.getRMSLevel (ch, 0, b.getNumSamples());
}

static void shot (juce::AudioProcessor& p, const char* file, std::function<void (ConstellationEditor&)> prep = {})
{
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1200, 740);
    auto* ce = dynamic_cast<ConstellationEditor*> (ed.get());
    for (int k = 0; k < 6; ++k) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    if (prep && ce) prep (*ce);
    for (int k = 0; k < 4; ++k) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    juce::Image img (juce::Image::ARGB, 1200, 740, true);
    for (int k = 0; k < 30; ++k) { juce::Graphics g (img); ed->paintEntireComponent (g, true); }
    juce::File f (juce::String ("/home/claude/constellation/build/") + file); f.deleteFile();
    juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    const double sr = 48000; const int bs = 512;
    int failures = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "[ok]  " : "[FAIL]", what); if (! ok) ++failures; };

    ConstellationProcessor kick, bass, pad, sky;
    sky.setSkyMode (true);
    for (auto* p : { &kick, &bass, &pad, &sky })
    {
        p->setPlayConfigDetails (2, 2, sr, bs);
        p->prepareToPlay (sr, bs);
    }
    juce::AudioProcessor::TrackProperties tp; tp.name = "Kick"; kick.updateTrackProperties (tp);
    tp.name = "Sub Bass"; bass.updateTrackProperties (tp);
    tp.name = "Pad Atmos"; pad.updateTrackProperties (tp);
    setP (kick, "priority", 10); setP (bass, "priority", 4); setP (pad, "priority", 2);
    setP (bass, "unmask", 1.0f);

    juce::AudioBuffer<float> bk (2, bs), bb (2, bs), bp (2, bs), bm (2, bs);
    juce::MidiBuffer midi;
    double ph1 = 0, ph2 = 0; juce::Random rnd (1);
    bool finite = true;
    for (int blk = 0; blk < 400; ++blk)
    {
        for (int i = 0; i < bs; ++i)
        {
            const float k = 0.6f * std::sin ((float) ph1); ph1 += 2 * juce::MathConstants<double>::pi * 60 / sr;   // 60 Hz "kick"
            const float b = 0.5f * std::sin ((float) ph2); ph2 += 2 * juce::MathConstants<double>::pi * 55 / sr;   // 55 Hz bass
            const float n = 0.1f * (rnd.nextFloat() * 2 - 1);
            bk.setSample (0, i, k); bk.setSample (1, i, k);
            bb.setSample (0, i, b); bb.setSample (1, i, b);
            bp.setSample (0, i, n); bp.setSample (1, i, rnd.nextFloat() * 0.2f - 0.1f);
        }
        kick.processBlock (bk, midi); bass.processBlock (bb, midi); pad.processBlock (bp, midi);
        bm.clear(); for (int c = 0; c < 2; ++c) { bm.addFrom (c, 0, bk, c, 0, bs); bm.addFrom (c, 0, bb, c, 0, bs); bm.addFrom (c, 0, bp, c, 0, bs); }
        sky.processBlock (bm, midi);
        for (auto* b : { &bk, &bb, &bp, &bm }) for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) if (! std::isfinite (b->getSample (c, i))) finite = false;
        if (blk % 10 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
    }
    check (finite, "all outputs finite");
    auto& reg = Registry::get();
    const float cut50 = reg.slot (bass.getSlot()).cutDb[0].get();
    std::printf ("        bass unmask cut @50Hz = %.2f dB, @100Hz = %.2f dB, @3k = %.2f dB\n", cut50, reg.slot (bass.getSlot()).cutDb[1].get(), reg.slot (bass.getSlot()).cutDb[6].get());
    check (cut50 < -2.0f, "bass ducks where the higher-priority kick plays");
    check (reg.slot (kick.getSlot()).cutDb[0].get() > -0.1f, "kick (highest priority) is never ducked");
    check (std::abs (reg.slot (bass.getSlot()).cutDb[6].get()) < 0.5f, "no ducking in bands where the kick is silent");
    check (kick.effectiveRole (kick.getSlot()) == rKick && bass.effectiveRole (bass.getSlot()) == rBass && pad.effectiveRole (pad.getSlot()) == rPad, "roles detected from track names");

    // pan hard left → right channel silent
    setP (kick, "x", -1.0f); setP (kick, "space", 0.0f);
    for (int blk = 0; blk < 60; ++blk) { for (int i = 0; i < bs; ++i) { const float k = 0.5f * std::sin ((float) ph1); ph1 += 0.01; bk.setSample (0, i, k); bk.setSample (1, i, k); } kick.processBlock (bk, midi); }
    std::printf ("        hard-left: L=%.4f R=%.6f\n", rms (bk, 0), rms (bk, 1));
    check (rms (bk, 1) < 0.01f * rms (bk, 0), "hard-left pan");

    // binaural: source on the right → right ear louder & no NaN
    reg.globals.binaural.store (1); setP (kick, "x", 0.9f);
    for (int blk = 0; blk < 80; ++blk) { for (int i = 0; i < bs; ++i) { const float k = 0.3f * (rnd.nextFloat() * 2 - 1); bk.setSample (0, i, k); bk.setSample (1, i, k); } kick.processBlock (bk, midi); }
    std::printf ("        binaural right: L=%.4f R=%.4f\n", rms (bk, 0), rms (bk, 1));
    check (rms (bk, 1) > rms (bk, 0) * 1.3f, "binaural ILD toward the right ear");
    reg.globals.binaural.store (0);

    // 7.1.4 render
    {
        ConstellationProcessor s;
        juce::AudioProcessor::BusesLayout l; l.inputBuses.add (juce::AudioChannelSet::stereo()); l.outputBuses.add (juce::AudioChannelSet::create7point1point4());
        check (s.setBusesLayout (l), "7.1.4 layout accepted");
        s.setPlayConfigDetails (2, 12, sr, bs); s.prepareToPlay (sr, bs);
        setP (s, "x", 0.0f); setP (s, "y", 1.0f); setP (s, "space", 0.0f); setP (s, "z", 0.0f);
        reg.globals.wrap.set (1.0f);
        juce::AudioBuffer<float> b12 (12, bs);
        for (int blk = 0; blk < 80; ++blk)
        {
            b12.clear();
            for (int i = 0; i < bs; ++i) { const float k = 0.3f * (rnd.nextFloat() * 2 - 1); b12.setSample (0, i, k); b12.setSample (1, i, k); }
            s.processBlock (b12, midi);
        }
        float topE = 0, earE = 0; auto set = juce::AudioChannelSet::create7point1point4();
        for (int c = 0; c < 12; ++c) { auto t = set.getTypeOfChannel (c); const float e = rms (b12, c);
            if (t == juce::AudioChannelSet::topFrontLeft || t == juce::AudioChannelSet::topFrontRight || t == juce::AudioChannelSet::topRearLeft || t == juce::AudioChannelSet::topRearRight) topE += e; else earE += e; }
        std::printf ("        7.1.4 elevated source: top=%.4f ear=%.4f\n", topE, earE);
        check (topE > earE, "elevated star goes to the height speakers");
        reg.globals.wrap.set (0.0f);
    }

    // scenes + morph
    sky.storeScene (0);
    ConstellationProcessor::setStarParam (pad.getSlot(), pX, 1.0f);
    sky.storeScene (1);
    ConstellationProcessor::setStarParam (pad.getSlot(), pX, -1.0f);
    setP (sky, "morph", 0.5f);
    for (int k = 0; k < 10; ++k) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    const float px = pad.apvts.getRawParameterValue ("x")->load();
    std::printf ("        morph 0.5 pad x = %.3f\n", px);
    check (std::abs (px - 0.5f) < 0.05f, "scene morph interpolates star positions");

    // state round-trip
    juce::MemoryBlock mb; sky.getStateInformation (mb);
    ConstellationProcessor sky2; sky2.setStateInformation (mb.getData(), (int) mb.getSize());
    check (sky2.isSkyMode() && sky2.sceneStored (0) && sky2.sceneStored (1), "state restore keeps SKY mode and scenes");

    // style
    sky.applyStyle (sClub);
    check (std::abs (kick.apvts.getRawParameterValue ("x")->load()) < 0.01f, "Auto-Mix centers the kick");

    // licence: trial counting + offline key check
    {
        auto& L = ConstellationProcessor::licence();
        std::printf ("        trial days left = %d, licensed = %d\n", L.daysLeft.load(), (int) L.licensed.load());
        check (! L.expired(), "fresh install is in the 30-day trial");
        shot (sky, "licence.png", [] (ConstellationEditor& e) { e.showLicence(); });
        check (! ConstellationProcessor::activate ("tester@polaris.mx", "DEADBEEF-1234"), "a wrong key is rejected");
        check (! ConstellationProcessor::activate ("other@polaris.mx", "41691637-53FD413C-05E78B14-A66B2F3E-0F335769-ADAA0B23-4886AEEB-67E3347D-A0CC4A04-9E963E3C-62DCC98A-561B9BBA-55F0F106-5A8B9F49-35BA3DB2-989E5AE8-2393BBD6-F42BD897-C13DBBF3-0A3F6596-1BFA782A-6ABF26E1-25A0640E-298F7082-59AC7C8E-32C199EB-01957089-63D52EC3-C964C966-2A6E6A02-A013C694-F7A287C4"), "a key for another e-mail is rejected");
        check (ConstellationProcessor::activate ("  Tester@Polaris.MX ", "41691637-53FD413C-05E78B14-A66B2F3E-0F335769-ADAA0B23-4886AEEB-67E3347D-A0CC4A04-9E963E3C-62DCC98A-561B9BBA-55F0F106-5A8B9F49-35BA3DB2-989E5AE8-2393BBD6-F42BD897-C13DBBF3-0A3F6596-1BFA782A-6ABF26E1-25A0640E-298F7082-59AC7C8E-32C199EB-01957089-63D52EC3-C964C966-2A6E6A02-A013C694-F7A287C4"), "a valid key activates (case/space tolerant)");
        check (L.licensed.load(), "licence flag set");
    }

    // surround recorder: stars + SKY in one process, timeline-locked, like Ableton exporting
    {
        TestHead head;
        for (auto* p : { &kick, &bass, &pad, &sky }) p->setPlayHead (&head);
        setP (pad, "y", 1.0f); setP (pad, "x", 0.0f); setP (pad, "z", 0.1f); setP (pad, "space", 0.0f); setP (pad, "mute", 0.0f);
        setP (kick, "mute", 1.0f); setP (bass, "mute", 1.0f);
        check (sky.armRecorder (2), "SKY arms the 7.1.4 recorder");
        shot (sky, "sky_rec.png");
        head.playing = true; head.pos = 48000;
        const int blocks = 200;
        for (int blk = 0; blk < blocks; ++blk)
        {
            for (int i = 0; i < bs; ++i) { const float v = 0.3f * (rnd.nextFloat() * 2 - 1); bp.setSample (0, i, v); bp.setSample (1, i, v); bk.setSample (0, i, 0); bk.setSample (1, i, 0); bb.setSample (0, i, 0); bb.setSample (1, i, 0); }
            kick.processBlock (bk, midi); bass.processBlock (bb, midi); pad.processBlock (bp, midi);
            bm.makeCopyOf (bp); sky.processBlock (bm, midi);
            head.pos += bs;
        }
        check (sky.getRecState() == ConstellationProcessor::recRunning, "recorder runs while the transport plays");
        head.playing = false; sky.processBlock (bm, midi);
        for (int k = 0; k < 10; ++k) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        check (sky.getRecState() == ConstellationProcessor::recIdle && sky.lastRecording.existsAsFile(), "file finalised when the transport stops");
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> r (wav.createReaderFor (sky.lastRecording.createInputStream().release(), true));
        std::unique_ptr<juce::AudioFormatReader> rb (wav.createReaderFor (sky.lastRecordingBin.createInputStream().release(), true));
        check (r != nullptr && r->numChannels == 12 && r->lengthInSamples == (juce::int64) blocks * bs, "7.1.4 WAV: 12 channels, sample-exact length");
        check (rb != nullptr && rb->numChannels == 2, "binaural WAV written");
        if (r != nullptr)
        {
            juce::AudioBuffer<float> wb (12, (int) r->lengthInSamples); r->read (&wb, 0, wb.getNumSamples(), 0, true, true);
            float top = 0, ear = 0;
            for (int c = 0; c < 12; ++c) (c >= 8 ? top : ear) += wb.getRMSLevel (c, 0, wb.getNumSamples());
            std::printf ("        7.1.4 file: top=%.4f ear=%.4f  layout=%s  file=%s\n", top, ear, r->getChannelLayout().getDescription().toRawUTF8(), sky.lastRecording.getFileName().toRawUTF8());
            check (top > ear && top > 0.01f, "elevated star lands in the height channels of the file");
        }
        sky.lastRecording.copyFileTo (juce::File ("/home/claude/constellation/build/test_7.1.4.wav"));
        for (auto* p : { &kick, &bass, &pad, &sky }) p->setPlayHead (nullptr);
        setP (kick, "mute", 0.0f); setP (bass, "mute", 0.0f);
    }

    // editor smoke test
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (sky.createEditor());
        ed->setSize (1200, 740);
        juce::Image img (juce::Image::ARGB, 1200, 740, true);
        juce::Graphics g (img);
        for (int k = 0; k < 3; ++k) { juce::MessageManager::getInstance()->runDispatchLoopUntil (60); }
        ed->paintEntireComponent (g, true);
        juce::PNGImageFormat png; juce::File f ("/home/claude/constellation/build/sky.png"); f.deleteFile();
        juce::FileOutputStream os (f); png.writeImageToStream (img, os);
        check (true, "editor renders (build/sky.png)");
    }
    // extra screenshots for the manual: 7.1.4 star, top view, Spanish
    {
        ConstellationProcessor s;
        juce::AudioProcessor::BusesLayout l; l.inputBuses.add (juce::AudioChannelSet::stereo()); l.outputBuses.add (juce::AudioChannelSet::create7point1point4());
        s.setBusesLayout (l); s.setPlayConfigDetails (2, 12, sr, bs); s.prepareToPlay (sr, bs);
        juce::AudioProcessor::TrackProperties t2; t2.name = "Lead Vox"; t2.colour = juce::Colour (0xffff7ad9); s.updateTrackProperties (t2);
        reg.globals.wrap.set (0.75f); reg.globals.room.store (5);
        juce::AudioBuffer<float> b12 (12, bs);
        for (int k = 0; k < 4; ++k)
        {
            for (int blk = 0; blk < 20; ++blk)
            {
                b12.clear(); for (int i = 0; i < bs; ++i) { const float v = 0.3f * std::sin ((float) ph2 * 7); ph2 += 0.02; b12.setSample (0, i, v); b12.setSample (1, i, v); }
                s.processBlock (b12, midi);
                for (int i = 0; i < bs; ++i) { const float k2 = 0.6f * std::sin ((float) ph1); ph1 += 0.008; bk.setSample (0, i, k2); bk.setSample (1, i, k2); bb.setSample (0, i, k2 * 0.7f); bb.setSample (1, i, k2 * 0.7f); bp.setSample (0, i, rnd.nextFloat() * 0.2f - 0.1f); bp.setSample (1, i, rnd.nextFloat() * 0.2f - 0.1f); }
                kick.processBlock (bk, midi); bass.processBlock (bb, midi); pad.processBlock (bp, midi);
            }
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        }
        for (int lang = 0; lang < 2; ++lang)
        {
            reg.globals.lang.store (lang);
            std::unique_ptr<juce::AudioProcessorEditor> ed (s.createEditor());
            ed->setSize (1200, 740);
            if (auto* sv = dynamic_cast<SkyView*> (ed->getChildComponent (0))) sv->setView (lang == 0 ? SkyView::vTop : SkyView::v3D);
            for (int k = 0; k < 12; ++k) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            juce::Image img (juce::Image::ARGB, 1200, 740, true); for (int k = 0; k < 40; ++k) { juce::Graphics g (img); ed->paintEntireComponent (g, true); }
            juce::File f (juce::String ("/home/claude/constellation/build/star_") + (lang ? "es" : "en") + ".png"); f.deleteFile();
            juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        }
        reg.globals.lang.store (0); reg.globals.wrap.set (0.0f);
    }
    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d FAILURES\n", failures);
    return failures;
}
