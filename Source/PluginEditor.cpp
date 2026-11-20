// CONSTELLATION — editor. Design: Dark Funk Durcal · Polaris Sound Lab, Mérida, Yucatán
#include "PluginEditor.h"

using namespace cst;

namespace
{
juce::Font F (float h, bool bold = true) { return bold ? psl::ui (h * 1.08f) : psl::body (h); }
const char* kDonateUrl = "https://www.paypal.com/donate/?business=drophlex%40gmail.com&currency_code=MXN";
constexpr float kCamD = 3.4f;

void paperSheet (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& code, const juce::String& title, const juce::String& kana)
{
    using namespace psl;
    g.setColour (C::paper); g.fillRect (r);
    auto head = r.removeFromTop (54);
    g.setColour (C::cyan); g.fillRect (head);
    text (g, upper (title), head.reduced (22, 4), display (34), C::ink);
    text (g, kana, head.reduced (22, 6), jp (15), C::ink, juce::Justification::bottomRight);
    tech (g, code, head.reduced (22, 6), C::ink, juce::Justification::topRight, 9);
    regMark (g, { r.getX() + 12, r.getBottom() - 12 }, 6, C::ink);
    regMark (g, { r.getRight() - 12, r.getBottom() - 12 }, 6, C::ink);
}

struct HelpOverlay : juce::Component
{
    void paint (juce::Graphics& g) override
    {
        g.fillAll (psl::C::ink.withAlpha (0.93f));
        auto r = getLocalBounds().reduced (60, 44).toFloat();
        paperSheet (g, r, "CST-HLP // 01", "CONSTELLATION - " + juce::String (Registry::get().globals.lang.load() == 1 ? "GUIA" : "GUIDE"), juce::String::fromUTF8 ("ガイド"));
        r.removeFromTop (54);
        g.setColour (psl::C::ink); g.setFont (psl::body (14.0f));
        g.drawFittedText (T (sHelpText), r.reduced (28, 20).toNearestInt(), juce::Justification::topLeft, 40, 1.0f);
        psl::tech (g, T (sFooter), r.reduced (30, 12), psl::C::grey, juce::Justification::bottomRight, 9);
    }
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
};
struct LicencePanel : juce::Component
{
    juce::TextEditor email, key;
    juce::TextButton buy, activate, close;
    juce::String status; bool ok = false;
    std::function<void()> onDone;
    LicencePanel()
    {
        for (auto* t : { &email, &key })
        {
            addAndMakeVisible (*t);
            t->setFont (psl::mono (14));
        }
        key.setMultiLine (true); key.setReturnKeyStartsNewLine (false);
        for (auto* b : { &buy, &activate, &close }) addAndMakeVisible (*b);
        buy.setColour (juce::TextButton::buttonOnColourId, psl::C::cyan); buy.setToggleState (true, juce::dontSendNotification);
        activate.setColour (juce::TextButton::buttonOnColourId, psl::C::ink); activate.setColour (juce::TextButton::textColourOnId, psl::C::paper); activate.setToggleState (true, juce::dontSendNotification);
        close.setColour (juce::TextButton::buttonOnColourId, Theme::cyan());
        buy.onClick = [] { juce::URL (kBuyUrl).launchInDefaultBrowser(); };
        activate.onClick = [this]
        {
            ok = ConstellationProcessor::activate (email.getText(), key.getText());
            status = ok ? T2 (xActOk) : T2 (xActBad);
            repaint();
            if (ok && onDone) onDone();
        };
        close.onClick = [this] { setVisible (false); };
        refreshTexts();
    }
    void refreshTexts()
    {
        buy.setButtonText (T2 (xBuy) + "  " + kPrice); activate.setButtonText (T2 (xActivate)); close.setButtonText (T2 (xClose));
        email.setTextToShowWhenEmpty (T2 (xEmail), Theme::dim()); key.setTextToShowWhenEmpty (T2 (xKey), Theme::dim());
        repaint();
    }
    juce::Rectangle<int> box() const { return getLocalBounds().withSizeKeepingCentre (juce::jmin (640, getWidth() - 40), juce::jmin (520, getHeight() - 40)); }
    void paint (juce::Graphics& g) override
    {
        using namespace psl;
        g.fillAll (C::ink.withAlpha (0.93f));
        auto r = box().toFloat();
        paperSheet (g, r, "CST-LIC // 30D TRIAL", T2 (xLicTitle), juce::String::fromUTF8 ("ライセンス"));
        auto in = r.reduced (26, 0); in.removeFromTop (66);
        auto& L = ConstellationProcessor::licence();
        auto st = in.removeFromTop (24);
        g.setColour (L.licensed ? C::green : (L.expired() ? C::orange : C::ink)); g.fillRect (st.withWidth (12).reduced (0, 6));
        text (g, upper (L.licensed ? T2 (xLicensed) + ": " + L.owner : (L.expired() ? T2 (xExpired) : T2 (xTrial).replace ("%d", juce::String (L.daysLeft.load())))),
              st.withTrimmedLeft (20), ui (14), C::ink);
        g.setColour (C::ink); g.setFont (body (14));
        g.drawFittedText (T2 (xLicText).replace ("%s", kPrice), in.removeFromTop (118).toNearestInt(), juce::Justification::topLeft, 12, 1.0f);
        if (status.isNotEmpty())
            text (g, status, box().toFloat().reduced (26, 20).removeFromBottom (64).removeFromTop (22), ui (13), ok ? juce::Colour (0xff0a7a45) : C::orange);
    }
    void resized() override
    {
        auto in = box().reduced (26, 20);
        in.removeFromTop (66 + 24 + 118 + 6);
        buy.setBounds (in.removeFromTop (40).removeFromLeft (260)); in.removeFromTop (14);
        email.setBounds (in.removeFromTop (34)); in.removeFromTop (8);
        key.setBounds (in.removeFromTop (70)); in.removeFromTop (10);
        auto row = in.removeFromBottom (38);
        close.setBounds (row.removeFromRight (110)); row.removeFromRight (10);
        activate.setBounds (row.removeFromRight (150));
    }
    void mouseDown (const juce::MouseEvent& e) override { if (! box().contains (e.getPosition())) setVisible (false); }
};
} // namespace

// ───────────────────────────── SkyView ─────────────────────────────
SkyView::SkyView (ConstellationProcessor& p) : proc (p)
{
    juce::Random rnd (7);
    for (int i = 0; i < 260; ++i) bgStars.push_back ({ rnd.nextFloat(), rnd.nextFloat(), 0.4f + rnd.nextFloat() * 1.4f, rnd.nextFloat() * 6.28f });
    setOpaque (true);
}

void SkyView::setView (int v)
{
    view = v;
    if (v == vTop)        { yawTarget = 0.0f;   pitchTarget = 1.5707f; }
    else if (v == vFront) { yawTarget = 0.0f;   pitchTarget = 0.0f; }
    else                  { yawTarget = -0.55f; pitchTarget = 0.42f; }
}

float SkyView::listenerZ() const { return -1.35f + 1.35f * Registry::get().globals.wrap.get(); }

SkyView::P SkyView::project (float wx, float wy, float wz) const
{
    const float cy = std::cos (yaw), sy = std::sin (yaw);
    const float x1 = wx * cy - wz * sy, z1 = wx * sy + wz * cy;
    const float cp = std::cos (pitch), sp = std::sin (pitch);
    const float y2 = wy * cp + z1 * sp, z2 = -wy * sp + z1 * cp;
    const float zc = z2 + kCamD;
    const float f = kCamD / juce::jmax (0.4f, zc);
    return { fitX + x1 * f * fitS, fitY - y2 * f * fitS, zc, f };
}

void SkyView::fit()
{
    const float s0 = fitS, x0 = fitX, y0 = fitY;
    fitS = 1.0f; fitX = 0; fitY = 0;
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    const float lz = listenerZ();
    for (float x : { -1.0f, 1.0f }) for (float y : { -1.0f, 1.0f }) for (float z : { juce::jmin (-1.0f, lz - 0.2f), 1.0f })
    {
        auto pp = project (x, y, z);
        minX = std::min (minX, pp.x); maxX = std::max (maxX, pp.x); minY = std::min (minY, pp.y); maxY = std::max (maxY, pp.y);
    }
    juce::ignoreUnused (s0, x0, y0);
    const float w = (float) getWidth(), h = (float) getHeight();
    const float S = juce::jmin ((w - 80) / juce::jmax (0.01f, maxX - minX), (h - 70) / juce::jmax (0.01f, maxY - minY));
    fitS = S;
    fitX = w * 0.5f - S * (minX + maxX) * 0.5f;
    fitY = h * 0.5f - S * (minY + maxY) * 0.5f + 6.0f;
}

void SkyView::drawBackground (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    g.fillAll (psl::C::ink);
    psl::dotGrid (g, getLocalBounds().toFloat(), 16.0f, psl::C::ink3);
    g.setColour (psl::C::rule);
    for (float x = 0; x < w; x += 80) { g.fillRect (x, 0.0f, 1.0f, 6.0f); g.fillRect (x, h - 6.0f, 1.0f, 6.0f); psl::tech (g, juce::String ((int) x).paddedLeft ('0', 4), { x + 3, h - 14, 40, 10 }, psl::C::rule, juce::Justification::centredLeft, 8); }
    for (float y = 0; y < h; y += 80) { g.fillRect (0.0f, y, 6.0f, 1.0f); g.fillRect (w - 6.0f, y, 6.0f, 1.0f); }
    for (auto& s : bgStars)
    {
        const bool on = std::sin (time * (0.6f + s.s) + s.ph) > 0.1f;
        g.setColour (psl::C::paper.withAlpha (on ? 0.5f : 0.15f));
        const float x = s.x * w, y = s.y * h;
        if (s.s > 1.65f) { g.fillRect (x - 3, y, 7.0f, 1.0f); g.fillRect (x, y - 3, 1.0f, 7.0f); } else g.fillRect (x, y, 1.0f, 1.0f);
    }
}

void SkyView::drawGrid (juce::Graphics& g)
{
    auto seg = [&] (float ax, float ay, float az, float bx, float by, float bz, juce::Colour c, float th)
    {
        auto a = project (ax, ay, az), b = project (bx, by, bz);
        g.setColour (c); g.drawLine (a.x, a.y, b.x, b.y, th);
    };
    const auto gc = psl::C::paper.withAlpha (0.10f);
    for (int i = 0; i <= 8; ++i)
    {
        const float t = -1.0f + 0.25f * (float) i;
        seg (t, -1, -1, t, -1, 1, gc, 1.0f);
        seg (-1, -1, t, 1, -1, t, gc, 1.0f);
        if (view != vTop) { seg (t, -1, 1, t, 1, 1, gc.withMultipliedAlpha (0.5f), 1.0f); seg (-1, t, 1, 1, t, 1, gc.withMultipliedAlpha (0.5f), 1.0f); }
    }
    const auto ec = psl::C::paper.withAlpha (0.35f);
    for (float x : { -1.0f, 1.0f }) for (float z : { -1.0f, 1.0f }) seg (x, -1, z, x, 1, z, ec.withMultipliedAlpha (0.5f), 1.0f);
    for (float y : { -1.0f, 1.0f }) { seg (-1, y, -1, 1, y, -1, ec, 1.2f); seg (-1, y, 1, 1, y, 1, ec, 1.0f); seg (-1, y, -1, -1, y, 1, ec, 1.0f); seg (1, y, -1, 1, y, 1, ec, 1.0f); }

    auto lab = [&] (const juce::String& s, float x, float y, float z) { auto pp = project (x, y, z); psl::tech (g, s, { pp.x - 60, pp.y - 6, 120, 12 }, psl::C::cyan, juce::Justification::centred, 9); };
    const bool es = Registry::get().globals.lang.load() == 1;
    lab ("L", -1.12f, -1.0f, -1.0f); lab ("R", 1.12f, -1.0f, -1.0f);
    if (view != vFront) { lab (es ? "CERCA" : "NEAR", 0, -1.0f, -1.12f); lab (es ? "LEJOS" : "FAR", 0, -1.0f, 1.12f); }
    if (view != vTop)   { lab (es ? "BRILLANTE" : "HIGH / BRIGHT", 0, 1.1f, 1.0f); lab (es ? "GRAVE" : "LOW / WARM", 0, -1.12f, 1.0f); }
}

void SkyView::drawStar (juce::Graphics& g, int slot, const P& pp, bool fresh)
{
    auto& s = Registry::get().slot (slot);
    const auto col = juce::Colour (s.colour);
    const float lv = sat01 ((s.levelDb.get() + 48.0f) / 48.0f);
    const bool muted = s.val[pMute].get() > 0.5f;
    const float alpha = ! fresh ? 0.3f : (muted ? 0.4f : 1.0f);
    const float sc = juce::jlimit (0.6f, 1.5f, pp.scale);
    const float r = (5.0f + 9.0f * lv) * sc;
    const float w = s.val[pWidth].get();

    if (view == v3D)   // drop line to the floor + floor tick
    {
        auto fl = project (s.val[pX].get(), -1.0f, toWZ (s.val[pZ].get()));
        juce::Path dl; dl.startNewSubPath (pp.x, pp.y); dl.lineTo (fl.x, fl.y);
        juce::Path dd; const float d[] = { 2, 3 }; juce::PathStrokeType (1.0f).createDashedStroke (dd, dl, d, 2);
        g.setColour (col.withAlpha (0.5f * alpha)); g.fillPath (dd);
        g.fillRect (fl.x - r * (0.6f + w * 0.5f), fl.y - 1, r * (1.2f + w), 2.0f);
    }
    // width bracket
    const float bw = r * (1.6f + 1.4f * w);
    g.setColour (col.withAlpha (0.6f * alpha));
    g.fillRect (pp.x - bw, pp.y - r - 4, 1.0f, r * 2 + 8); g.fillRect (pp.x + bw, pp.y - r - 4, 1.0f, r * 2 + 8);
    g.fillRect (pp.x - bw, pp.y - r - 4, 4.0f, 1.0f); g.fillRect (pp.x + bw - 3, pp.y - r - 4, 4.0f, 1.0f);
    g.fillRect (pp.x - bw, pp.y + r + 4, 4.0f, 1.0f); g.fillRect (pp.x + bw - 3, pp.y + r + 4, 4.0f, 1.0f);
    // spectral ticks (10 bands) + unmask cuts
    for (int b = 0; b < kBands; ++b)
    {
        const float ang = -kPi * 0.5f + (float) b / (float) kBands * 2.0f * kPi;
        const float e = sat01 ((s.bandDb[(size_t) b].get() + 60.0f) / 52.0f);
        const float r0 = r + 4.0f, r1 = r0 + 3.0f + r * 1.3f * e;
        const float cx = std::cos (ang), sy = std::sin (ang);
        g.setColour (col.withAlpha (alpha * (0.35f + 0.65f * e)));
        g.drawLine (pp.x + cx * r0, pp.y + sy * r0, pp.x + cx * r1, pp.y + sy * r1, 2.0f);
        const float c = -s.cutDb[(size_t) b].get() / 9.0f;
        if (c > 0.04f) { g.setColour (psl::C::magenta.withAlpha (alpha)); g.drawLine (pp.x + cx * r1, pp.y + sy * r1, pp.x + cx * (r1 + 2 + r * c), pp.y + sy * (r1 + 2 + r * c), 3.0f); }
    }
    // body: flat square + crosshair
    g.setColour (col.withAlpha (alpha)); g.fillRect (pp.x - r, pp.y - r, r * 2, r * 2);
    g.setColour (psl::C::ink.withAlpha (alpha)); g.fillRect (pp.x - r * 0.35f, pp.y - 0.5f, r * 0.7f, 1.0f); g.fillRect (pp.x - 0.5f, pp.y - r * 0.35f, 1.0f, r * 0.7f);
    if (muted) psl::hatch (g, { pp.x - r, pp.y - r, r * 2, r * 2 }, psl::C::ink, 4);
    if (slot == selected)
    {
        const float q = r + 9 + 2 * std::sin (time * 3.0f);
        g.setColour (psl::C::yellow);
        for (int k = 0; k < 4; ++k)
        {
            const float sx = (k & 1) ? 1.0f : -1.0f, sy = (k & 2) ? 1.0f : -1.0f;
            g.fillRect (pp.x + sx * q - (sx > 0 ? 8.0f : 0.0f), pp.y + sy * q - (sy > 0 ? 2.0f : 0.0f), 8.0f, 2.0f);
            g.fillRect (pp.x + sx * q - (sx > 0 ? 2.0f : 0.0f), pp.y + sy * q - (sy > 0 ? 8.0f : 0.0f), 2.0f, 8.0f);
        }
    }
    else if (slot == hover) { g.setColour (psl::C::paper.withAlpha (0.6f)); g.drawRect (pp.x - r - 6, pp.y - r - 6, r * 2 + 12, r * 2 + 12, 1.0f); }

    // label tag: colour block + mono caps
    juce::String lbl = psl::upper (s.name) + "  P" + juce::String ((int) std::round (s.val[pPriority].get()));
    if (! fresh) lbl << "  (" << psl::upper (T (sSleeping)) << ")";
    const auto f = psl::mono (10.0f, true);
    const float tw = juce::GlyphArrangement::getStringWidth (f, lbl) + 12;
    int ly = (int) (pp.y + r + 10);
    juce::Rectangle<int> lr ((int) (pp.x - tw * 0.5f), ly, (int) tw, 15);
    for (int guard = 0; guard < 6; ++guard)
    {
        bool hit = false;
        for (auto& u : labelRects) if (u.intersects (lr)) { hit = true; break; }
        if (! hit) break;
        lr = lr.translated (0, 17);
    }
    labelRects.push_back (lr);
    g.setColour (psl::C::ink.withAlpha (0.85f * alpha)); g.fillRect (lr);
    g.setColour (col.withAlpha (alpha)); g.fillRect (lr.withWidth (4));
    psl::text (g, lbl, lr.toFloat().withTrimmedLeft (8), f, psl::C::paper.withAlpha (alpha));
}

void SkyView::paint (juce::Graphics& g)
{
    yaw += 0.18f * (yawTarget - yaw); pitch += 0.18f * (pitchTarget - pitch);
    fit();
    drawBackground (g);
    drawGrid (g);

    auto& reg = Registry::get();
    const uint32_t now = juce::Time::getMillisecondCounter();

    // listener + speakers
    {
        const float lz = listenerZ();
        auto hp = project (0, -0.15f, lz);
        const float hr = 13.0f * hp.scale;
        if (proc.getBus (false, 0) != nullptr && proc.getBus (false, 0)->getNumberOfChannels() > 2)
        {
            SpeakerLayout sl; sl.build (proc.getBus (false, 0)->getCurrentLayout());
            auto drawSpk = [&] (const Speaker& sp, float yy)
            {
                const float a = sp.az * kPi / 180.0f;
                auto q = project (std::sin (a) * 1.25f, yy, lz + std::cos (a) * 1.25f);
                g.setColour (psl::C::paper); g.fillRect (q.x - 6, q.y - 6, 12.0f, 12.0f);
                g.setColour (psl::C::ink); g.fillRect (q.x - 2, q.y - 2, 4.0f, 4.0f);
            };
            for (auto& sp : sl.ear) drawSpk (sp, -0.15f);
            for (auto& sp : sl.top) drawSpk (sp, 0.85f);
        }
        g.setColour (psl::C::yellow); g.drawEllipse (hp.x - hr, hp.y - hr, hr * 2, hr * 2, 2.0f);
        g.fillRect (hp.x - 2, hp.y - 2, 4.0f, 4.0f);
        auto nose = project (0, -0.15f, lz + 0.16f);
        g.drawLine (hp.x, hp.y, nose.x, nose.y, 2.0f);
        psl::tech (g, "LSTNR", { hp.x + hr + 4, hp.y - 6, 60, 12 }, psl::C::yellow, juce::Justification::centredLeft, 8.5f);
        if (reg.globals.binaural.load())
        {
            juce::Path ph; ph.addCentredArc (hp.x, hp.y, hr * 1.5f, hr * 1.5f, 0, -1.2f, 1.2f, true);
            g.setColour (Theme::pink()); g.strokePath (ph, juce::PathStrokeType (2.5f));
        }
    }

    // gather stars
    drawn.clear();
    std::vector<int> live;
    for (int t = 0; t < kMaxStars; ++t)
    {
        auto& s = reg.slot (t);
        if (s.active.load() == 0 || s.isSky.load()) continue;
        drawn.push_back ({ t, project (s.val[pX].get(), s.val[pY].get(), toWZ (s.val[pZ].get())) });
        if (s.fresh (now)) live.push_back (t);
    }

    // masking filaments
    for (size_t i = 0; i < live.size(); ++i)
        for (size_t j = i + 1; j < live.size(); ++j)
        {
            const float m = ConstellationProcessor::pairMasking (live[i], live[j]);
            if (m < 0.32f) continue;
            const float a = sat01 ((m - 0.32f) / 0.6f);
            P pa {}, pb {};
            for (auto& d : drawn) { if (d.first == live[i]) pa = d.second; if (d.first == live[j]) pb = d.second; }
            const float pulse = 0.6f + 0.4f * std::sin (time * 5.0f + (float) i);
            juce::Path ln; ln.startNewSubPath (pa.x, pa.y); ln.lineTo (pb.x, pb.y);
            juce::Path dd; const float dsh[] = { 6, 4 }; juce::PathStrokeType (1.0f + 2.5f * a).createDashedStroke (dd, ln, dsh, 2);
            g.setColour (psl::C::orange.withAlpha (0.35f + 0.6f * a * pulse)); g.fillPath (dd);
            const float t = std::fmod (time * 0.6f + 0.17f * (float) (i + j), 1.0f);
            g.setColour (psl::C::orange); g.fillRect (pa.x + (pb.x - pa.x) * t - 3.0f, pa.y + (pb.y - pa.y) * t - 3.0f, 6.0f, 6.0f);
            psl::tech (g, "CLASH " + juce::String (juce::roundToInt (m * 100)), { (pa.x + pb.x) * 0.5f - 30, (pa.y + pb.y) * 0.5f - 14, 60, 10 }, psl::C::orange, juce::Justification::centred, 8);
        }

    std::sort (drawn.begin(), drawn.end(), [] (auto& a, auto& b) { return a.second.depth > b.second.depth; });
    labelRects.clear();
    for (auto& d : drawn) drawStar (g, d.first, d.second, reg.slot (d.first).fresh (now));

    if (reg.globals.binaural.load())
    {
        auto bar = getLocalBounds().removeFromTop (30).reduced (juce::jmax (10, getWidth() / 2 - 330), 4).toFloat();
        g.setColour (psl::C::magenta); g.fillRect (bar);
        g.setColour (psl::C::ink); g.setFont (psl::ui (12.5f)); g.drawFittedText (psl::upper (T2 (xPhonesWarn)), bar.reduced (8, 0).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }

    if (drawn.empty())
    {
        g.setColour (Theme::dim()); g.setFont (F (15.0f, false));
        g.drawFittedText (T (sNoStars), getLocalBounds().reduced (60), juce::Justification::centred, 3);
    }
    g.setColour (Theme::line()); g.drawRect (getLocalBounds(), 1);
}

int SkyView::hitTest (juce::Point<float> pt) const
{
    int best = -1; float bd = 1e9f;
    for (auto& d : drawn)
    {
        const float r = 22.0f * juce::jlimit (0.6f, 1.6f, d.second.scale);
        const float dist = pt.getDistanceFrom ({ d.second.x, d.second.y });
        if (dist < r && dist < bd) { bd = dist; best = d.first; }
    }
    return best;
}

void SkyView::mouseMove (const juce::MouseEvent& e) { hover = hitTest (e.position); }

void SkyView::mouseDown (const juce::MouseEvent& e)
{
    lastMouse = e.position;
    dragging = hitTest (e.position);
    orbiting = dragging < 0;
    if (dragging >= 0) { selected = dragging; if (onSelect) onSelect (dragging); }
}

void SkyView::mouseDrag (const juce::MouseEvent& e)
{
    const auto d = e.position - lastMouse;
    lastMouse = e.position;
    if (orbiting)
    {
        if (view == v3D) { yawTarget += d.x * 0.008f; pitchTarget = juce::jlimit (0.0f, 1.55f, pitchTarget + d.y * 0.006f); yaw = yawTarget; pitch = pitchTarget; }
        return;
    }
    if (dragging < 0) return;
    auto& s = Registry::get().slot (dragging);
    if (e.mods.isAltDown())
    {
        ConstellationProcessor::setStarParam (dragging, pWidth, juce::jlimit (0.0f, 2.0f, s.val[pWidth].get() + d.x * 0.01f));
        return;
    }
    const float x = s.val[pX].get(), y = s.val[pY].get(), z = s.val[pZ].get();
    const float wz = toWZ (z);
    // choose the two axes for this view
    int axB = 2; // 1 = y, 2 = z
    if (view == vFront || (view == v3D && (e.mods.isShiftDown() || e.mods.isRightButtonDown()))) axB = 1;
    const auto p0 = project (x, y, wz);
    const auto pxp = project (x + 0.05f, y, wz);
    const auto pbp = axB == 1 ? project (x, y + 0.05f, wz) : project (x, y, wz + 0.05f);
    const float ax = (pxp.x - p0.x) / 0.05f, ay = (pxp.y - p0.y) / 0.05f;
    const float bx = (pbp.x - p0.x) / 0.05f, by = (pbp.y - p0.y) / 0.05f;
    const float det = ax * by - ay * bx;
    if (std::abs (det) < 1e-3f) return;
    const float da = (d.x * by - d.y * bx) / det, db = (ax * d.y - ay * d.x) / det;
    ConstellationProcessor::setStarParam (dragging, pX, juce::jlimit (-1.0f, 1.0f, x + da));
    if (axB == 1) ConstellationProcessor::setStarParam (dragging, pY, juce::jlimit (-1.0f, 1.0f, y + db));
    else          ConstellationProcessor::setStarParam (dragging, pZ, juce::jlimit (0.0f, 1.0f, z + db * 0.5f));
}

void SkyView::mouseUp (const juce::MouseEvent&) { dragging = -1; orbiting = false; }

void SkyView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hitTest (e.position);
    if (h < 0) { setView (view); return; }
    ConstellationProcessor::setStarParam (h, pX, 0.0f);
    ConstellationProcessor::setStarParam (h, pY, 0.0f);
    ConstellationProcessor::setStarParam (h, pZ, 0.25f);
    ConstellationProcessor::setStarParam (h, pWidth, 1.0f);
}

void SkyView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int h = hitTest (e.position);
    if (h < 0) return;
    auto& s = Registry::get().slot (h);
    if (e.mods.isShiftDown()) ConstellationProcessor::setStarParam (h, pGain, juce::jlimit (-36.0f, 12.0f, s.val[pGain].get() + w.deltaY * 6.0f));
    else                      ConstellationProcessor::setStarParam (h, pWidth, juce::jlimit (0.0f, 2.0f, s.val[pWidth].get() + w.deltaY * 0.6f));
}

// ───────────────────────────── meters ─────────────────────────────
void BandMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::bg().withAlpha (0.6f)); g.fillRect (r);
    g.setColour (Theme::line()); g.drawRect (r, 1.0f);
    if (slot < 0) return;
    auto& s = Registry::get().slot (slot);
    const auto col = juce::Colour (s.colour);
    auto inner = r.reduced (10, 18);
    const float bw = inner.getWidth() / (float) kBands;
    static const char* lab[kBands] = { "50", "100", "200", "400", "800", "1.6k", "3k", "6k", "10k", "15k" };
    for (int b = 0; b < kBands; ++b)
    {
        const float e = sat01 ((s.bandDb[(size_t) b].get() + 60.0f) / 56.0f);
        const float x = inner.getX() + bw * (float) b + 2;
        const float hBar = inner.getHeight() * e;
        psl::segBar (g, { x, inner.getY(), bw - 4, inner.getHeight() }, e, col, psl::C::ink3, 14);
        juce::ignoreUnused (hBar);
        const float c = sat01 (-s.cutDb[(size_t) b].get() / 9.0f);
        if (c > 0.01f) { g.setColour (Theme::danger().withAlpha (0.85f)); g.fillRect (x, inner.getY(), bw - 4, inner.getHeight() * c * 0.5f); }
        g.setColour (Theme::dim()); g.setFont (F (9.0f, false));
        g.drawText (lab[b], (int) x - 4, (int) inner.getBottom() + 2, (int) bw + 4, 12, juce::Justification::centred);
    }
    psl::tech (g, T (sBands) + " / " + T (sCuts), r.reduced (10, 3).withHeight (12), Theme::dim(), juce::Justification::centredLeft, 8.5f);
}

void MasterMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::bg().withAlpha (0.6f)); g.fillRect (r);
    g.setColour (Theme::line()); g.drawRect (r, 1.0f);
    const uint32_t now = juce::Time::getMillisecondCounter();
    int n = 0; for (int t = 0; t < kMaxStars; ++t) { auto& s = Registry::get().slot (t); if (s.fresh (now) && ! s.isSky.load()) ++n; }
    psl::text (g, juce::String (n).paddedLeft ('0', 2), r.removeFromLeft (50), psl::display (34), Theme::gold(), juce::Justification::centred);
    g.setColour (Theme::dim()); g.setFont (F (10.0f));
    auto top = r.removeFromTop (r.getHeight() * 0.42f);
    g.drawText (T (sStarsOnline), top.toNearestInt(), juce::Justification::centredLeft);
    if (! proc.isSkyMode()) return;
    auto bars = r.reduced (2, 4);
    auto bar = [&] (float db, juce::Rectangle<float> b)
    {
        const float v = sat01 ((db + 60.0f) / 60.0f);
        g.setColour (Theme::line()); g.fillRect (b);
        g.setColour (db > -1.0f ? Theme::danger() : Theme::cyan()); g.fillRect (b.withWidth (b.getWidth() * v));
    };
    auto half = bars.removeFromLeft (bars.getWidth() * 0.55f);
    bar (proc.masterL.load(), half.removeFromTop (half.getHeight() * 0.5f).reduced (0, 2));
    bar (proc.masterR.load(), half.reduced (0, 2));
    const float c = juce::jlimit (-1.0f, 1.0f, proc.correlation.load());
    auto cr = bars.reduced (8, 2);
    g.setColour (Theme::dim()); g.drawText (T (sCorr), cr.removeFromLeft (30).toNearestInt(), juce::Justification::centredLeft);
    g.setColour (Theme::line()); g.fillRect (cr.withSizeKeepingCentre (cr.getWidth(), 4));
    const float cx = cr.getX() + (c + 1.0f) * 0.5f * cr.getWidth();
    g.setColour (c < 0 ? Theme::danger() : Theme::gold()); g.fillRect (cx - 3, cr.getCentreY() - 7, 6.0f, 14.0f);
}

// ───────────────────────────── Editor ─────────────────────────────
ConstellationEditor::ConstellationEditor (ConstellationProcessor& p)
    : AudioProcessorEditor (p), proc (p), sky (p), meter (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (sky);
    addAndMakeVisible (bands);
    addAndMakeVisible (meter);
    sky.onSelect = [this] (int s) { select (s); };

    auto tb = [this] (juce::TextButton& b, juce::Colour acc, bool toggle = false)
    {
        b.setColour (juce::TextButton::buttonOnColourId, acc);
        b.setClickingTogglesState (toggle);
        addAndMakeVisible (b);
    };
    tb (modeStar, psl::C::paper); tb (modeSky, Theme::gold());
    tb (view3D, psl::C::paper); tb (viewTop, psl::C::paper); tb (viewFront, psl::C::paper);
    tb (phones, Theme::pink()); tb (langBtn, Theme::cyan()); tb (helpBtn, Theme::gold());
    tb (applyStyle, Theme::cyan()); tb (untangleBtn, Theme::danger()); tb (monoBtn, Theme::gold());
    tb (muteBtn, Theme::dim()); tb (soloBtn, Theme::gold()); tb (storeBtn, Theme::danger());
    for (auto& b : sceneBtn) tb (b, Theme::gold());
    tb (donate, Theme::pink());
    donate.setColour (juce::TextButton::textColourOffId, Theme::pink().withAlpha (0.9f));

    modeStar.onClick = [this] { proc.setSkyMode (false); select (proc.getSlot()); resized(); };
    modeSky.onClick  = [this] { proc.setSkyMode (true); resized(); };
    view3D.onClick    = [this] { sky.setView (SkyView::v3D); };
    viewTop.onClick   = [this] { sky.setView (SkyView::vTop); };
    viewFront.onClick = [this] { sky.setView (SkyView::vFront); };
    phones.onClick = [this] { auto& g = Registry::get().globals; g.binaural.store (g.binaural.load() ? 0 : 1); };
    langBtn.onClick = [this] { auto& g = Registry::get().globals; g.lang.store (g.lang.load() ? 0 : 1); };
    helpBtn.onClick = [this] { help->setVisible (! help->isVisible()); help->toFront (false); };
    donate.onClick = [] { juce::URL (kDonateUrl).launchInDefaultBrowser(); };
    applyStyle.onClick = [this] { proc.applyStyle (styleBox.getSelectedItemIndex()); };
    untangleBtn.onClick = [this] { proc.untangle(); };
    monoBtn.onClick = [this] { auto& g = Registry::get().globals; g.monoCheck.store (g.monoCheck.load() ? 0 : 1); };
    muteBtn.onClick = [this] { if (selected >= 0) ConstellationProcessor::setStarParam (selected, pMute, Registry::get().slot (selected).val[pMute].get() > 0.5f ? 0.0f : 1.0f); };
    soloBtn.onClick = [this] { if (selected >= 0) ConstellationProcessor::setStarParam (selected, pSolo, Registry::get().slot (selected).val[pSolo].get() > 0.5f ? 0.0f : 1.0f); };
    storeBtn.onClick = [this] { storeArmed = ! storeArmed; };
    for (int i = 0; i < 4; ++i)
    {
        sceneBtn[i].setButtonText (juce::String::charToString ((juce::juce_wchar) ('A' + i)));
        sceneBtn[i].onClick = [this, i]
        {
            if (storeArmed) { proc.storeScene (i); storeArmed = false; }
            else proc.recallScene (i);
        };
    }

    addAndMakeVisible (room);
    for (int i = 0; i < kNumRooms; ++i) room.addItem (kRooms[i].en, i + 1);
    room.onChange = [this] { if (! updating) Registry::get().globals.room.store (room.getSelectedItemIndex()); };
    addAndMakeVisible (roleBox);
    roleBox.onChange = [this] { if (! updating && selected >= 0) ConstellationProcessor::setStarParam (selected, pRole, (float) roleBox.getSelectedItemIndex()); };
    addAndMakeVisible (styleBox);

    auto lin = [this] (juce::Slider& s, juce::Label& l, double lo, double hi, juce::Colour c)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRange (lo, hi, 0.0);
        s.setColour (juce::Slider::trackColourId, c);
        addAndMakeVisible (s);
        l.setFont (F (11.0f)); l.setColour (juce::Label::textColourId, Theme::dim());
        addAndMakeVisible (l);
    };
    lin (wrapSlider, wrapLabel, 0.0, 1.0, Theme::pink());
    wrapSlider.onValueChange = [this] { if (! updating) Registry::get().globals.wrap.set ((float) wrapSlider.getValue()); };
    lin (morphSlider, morphLabel, 0.0, 1.0, Theme::gold());
    morphAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "morph", morphSlider);
    lin (prioSlider, prioLabel, 1.0, 10.0, Theme::gold());
    prioSlider.setRange (1.0, 10.0, 1.0);
    prioSlider.onValueChange = [this] { if (! updating && selected >= 0) ConstellationProcessor::setStarParam (selected, pPriority, (float) prioSlider.getValue()); };

    for (auto* l : { &nameLabel, &hintLabel, &scenesLabel, &autoLabel }) { addAndMakeVisible (*l); l->setColour (juce::Label::textColourId, Theme::dim()); l->setFont (F (11.0f)); }
    nameLabel.setFont (F (18.0f)); nameLabel.setColour (juce::Label::textColourId, Theme::text());
    hintLabel.setFont (F (11.0f, false)); hintLabel.setJustificationType (juce::Justification::topLeft);

    struct Def { int param; double lo, hi, def; juce::uint32 c; };
    const Def defs[9] = { { pGain, -36, 12, 0, 0xff58e1ff }, { pWidth, 0, 2, 1, 0xffff4fd8 }, { pTone, -1, 1, 0, 0xffffd166 },
                          { pX, -1, 1, 0, 0xff58e1ff }, { pY, -1, 1, 0, 0xff7affc1 }, { pZ, 0, 1, 0.25, 0xff9a7cff },
                          { pSpace, 0, 1, 0.25, 0xff9ad0ff }, { pLowCut, 20, 800, 20, 0xffffa06a }, { pUnmask, 0, 1, 0.5, 0xffff5a4e } };
    for (int i = 0; i < 9; ++i)
    {
        auto& k = knobs[(size_t) i];
        k.param = defs[i].param;
        k.s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        k.s.setRange (defs[i].lo, defs[i].hi, 0.0);
        if (k.param == pLowCut) k.s.setSkewFactor (0.35);
        k.s.setDoubleClickReturnValue (true, defs[i].def);
        k.s.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (defs[i].c));
        k.s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.s.setColour (juce::Slider::textBoxTextColourId, Theme::text());
        k.s.setNumDecimalPlacesToDisplay (k.param == pLowCut ? 0 : 2);
        if (k.param == pGain) k.s.setTextValueSuffix (" dB");
        if (k.param == pLowCut) k.s.setTextValueSuffix (" Hz");
        const int prm = k.param;
        k.s.onValueChange = [this, i, prm] { if (! updating && selected >= 0) ConstellationProcessor::setStarParam (selected, prm, (float) knobs[(size_t) i].s.getValue()); };
        addAndMakeVisible (k.s);
        k.l.setJustificationType (juce::Justification::centred);
        k.l.setFont (F (11.0f)); k.l.setColour (juce::Label::textColourId, Theme::dim());
        addAndMakeVisible (k.l);
    }

    help = std::make_unique<HelpOverlay>();
    addChildComponent (*help);

    // surround export (SKY)
    tb (recBtn, Theme::danger());
    addAndMakeVisible (recFormat);
    for (int f = 0; f < 3; ++f) recFormat.addItem (recFormatName (f), f + 1);
    recFormat.setSelectedItemIndex (Registry::get().globals.recFormat.load(), juce::dontSendNotification);
    for (auto* l : { &recLabel, &exportLabel }) { addAndMakeVisible (*l); l->setFont (F (11.0f)); l->setColour (juce::Label::textColourId, Theme::dim()); }
    recLabel.setFont (F (10.5f, false));
    recLabel.setInterceptsMouseClicks (true, false);
    recLabel.addMouseListener (this, false);
    recBtn.onClick = [this]
    {
        if (proc.getRecState() == ConstellationProcessor::recIdle) proc.armRecorder (recFormat.getSelectedItemIndex());
        else proc.stopRecorder();
        refreshRec();
    };

    // licence
    tb (licBtn, Theme::gold());
    licBtn.onClick = [this] { showLicence(); };
    auto lp = std::make_unique<LicencePanel>();
    lp->onDone = [this] { refreshTexts(); };
    licPanel = std::move (lp);
    addChildComponent (*licPanel);
    if (ConstellationProcessor::licence().expired()) juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<ConstellationEditor> (this)] { if (sp) sp->showLicence(); });

    select (proc.isSkyMode() ? -1 : proc.getSlot());
    refreshTexts();
    setResizable (true, true);
    setResizeLimits (1180, 660, 2200, 1400);
    setSize (1300, 760);
    startTimerHz (30);
}

ConstellationEditor::~ConstellationEditor() { stopTimer(); setLookAndFeel (nullptr); }

void ConstellationEditor::showLicence()
{
    static_cast<LicencePanel*> (licPanel.get())->refreshTexts();
    licPanel->setBounds (getLocalBounds());
    licPanel->setVisible (true);
    licPanel->toFront (true);
}

void ConstellationEditor::refreshRec()
{
    const int st = proc.getRecState();
    recBtn.setToggleState (st != ConstellationProcessor::recIdle, juce::dontSendNotification);
    const bool blink = st == ConstellationProcessor::recRunning && (juce::Time::getMillisecondCounter() / 500) % 2 == 0;
    recBtn.setButtonText (juce::String::fromUTF8 (blink ? "\xe2\x97\x8f " : "\xe2\x97\x8b ") + T2 (xRec));
    recFormat.setEnabled (st == ConstellationProcessor::recIdle);
    juce::String txt;
    if (st == ConstellationProcessor::recWaiting) txt = T2 (xArmed);
    else if (st == ConstellationProcessor::recRunning)
    {
        const int secs = (int) proc.getRecSeconds();
        txt = T2 (xRecording) + "  " + juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2);
    }
    else if (proc.lastRecording.existsAsFile()) txt = T2 (xSaved);
    else
    {
        const auto tn = proc.getTrackName().toLowerCase();
        txt = (tn == "master" || tn == "main" || tn == "track") ? juce::String() : T2 (xMasterHint);
    }
    recLabel.setText (txt, juce::dontSendNotification);
    recLabel.setColour (juce::Label::textColourId, st == ConstellationProcessor::recRunning ? Theme::danger() : (st == ConstellationProcessor::recWaiting ? Theme::gold() : Theme::dim()));
    auto& L = ConstellationProcessor::licence();
    licBtn.setVisible (! L.licensed.load());
    licBtn.setButtonText (L.expired() ? T2 (xBuy) : T2 (xTrial).replace ("%d", juce::String (L.daysLeft.load())));
    licBtn.setToggleState (L.expired(), juce::dontSendNotification);
}

void ConstellationEditor::select (int slot)
{
    selected = slot;
    sky.selected = slot;
    bands.slot = slot;
    refreshPanel();
}

void ConstellationEditor::refreshTexts()
{
    const bool es = Registry::get().globals.lang.load() == 1;
    lastLang = es ? 1 : 0;
    modeStar.setButtonText (juce::String::fromUTF8 ("\xe2\x98\x85 ") + T (sStar));
    modeSky.setButtonText (juce::String::fromUTF8 ("\xe2\x9c\xa6 ") + T (sSky));
    view3D.setButtonText (T (sView3D)); viewTop.setButtonText (T (sViewTop)); viewFront.setButtonText (T (sViewFront));
    phones.setButtonText (T (sPhones));
    langBtn.setButtonText (T (sLang)); helpBtn.setButtonText (T (sHelp));
    applyStyle.setButtonText (T (sApply)); untangleBtn.setButtonText (T (sUntangle)); monoBtn.setButtonText (T (sMono));
    muteBtn.setButtonText (T (sMute)); soloBtn.setButtonText (T (sSolo)); storeBtn.setButtonText (T (sStore));
    donate.setButtonText (T (sDonate));
    wrapLabel.setText (T (sWrap), juce::dontSendNotification);
    morphLabel.setText (T (sMorph), juce::dontSendNotification);
    prioLabel.setText (T (sPriority), juce::dontSendNotification);
    scenesLabel.setText (T (sScenes), juce::dontSendNotification);
    autoLabel.setText (T (sAutoMix), juce::dontSendNotification);
    exportLabel.setText (T2 (xExport), juce::dontSendNotification);
    if (licPanel) static_cast<LicencePanel*> (licPanel.get())->refreshTexts();
    const int knobKeys[9] = { sGain, sWidth, sTone, sPan, sHeight, sDepth, sSpace, sLowCut, sUnmask };
    for (int i = 0; i < 9; ++i) knobs[(size_t) i].l.setText (T (knobKeys[i]), juce::dontSendNotification);

    updating = true;
    const int ri = room.getSelectedItemIndex(), rr = roleBox.getSelectedItemIndex(), si = juce::jmax (0, styleBox.getSelectedItemIndex());
    room.clear (juce::dontSendNotification); roleBox.clear (juce::dontSendNotification); styleBox.clear (juce::dontSendNotification);
    for (int i = 0; i < kNumRooms; ++i) room.addItem (juce::String::fromUTF8 (es ? kRooms[i].es : kRooms[i].en), i + 1);
    for (int i = 0; i < kNumRoles; ++i) roleBox.addItem (juce::String::fromUTF8 (es ? roleNameEs[i] : roleNameEn[i]), i + 1);
    for (int i = 0; i < kNumStyles; ++i) styleBox.addItem (juce::String::fromUTF8 (es ? styleNameEs[i] : styleNameEn[i]), i + 1);
    room.setSelectedItemIndex (ri >= 0 ? ri : Registry::get().globals.room.load(), juce::dontSendNotification);
    if (rr >= 0) roleBox.setSelectedItemIndex (rr, juce::dontSendNotification);
    styleBox.setSelectedItemIndex (si, juce::dontSendNotification);
    updating = false;
    hintLabel.setText (proc.isSkyMode() ? T (sSkyHint) : T (sStarHint), juce::dontSendNotification);
    if (help) help->repaint();
    repaint();
}

void ConstellationEditor::refreshPanel()
{
    auto& reg = Registry::get();
    updating = true;
    const bool valid = selected >= 0 && reg.slot (selected).active.load() && ! reg.slot (selected).isSky.load();
    for (auto& k : knobs)
    {
        k.s.setEnabled (valid);
        if (valid && ! k.s.isMouseButtonDown()) k.s.setValue (reg.slot (selected).val[(size_t) k.param].get(), juce::dontSendNotification);
    }
    prioSlider.setEnabled (valid); roleBox.setEnabled (valid); muteBtn.setEnabled (valid); soloBtn.setEnabled (valid);
    if (valid)
    {
        auto& s = reg.slot (selected);
        if (! prioSlider.isMouseButtonDown()) prioSlider.setValue (std::round (s.val[pPriority].get()), juce::dontSendNotification);
        roleBox.setSelectedItemIndex ((int) std::round (s.val[pRole].get()), juce::dontSendNotification);
        muteBtn.setToggleState (s.val[pMute].get() > 0.5f, juce::dontSendNotification);
        soloBtn.setToggleState (s.val[pSolo].get() > 0.5f, juce::dontSendNotification);
        nameLabel.setText (s.name, juce::dontSendNotification);
    }
    else nameLabel.setText ("-", juce::dontSendNotification);
    auto& g = reg.globals;
    if (! wrapSlider.isMouseButtonDown()) wrapSlider.setValue (g.wrap.get(), juce::dontSendNotification);
    room.setSelectedItemIndex (g.room.load(), juce::dontSendNotification);
    phones.setToggleState (g.binaural.load() != 0, juce::dontSendNotification);
    monoBtn.setToggleState (g.monoCheck.load() != 0, juce::dontSendNotification);
    storeBtn.setToggleState (storeArmed, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i) sceneBtn[i].setToggleState (proc.sceneStored (i), juce::dontSendNotification);
    modeStar.setToggleState (! proc.isSkyMode(), juce::dontSendNotification);
    modeSky.setToggleState (proc.isSkyMode(), juce::dontSendNotification);
    view3D.setToggleState (sky.view == SkyView::v3D, juce::dontSendNotification);
    viewTop.setToggleState (sky.view == SkyView::vTop, juce::dontSendNotification);
    viewFront.setToggleState (sky.view == SkyView::vFront, juce::dontSendNotification);
    updating = false;
}

void ConstellationEditor::timerCallback()
{
    sky.time += 1.0f / 30.0f;
    auto& reg = Registry::get();
    const uint32_t now = juce::Time::getMillisecondCounter();
    if (selected < 0 || ! reg.slot (selected).active.load() || reg.slot (selected).isSky.load())
    {
        int pick = -1;
        if (! proc.isSkyMode() && proc.getSlot() >= 0) pick = proc.getSlot();
        else for (int t = 0; t < kMaxStars; ++t) if (reg.slot (t).fresh (now) && ! reg.slot (t).isSky.load()) { pick = t; break; }
        if (pick != selected) select (pick);
    }
    if (lastLang != reg.globals.lang.load()) refreshTexts();
    const bool skyMode = proc.isSkyMode();
    if (skyMode != storeBtn.isVisible()) { resized(); refreshTexts(); }
    refreshPanel();
    refreshRec();
    sky.repaint(); bands.repaint(); meter.repaint();
    repaint (getLocalBounds().removeFromRight (310).removeFromTop (120));
}

void ConstellationEditor::paint (juce::Graphics& g)
{
    using namespace psl;
    const auto R = getLocalBounds().toFloat();
    g.fillAll (C::ink);
    // top band: flat cyan, condensed title, katakana, codes
    g.setColour (C::cyan); g.fillRect (R.withHeight (56));
    juce::Path star;   // 4-point polaris star, hard edges
    const float cx = 26, cy = 28;
    for (int i = 0; i < 8; ++i) { const float a = (float) i * kPi / 4.0f - kPi / 2.0f, rr = (i % 2 == 0) ? 17.0f : 4.0f; if (i == 0) star.startNewSubPath (cx + std::cos (a) * rr, cy + std::sin (a) * rr); else star.lineTo (cx + std::cos (a) * rr, cy + std::sin (a) * rr); }
    star.closeSubPath(); g.setColour (C::ink); g.fillPath (star);
    text (g, "CONSTELLATION", { 48, 3, 300, 50 }, display (42), C::ink);
    const float tx = 52.0f + juce::GlyphArrangement::getStringWidth (display (42), "CONSTELLATION") + 8.0f;
    text (g, juce::String::fromUTF8 ("コンステレーション"), { tx, 6, 140, 18 }, jp (12), C::ink);
    tech (g, "3D.MIX // SURROUND", { tx, 24, 150, 12 }, C::ink, juce::Justification::centredLeft, 8);
    tech (g, "PSL // V1.1.0", { tx, 36, 150, 12 }, C::ink, juce::Justification::centredLeft, 8);
    g.setColour (C::ink); g.fillRect (0.0f, 56.0f, R.getWidth(), 2.0f);

    // right panel
    auto panel = getLocalBounds().withTrimmedTop (58).removeFromRight (310).withTrimmedBottom (122).toFloat();
    g.setColour (C::ink2); g.fillRect (panel);
    g.setColour (C::rule); g.fillRect (panel.getX(), panel.getY(), 1.0f, panel.getHeight());
    if (selected >= 0)
    {
        const auto col = juce::Colour (Registry::get().slot (selected).colour);
        g.setColour (col); g.fillRect (panel.getX() + 14, panel.getY() + 18, 22.0f, 22.0f);
        g.setColour (C::ink); g.fillRect (panel.getX() + 24, panel.getY() + 22, 2.0f, 14.0f); g.fillRect (panel.getX() + 18, panel.getY() + 28, 14.0f, 2.0f);
    }
    sectionTab (g, { panel.getX() + 14, panel.getY() + 2, panel.getWidth() - 28, 13 }, "01", proc.isSkyMode() ? T (sSelected) : T (sStar), juce::String::fromUTF8 ("星"), C::cyan);
    tech (g, T (sRole), { panel.getX() + 14, panel.getY() + 62, 60, 24 }, C::grey, juce::Justification::centredLeft, 9);

    // bottom band
    auto bottom = R.withTop (R.getBottom() - 122);
    g.setColour (C::ink2); g.fillRect (bottom);
    g.setColour (C::cyan); g.fillRect (bottom.withHeight (2));
    auto foot = R.withTop (R.getBottom() - 24);
    g.setColour (C::ink); g.fillRect (foot);
    tech (g, T (sFooter), foot.reduced (14, 0).withTrimmedRight (330), C::grey, juce::Justification::centredLeft, 9);
    barcode (g, { foot.getRight() - 470, foot.getY() + 6, 120, 12 }, 31, C::rule.brighter (0.4f));
}

void ConstellationEditor::paintOverChildren (juce::Graphics&) {}

void ConstellationEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == &recLabel && proc.lastRecording.existsAsFile()) proc.lastRecording.revealToUser();
}

void ConstellationEditor::resized()
{
    const bool skyMode = proc.isSkyMode();
    auto r = getLocalBounds();
    auto top = r.removeFromTop (58).reduced (8, 12);
    top.removeFromLeft (440);
    modeStar.setBounds (top.removeFromLeft (104)); top.removeFromLeft (4);
    modeSky.setBounds (top.removeFromLeft (92)); top.removeFromLeft (10);
    view3D.setBounds (top.removeFromLeft (40)); top.removeFromLeft (3);
    viewTop.setBounds (top.removeFromLeft (60)); top.removeFromLeft (3);
    viewFront.setBounds (top.removeFromLeft (66));
    helpBtn.setBounds (top.removeFromRight (34)); top.removeFromRight (5);
    langBtn.setBounds (top.removeFromRight (40)); top.removeFromRight (5);
    phones.setBounds (top.removeFromRight (116)); top.removeFromRight (6);
    room.setBounds (top.removeFromRight (150));
    top.removeFromRight (8);
    if (top.getWidth() > 60) licBtn.setBounds (top.removeFromRight (juce::jmin (170, top.getWidth())));
    if (licPanel) licPanel->setBounds (getLocalBounds());

    auto bottom = r.removeFromBottom (122);
    auto footer = bottom.removeFromBottom (26);
    donate.setBounds (footer.removeFromRight (300).reduced (8, 3));
    auto b = bottom.reduced (12, 10);

    auto col = [&] (int w) { auto c = b.removeFromLeft (w); b.removeFromLeft (14); return c; };
    storeBtn.setVisible (skyMode); scenesLabel.setVisible (skyMode); morphSlider.setVisible (skyMode); morphLabel.setVisible (skyMode); monoBtn.setVisible (skyMode);
    for (auto& s : sceneBtn) s.setVisible (skyMode);
    if (skyMode)
    {
        auto sc = col (250);
        scenesLabel.setBounds (sc.removeFromTop (18));
        auto row = sc.removeFromTop (34);
        for (auto& s : sceneBtn) { s.setBounds (row.removeFromLeft (40)); row.removeFromLeft (4); }
        storeBtn.setBounds (row.reduced (0, 0));
        auto mo = sc.removeFromTop (30);
        morphLabel.setBounds (mo.removeFromLeft (52)); morphSlider.setBounds (mo);
    }
    {
        auto am = col (250);
        autoLabel.setBounds (am.removeFromTop (18));
        auto row = am.removeFromTop (34);
        applyStyle.setBounds (row.removeFromRight (70)); row.removeFromRight (4);
        styleBox.setBounds (row);
        untangleBtn.setBounds (am.removeFromTop (30).withTrimmedTop (4));
    }
    {
        auto wr = col (skyMode ? 190 : 220);
        wrapLabel.setBounds (wr.removeFromTop (18));
        wrapSlider.setBounds (wr.removeFromTop (34));
        if (skyMode) monoBtn.setBounds (wr.removeFromTop (30).withTrimmedTop (4).removeFromLeft (90));
    }
    for (juce::Component* c : { (juce::Component*) &recBtn, (juce::Component*) &recFormat, (juce::Component*) &recLabel, (juce::Component*) &exportLabel }) c->setVisible (skyMode);
    if (skyMode)
    {
        auto ex = col (230);
        exportLabel.setBounds (ex.removeFromTop (18));
        auto row = ex.removeFromTop (34);
        recFormat.setBounds (row.removeFromLeft (92)); row.removeFromLeft (6);
        recBtn.setBounds (row);
        recLabel.setBounds (ex.removeFromTop (34));
    }
    meter.setBounds (b.removeFromLeft (juce::jmin (260, b.getWidth())));

    auto panel = r.removeFromRight (310).reduced (16, 12);
    sky.setBounds (r);
    if (help) help->setBounds (getLocalBounds());
    auto head = panel.removeFromTop (40);
    head.removeFromLeft (34);
    nameLabel.setBounds (head);
    auto roleRow = panel.removeFromTop (30).withTrimmedLeft (44);
    roleBox.setBounds (roleRow);
    panel.removeFromTop (8);
    const int kw = panel.getWidth() / 3, kh = 98;
    for (int i = 0; i < 9; ++i)
    {
        auto cell = juce::Rectangle<int> (panel.getX() + (i % 3) * kw, panel.getY() + (i / 3) * kh, kw, kh);
        knobs[(size_t) i].l.setBounds (cell.removeFromTop (14));
        knobs[(size_t) i].s.setBounds (cell);
    }
    panel.removeFromTop (kh * 3 + 6);
    auto pr = panel.removeFromTop (26);
    prioLabel.setBounds (pr.removeFromLeft (70)); prioSlider.setBounds (pr);
    auto ms = panel.removeFromTop (34).withTrimmedTop (4);
    muteBtn.setBounds (ms.removeFromLeft (ms.getWidth() / 2 - 3)); ms.removeFromLeft (6); soloBtn.setBounds (ms);
    panel.removeFromTop (8);
    bands.setBounds (panel.removeFromTop (juce::jmax (70, juce::jmin (120, panel.getHeight() - 44))));
    hintLabel.setBounds (panel.withTrimmedTop (4));
}
