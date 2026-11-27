.pragma library
// Lyric appearance options shared by LyricStyleDialog and StyledVerse (Windows).
// Same stored names as MusicCompanion::lyricFonts()/lyricEffects() and LyricAppearance.FONTS/EFFECTS (Android);
// tests/lyrics/check_style_lists.py keeps the three in sync.
var fonts = ["Sans","Serif","Mono","Rounded","Fina","Média","Negra","Condensada","Casual","Cursiva","Versalete","Máquina"]
var fontLabels = ["Sans","Serif","Mono","Arredondada","Fina","Média","Negra","Condensada","Casual","Cursiva","Versalete","Máquina de escrever"]
var effects = ["Normal","Contorno","Contorno grosso","Brilho","Neon","Sombra","Relevo","3D","Vazado","Marca-texto","Gradiente","Piscante"]

// Families installed with Windows 10/11; nothing is bundled or downloaded.
function family(name) {
    switch (name) {
    case "Serif": return "Georgia"
    case "Mono": return "Consolas"
    case "Rounded": return "Trebuchet MS"
    case "Condensada": return "Bahnschrift"
    case "Casual": return "Comic Sans MS"
    case "Cursiva": return "Segoe Script"
    case "Versalete": return "Segoe UI"
    case "Máquina": return "Courier New"
    default: return "Segoe UI"
    }
}
// Weight variants of Segoe UI resolve by weight on every Qt font backend (GDI, DirectWrite, FreeType).
// Numeric values of Font.Light/Normal/DemiBold/Bold/Black (a library script has no QML enums).
function weight(name, bold) {
    const w = name === "Fina" ? 300 : name === "Média" ? 600 : name === "Negra" ? 900 : 400
    return bold ? Math.max(700, w) : w
}
function fontLabel(name) { const i = fonts.indexOf(name); return i < 0 ? name : fontLabels[i] }

// Rainbow rail: 1001 positions (0..1000) over 16 color stops, from white through the spectrum to black.
// Same table and arithmetic as LyricRail.java on Android (tests/android/LyricColorChecks.java pins the values).
var railMax = 1000
var stops = [0xffffff,0xff0000,0xff4000,0xff8000,0xffbf00,0xffff00,0x80ff00,0x00ff00,0x00ff80,0x00ffff,0x0080ff,0x0000ff,0x8000ff,0xff00ff,0xff0080,0x000000]
function railRgb(position) {
    const p = Math.max(0, Math.min(railMax, Math.round(position))), segments = stops.length - 1, scaled = p * segments
    const segment = Math.min(segments - 1, Math.floor(scaled / railMax))
    const t = (scaled - segment * railMax) / railMax
    const a = stops[segment], b = stops[segment + 1]
    function channel(shift) { const x = (a >> shift) & 255, y = (b >> shift) & 255; return Math.round(x + (y - x) * t) }
    return [channel(16), channel(8), channel(0)]
}
function hex(rgb) { return "#" + rgb.map(v => (v < 16 ? "0" : "") + v.toString(16).toUpperCase()).join("") }
function railColor(position) { return hex(railRgb(position)) }
function nearest(color) {
    const value = String(color || "#FFFFFF"), r = parseInt(value.substr(1, 2), 16), g = parseInt(value.substr(3, 2), 16), b = parseInt(value.substr(5, 2), 16)
    let best = 0, distance = Infinity
    for (let p = 0; p <= railMax; ++p) {
        const c = railRgb(p), d = (c[0] - r) * (c[0] - r) + (c[1] - g) * (c[1] - g) + (c[2] - b) * (c[2] - b)
        if (d < distance) { distance = d; best = p }
    }
    return best
}
function gradientStops() { return stops.map((s, i) => ({ position: i / (stops.length - 1), color: hex([(s >> 16) & 255, (s >> 8) & 255, s & 255]) })) }
