// Offline test: several Constellation instances in one process, like Ableton does.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

using namespace cst;

static void setP (ConstellationProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static float rms (const juce::AudioBuffer<float>& b, int ch)
{
    return b.getRMSLevel (ch, 0, b.getNumSamples());
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
