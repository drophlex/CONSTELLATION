// CONSTELLATION — UI strings (English default, Spanish optional)
#pragma once
#include <juce_core/juce_core.h>
#include "Registry.h"

namespace cst
{
enum S
{
    sStar, sSky, sView3D, sViewTop, sViewFront, sRoom, sPhones, sLang, sHelp,
    sGain, sWidth, sTone, sDepth, sHeight, sPan, sSpace, sLowCut, sUnmask, sPriority, sMute, sSolo, sRole,
    sScenes, sStore, sMorph, sWrap, sAutoMix, sApply, sUntangle, sMono, sDonate, sFooter,
    sNoStars, sStarsOnline, sCorr, sSelected, sSkyHint, sStarHint, sSleeping, sBands, sCuts,
    sHelpText, kNumStrings
};

inline const char* tr (int key)
{
    static const char* en[kNumStrings] = {
        "STAR", "SKY", "3D", "TOP", "FRONT", "Room", "3D Phones", "ES", "?",
        "Gain", "Width", "Tone", "Depth", "Height", "Pan", "Space", "Low Cut", "Unmask", "Priority", "Mute", "Solo", "Role",
        "Scenes", "Store", "Morph", "360\xc2\xb0 Wrap", "Auto-Mix", "Apply", "Untangle", "Mono", "\xe2\x99\xa5 If you like it, buy me a coffee",
        "Design by Dark Funk Durcal \xc2\xb7 Created at Polaris Sound Lab \xc2\xb7 M\xc3\xa9rida, Yucat\xc3\xa1n",
        "No stars yet. Insert CONSTELLATION on your tracks (STAR mode) - they appear here instantly.",
        "stars online", "Corr", "Selected", "SKY: put this one on the Master. Scenes + morph live here.",
        "STAR: this track. Drag any star - every track can be mixed from any window.", "sleeping", "Bands", "unmask",
        "HOW IT WORKS\n\n"
        "1. Insert Constellation on every track you want in the mix (mode STAR). Name and colour come from Ableton.\n"
        "2. Insert one more on the Master and switch it to SKY: that is your control room.\n"
        "3. Drag the stars: left/right = pan, up/down = height (brightness), into the screen = depth (distance + room).\n"
        "   3D view: drag = floor (pan + depth), Shift-drag = height, Alt-drag = width, wheel = width, double-click = reset.\n"
        "4. Red filaments show frequency clashes. UNMASK carves space automatically: lower-priority stars dip only the\n"
        "   bands where a higher-priority star is playing - and less if they are far apart in space.\n"
        "5. AUTO-MIX reads your track names (kick, bass, vox...) and places everything in one click. UNTANGLE fixes clashes.\n"
        "6. 3D PHONES = binaural surround on headphones (works in Ableton). In DAWs with surround buses (Logic, Reaper,\n"
        "   Cubase/Nuendo, Pro Tools...) choose a 5.1 / 7.1 / 7.1.4 output and the stars play on real speakers.\n"
        "7. 360\xc2\xb0 WRAP turns the stage into a sphere: the listener walks into the constellation.\n"
        "8. SCENES (SKY): Store A-D, then automate MORPH in Ableton to fly the mix between scenes.\n\n"
        "Tip: use the same format (all VST3 or all AU) on every track so the stars can see each other."
    };
    static const char* es[kNumStrings] = {
        "ESTRELLA", "CIELO", "3D", "ARRIBA", "FRENTE", "Sala", "3D aud\xc3\xad" "fonos", "EN", "?",
        "Ganancia", "Ancho", "Tono", "Profundidad", "Altura", "Paneo", "Espacio", "Corte graves", "Desenmascarar", "Prioridad", "Mute", "Solo", "Rol",
        "Escenas", "Guardar", "Morph", "Envolvente 360\xc2\xb0", "Auto-Mezcla", "Aplicar", "Desenredar", "Mono", "\xe2\x99\xa5 Si te gusta, inv\xc3\xadtame un caf\xc3\xa9",
        "Dise\xc3\xb1o de Dark Funk Durcal \xc2\xb7 Creado en Polaris Sound Lab \xc2\xb7 M\xc3\xa9rida, Yucat\xc3\xa1n",
        "A\xc3\xban no hay estrellas. Inserta CONSTELLATION en tus pistas (modo ESTRELLA) y aparecen aqu\xc3\xad al instante.",
        "estrellas en l\xc3\xadnea", "Corr", "Seleccionada", "CIELO: ponlo en el Master. Aqu\xc3\xad viven las escenas y el morph.",
        "ESTRELLA: esta pista. Arrastra cualquier estrella: toda la mezcla se controla desde cualquier ventana.", "dormida", "Bandas", "recorte",
        "C\xc3\x93MO FUNCIONA\n\n"
        "1. Inserta Constellation en cada pista de tu mezcla (modo ESTRELLA). El nombre y color vienen de Ableton.\n"
        "2. Inserta otro en el Master y c\xc3\xa1mbialo a CIELO: es tu cuarto de control.\n"
        "3. Arrastra las estrellas: izquierda/derecha = paneo, arriba/abajo = altura (brillo), hacia el fondo = profundidad.\n"
        "   Vista 3D: arrastrar = piso (paneo + profundidad), Shift = altura, Alt = ancho, rueda = ancho, doble clic = reset.\n"
        "4. Los filamentos rojos marcan choques de frecuencia. DESENMASCARAR abre espacio solo: las estrellas de menor\n"
        "   prioridad bajan \xc3\xbanicamente las bandas donde suena una de mayor prioridad (y menos si est\xc3\xa1n lejos en el espacio).\n"
        "5. AUTO-MEZCLA lee los nombres de tus pistas (kick, bajo, voz...) y acomoda todo en un clic. DESENREDAR corrige choques.\n"
        "6. 3D AUD\xc3\x8d" "FONOS = surround binaural en aud\xc3\xad" "fonos (funciona en Ableton). En DAWs con buses surround (Logic,\n"
        "   Reaper, Cubase/Nuendo, Pro Tools...) elige salida 5.1 / 7.1 / 7.1.4 y las estrellas suenan en bocinas reales.\n"
        "7. ENVOLVENTE 360\xc2\xb0 convierte el escenario en una esfera: el oyente entra a la constelaci\xc3\xb3n.\n"
        "8. ESCENAS (CIELO): guarda A-D y automatiza MORPH en Ableton para volar la mezcla entre escenas.\n\n"
        "Tip: usa el mismo formato (todo VST3 o todo AU) en todas las pistas para que las estrellas se vean entre s\xc3\xad."
    };
    const bool spanish = Registry::get().globals.lang.load() == 1;
    return (spanish ? es : en)[juce::jlimit (0, kNumStrings - 1, key)];
}

inline juce::String T (int key) { return juce::String::fromUTF8 (tr (key)); }

// ── surround export + licence strings ──
enum S2
{
    xExport, xRec, xArmed, xRecording, xSaved, xMasterHint, xTrial, xBuy, xLicensed, xLicTitle, xLicText,
    xEmail, xKey, xActivate, xActOk, xActBad, xExpired, xClose, xHaveKey, kNumStrings2
};
inline const char* kPrice = "$39 USD";
inline const char* kBuyUrl = "https://www.paypal.com/cgi-bin/webscr?cmd=_xclick&business=drophlex%40gmail.com&item_name=CONSTELLATION%20licence%20(send%20your%20e-mail)&amount=39.00&currency_code=USD&no_shipping=1";

inline juce::String T2 (int key)
{
    static const char* en[kNumStrings2] = {
        "Surround export", "REC", "Armed: press Play or Export Audio in Live", "Recording", "Saved - click to show the files",
        "SKY must be on the Master", "Trial: %d days left", "Buy licence", "Licensed", "CONSTELLATION licence",
        "Every feature is unlocked for 30 days. After that the stars stay visible but the audio passes through untouched until you activate a licence.\n\n"
        "One licence = one person, all your computers, Mac and Windows, VST3 and AU, every 1.x update.\n\n"
        "1. Press BUY (PayPal, %s).  2. You receive your key by e-mail at the address you paid with (usually the same day).  3. Paste e-mail and key here and press ACTIVATE.",
        "E-mail", "Licence key", "ACTIVATE", "Activated - thank you for supporting Polaris Sound Lab!", "That key does not match this e-mail. Copy it again, with no spaces missing.",
        "Trial ended - audio is bypassed. Get a licence to keep mixing.", "Close", "I have a key"
    };
    static const char* es[kNumStrings2] = {
        "Exportar surround", "REC", "Armado: presiona Play o Exportar audio en Live", "Grabando", "Guardado - clic para ver los archivos",
        "El CIELO debe estar en el Master", "Prueba: quedan %d d\xc3\xad" "as", "Comprar licencia", "Con licencia", "Licencia de CONSTELLATION",
        "Todo est\xc3\xa1 desbloqueado durante 30 d\xc3\xad" "as. Despu\xc3\xa9s las estrellas siguen visibles pero el audio pasa sin procesar hasta que actives una licencia.\n\n"
        "Una licencia = una persona, todas tus computadoras, Mac y Windows, VST3 y AU, todas las actualizaciones 1.x.\n\n"
        "1. Presiona COMPRAR (PayPal, %s).  2. Recibes tu llave por correo en la direcci\xc3\xb3n con la que pagaste (normalmente el mismo d\xc3\xad" "a).  3. Pega correo y llave aqu\xc3\xad y presiona ACTIVAR.",
        "Correo", "Llave de licencia", "ACTIVAR", "Activada. \xc2\xa1Gracias por apoyar a Polaris Sound Lab!", "Esa llave no corresponde a este correo. C\xc3\xb3piala otra vez completa.",
        "Termin\xc3\xb3 la prueba: el audio pasa sin procesar. Consigue una licencia para seguir mezclando.", "Cerrar", "Tengo una llave"
    };
    const bool spanish = Registry::get().globals.lang.load() == 1;
    return juce::String::fromUTF8 ((spanish ? es : en)[juce::jlimit (0, kNumStrings2 - 1, key)]);
}
} // namespace cst
