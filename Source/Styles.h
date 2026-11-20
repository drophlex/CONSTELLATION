// CONSTELLATION — roles, auto-detection from DAW track names, one-click mix styles.
#pragma once
#include <juce_core/juce_core.h>

namespace cst
{
enum Role { rAuto = 0, rKick, rSnare, rHats, rPerc, rBass, rLeadVox, rBackVox, rLeadSynth, rPad, rKeys, rGuitar, rStrings, rFx, rOther, kNumRoles };

inline const char* roleNameEn[kNumRoles] = { "Auto", "Kick", "Snare / Clap", "Hats / Cymbals", "Percussion", "Bass", "Lead Vocal",
    "Backing Vocals", "Lead Synth", "Pad", "Keys / Piano", "Guitar", "Strings / Brass", "FX / Atmos", "Other" };
inline const char* roleNameEs[kNumRoles] = { "Auto", "Bombo", "Caja / Clap", "Hats / Platillos", "Percusión", "Bajo", "Voz principal",
    "Coros", "Sinte principal", "Pad", "Teclados / Piano", "Guitarra", "Cuerdas / Metales", "FX / Ambiente", "Otro" };

inline const uint32_t roleColour[kNumRoles] = { 0xff9aa7ff, 0xffff4d6d, 0xffffb347, 0xfffff275, 0xffc1ff72, 0xff6a5cff, 0xffff7ad9,
    0xfff7a8ff, 0xff4df3ff, 0xff7affc1, 0xff9ad0ff, 0xffffa06a, 0xffe0c3ff, 0xffb0ffef, 0xffcfd6ff };

inline int detectRole (const juce::String& trackName)
{
    const auto n = trackName.toLowerCase();
    auto has = [&n] (std::initializer_list<const char*> keys) { for (auto k : keys) if (n.contains (k)) return true; return false; };
    if (has ({ "kick", "bombo", "kik", "bd " , "808 kick" }) || n == "bd") return rKick;
    if (has ({ "snare", "clap", "caja", "rim", "snr" }) || n == "sd") return rSnare;
    if (has ({ "hat", "hh", "cymbal", "ride", "crash", "platillo", "oh " })) return rHats;
    if (has ({ "perc", "conga", "bongo", "shaker", "tom", "tamb", "guiro", "güiro", "clave", "cowbell", "timbal" })) return rPerc;
    if (has ({ "bass", "bajo", "sub", "808", "reese" })) return rBass;
    if (has ({ "backing", "coro", "choir", "harmony", "bv", "adlib", "dobles" })) return rBackVox;
    if (has ({ "vox", "vocal", "voz", "voice", "rap", "mc ", "canto" })) return rLeadVox;
    if (has ({ "pad", "drone", "texture", "textura" })) return rPad;
    if (has ({ "piano", "keys", "rhodes", "teclado", "organ", "órgano", "organo", "wurli", "epiano" })) return rKeys;
    if (has ({ "guit", "gtr", "guitarra", "requinto" })) return rGuitar;
    if (has ({ "string", "violin", "cello", "cuerda", "brass", "horn", "sax", "trump", "trompeta", "metal", "orch" })) return rStrings;
    if (has ({ "fx", "sfx", "riser", "noise", "impact", "efecto", "ambient", "atmos", "sweep", "foley" })) return rFx;
    if (has ({ "lead", "arp", "synth", "sint", "pluck", "stab", "acid", "303" })) return rLeadSynth;
    return rOther;
}

struct Place { float x, y, z, width, priority, space, tone; };

enum Style { sClub = 0, sFunk, sHipHop, sAmbient, sBand, sPop, sImmersive, kNumStyles };
inline const char* styleNameEn[kNumStyles] = { "Club / Techno", "Funk / Disco", "Hip-Hop / Trap", "Ambient / Cinematic", "Live Band", "Wide Pop", "Immersive 360°" };
inline const char* styleNameEs[kNumStyles] = { "Club / Techno", "Funk / Disco", "Hip-Hop / Trap", "Ambient / Cinemático", "Banda en vivo", "Pop amplio", "Inmersivo 360°" };

inline Place stylePlace (int style, int role)
{
    //                          x      y      z    width prio space tone
    static const Place club[kNumRoles] = {
        { 0, 0, .5f, 1, 5, .3f, 0 },       { 0, -.65f, .04f, .15f, 10, .03f, 0 }, { 0, .05f, .22f, .6f, 7, .22f, .1f },
        { .38f, .75f, .35f, .8f, 5, .18f, .1f }, { -.48f, .35f, .45f, .9f, 4, .3f, 0 }, { 0, -.85f, .08f, .25f, 9, .03f, -.1f },
        { 0, .2f, .1f, .5f, 10, .22f, .1f }, { .6f, .35f, .5f, 1.4f, 5, .45f, 0 }, { .22f, .45f, .25f, 1.0f, 8, .3f, .1f },
        { 0, .3f, .82f, 1.8f, 2, .65f, -.1f }, { -.35f, .1f, .45f, 1.1f, 5, .3f, 0 }, { .5f, .1f, .4f, .9f, 5, .3f, 0 },
        { 0, .45f, .7f, 1.6f, 3, .5f, 0 }, { 0, .65f, .92f, 2.0f, 1, .8f, 0 }, { 0, 0, .5f, 1, 4, .3f, 0 } };
    static const Place funk[kNumRoles] = {
        { 0, 0, .5f, 1, 5, .3f, 0 },       { 0, -.6f, .1f, .2f, 9, .08f, 0 },  { .1f, .1f, .2f, .7f, 8, .3f, .15f },
        { .45f, .7f, .3f, .7f, 5, .25f, .1f }, { -.6f, .35f, .35f, .8f, 5, .35f, 0 }, { 0, -.8f, .08f, .3f, 9, .05f, 0 },
        { 0, .25f, .05f, .55f, 10, .3f, .1f }, { -.55f, .4f, .45f, 1.5f, 5, .5f, 0 }, { .3f, .5f, .3f, 1.0f, 7, .35f, .1f },
        { 0, .3f, .75f, 1.7f, 2, .6f, 0 }, { -.5f, .15f, .35f, 1.2f, 6, .35f, 0 }, { .62f, .15f, .3f, .6f, 7, .3f, .1f },
        { 0, .45f, .6f, 1.7f, 4, .55f, 0 }, { 0, .7f, .9f, 2.0f, 1, .75f, 0 }, { 0, 0, .5f, 1, 4, .3f, 0 } };
    static const Place hiphop[kNumRoles] = {
        { 0, 0, .5f, 1, 5, .25f, 0 },      { 0, -.7f, .03f, .1f, 9, .02f, 0 }, { 0, .1f, .15f, .5f, 8, .2f, .1f },
        { .3f, .75f, .3f, .6f, 5, .12f, .15f }, { -.4f, .35f, .4f, .8f, 4, .25f, 0 }, { 0, -.9f, .05f, .15f, 9, .02f, -.15f },
        { 0, .2f, .02f, .35f, 10, .15f, .15f }, { .5f, .3f, .4f, 1.3f, 6, .35f, 0 }, { -.25f, .45f, .35f, 1.0f, 6, .3f, 0 },
        { 0, .3f, .85f, 1.8f, 2, .55f, -.1f }, { .35f, .1f, .5f, 1.2f, 5, .35f, -.1f }, { -.45f, .1f, .5f, 1.0f, 5, .3f, 0 },
        { 0, .4f, .7f, 1.6f, 3, .5f, 0 }, { 0, .65f, .9f, 2.0f, 1, .7f, 0 }, { 0, 0, .5f, 1, 4, .25f, 0 } };
    static const Place ambient[kNumRoles] = {
        { 0, 0, .6f, 1.3f, 5, .55f, 0 },   { 0, -.6f, .3f, .4f, 7, .25f, -.1f }, { .2f, .1f, .5f, 1.0f, 5, .5f, 0 },
        { .55f, .75f, .6f, 1.3f, 4, .55f, .05f }, { -.6f, .4f, .65f, 1.3f, 4, .6f, 0 }, { 0, -.8f, .25f, .7f, 8, .2f, -.1f },
        { 0, .25f, .3f, .9f, 10, .5f, 0 }, { .5f, .4f, .7f, 1.8f, 5, .7f, 0 }, { .3f, .5f, .5f, 1.4f, 7, .6f, 0 },
        { 0, .35f, .9f, 2.0f, 3, .85f, -.05f }, { -.35f, .15f, .55f, 1.5f, 6, .6f, -.1f }, { .45f, .15f, .6f, 1.3f, 5, .6f, 0 },
        { 0, .45f, .8f, 1.9f, 4, .75f, 0 }, { 0, .75f, 1.0f, 2.0f, 2, 1.0f, 0 }, { 0, 0, .6f, 1.3f, 4, .5f, 0 } };
    static const Place band[kNumRoles] = {
        { 0, 0, .45f, 1, 5, .3f, 0 },      { 0, -.6f, .35f, .3f, 8, .2f, 0 },  { .08f, .05f, .38f, .5f, 7, .3f, 0 },
        { -.25f, .7f, .4f, 1.0f, 4, .3f, 0 }, { .55f, .3f, .45f, .6f, 4, .35f, 0 }, { .2f, -.8f, .3f, .3f, 8, .15f, 0 },
        { 0, .2f, .1f, .4f, 10, .3f, .05f }, { -.45f, .3f, .3f, 1.0f, 5, .4f, 0 }, { .4f, .4f, .35f, .8f, 6, .35f, 0 },
        { 0, .3f, .75f, 1.6f, 2, .55f, 0 }, { -.55f, .15f, .3f, 1.0f, 6, .3f, 0 }, { .6f, .1f, .25f, .7f, 7, .3f, 0 },
        { -.2f, .4f, .65f, 1.5f, 3, .5f, 0 }, { 0, .65f, .9f, 2.0f, 1, .7f, 0 }, { 0, 0, .45f, 1, 4, .3f, 0 } };
    static const Place pop[kNumRoles] = {
        { 0, 0, .45f, 1.2f, 5, .3f, 0 },   { 0, -.65f, .06f, .2f, 9, .05f, 0 }, { 0, .1f, .2f, .8f, 8, .3f, .1f },
        { .5f, .75f, .35f, 1.2f, 5, .25f, .15f }, { -.55f, .35f, .4f, 1.2f, 4, .3f, 0 }, { 0, -.85f, .08f, .3f, 9, .05f, 0 },
        { 0, .25f, .03f, .5f, 10, .28f, .15f }, { 0, .4f, .45f, 2.0f, 6, .45f, .05f }, { .35f, .5f, .25f, 1.3f, 7, .3f, .1f },
        { 0, .35f, .8f, 2.0f, 2, .6f, 0 }, { -.4f, .15f, .4f, 1.4f, 5, .3f, 0 }, { .55f, .1f, .35f, 1.2f, 5, .3f, 0 },
        { 0, .5f, .65f, 2.0f, 3, .5f, 0 }, { 0, .7f, .9f, 2.0f, 1, .7f, 0 }, { 0, 0, .45f, 1.2f, 4, .3f, 0 } };
    static const Place imm[kNumRoles] = {
        { 0, 0, .5f, 1.2f, 5, .35f, 0 },   { 0, -.5f, .05f, .3f, 9, .1f, 0 },  { 0, .1f, .2f, .8f, 7, .3f, .1f },
        { .7f, .8f, .55f, 1.0f, 5, .3f, .1f }, { -.8f, .4f, .65f, 1.0f, 4, .35f, 0 }, { 0, -.8f, .08f, .4f, 9, .05f, 0 },
        { 0, .25f, .05f, .5f, 10, .3f, .1f }, { -.7f, .5f, .85f, 1.4f, 5, .5f, 0 }, { .5f, .6f, .3f, 1.2f, 7, .35f, .1f },
        { 0, .9f, 1.0f, 2.0f, 2, .65f, 0 }, { -.6f, .2f, .35f, 1.2f, 5, .35f, 0 }, { .65f, .2f, .4f, 1.0f, 5, .35f, 0 },
        { .2f, .6f, .9f, 1.8f, 3, .55f, 0 }, { 0, 1.0f, .75f, 2.0f, 1, .8f, 0 }, { 0, .3f, .5f, 1.2f, 4, .35f, 0 } };
    static const Place* tables[kNumStyles] = { club, funk, hiphop, ambient, band, pop, imm };
    return tables[juce::jlimit (0, kNumStyles - 1, style)][juce::jlimit (0, kNumRoles - 1, role)];
}
} // namespace cst
