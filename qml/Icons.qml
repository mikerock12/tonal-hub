pragma Singleton
import QtQuick

// Ícones próprios, desenhados num grid 24x24 em sintaxe de path SVG.
// "f" = preenchido (fill), "s" = contorno (stroke). Renderizados por Icon.qml com QtQuick.Shapes.
QtObject {
    readonly property string homeS: "M3 10.5 L12 3 L21 10.5 L21 21 L15 21 L15 15 L9 15 L9 21 L3 21 Z"
    readonly property string homeF: homeS
    readonly property string searchS: "M10.5 3.5 A7 7 0 1 0 10.5 17.5 A7 7 0 1 0 10.5 3.5 Z M15.6 15.6 L21 21"
    readonly property string libraryS: "M4 3 L4 21 M9 3 L9 21 M13.5 4.5 L19.5 20.5"
    readonly property string plusS: "M12 5 L12 19 M5 12 L19 12"

    readonly property string playF: "M7 4.5 L19.5 12 L7 19.5 Z"
    readonly property string pauseF: "M6 4 L10 4 L10 20 L6 20 Z M14 4 L18 4 L18 20 L14 20 Z"
    readonly property string nextF: "M5 5 L15 12 L5 19 Z M16.5 5 L19 5 L19 19 L16.5 19 Z"
    readonly property string prevF: "M19 5 L19 19 L9 12 Z M5 5 L7.5 5 L7.5 19 L5 19 Z"

    readonly property string shuffleS: "M3 7 L6 7 C10 7 13 17 18 17 L21 17 M18 14 L21 17 L18 20 M3 17 L6 17 C7.8 17 9.2 15.4 10.3 13.6 M13.7 10.4 C14.8 8.6 16.2 7 18 7 L21 7 M18 4 L21 7 L18 10"
    readonly property string repeatS: "M4 11 L4 9 C4 7.3 5.3 6 7 6 L20 6 M17 3 L20 6 L17 9 M20 13 L20 15 C20 16.7 18.7 18 17 18 L4 18 M7 21 L4 18 L7 15"
    readonly property string repeatOneS: repeatS + " M11 10.5 L12.5 9.5 L12.5 14.5"

    readonly property string heart: "M12 20.5 L10.6 19.2 C5.4 14.5 2 11.4 2 7.6 C2 4.5 4.4 2.1 7.5 2.1 C9.2 2.1 10.9 2.9 12 4.2 C13.1 2.9 14.8 2.1 16.5 2.1 C19.6 2.1 22 4.5 22 7.6 C22 11.4 18.6 14.5 13.4 19.2 Z"

    readonly property string speakerF: "M3 9 L7 9 L12 4.5 L12 19.5 L7 15 L3 15 Z"
    readonly property string volHighS: "M15.5 8.5 A5 5 0 0 1 15.5 15.5 M18.5 5.5 A9 9 0 0 1 18.5 18.5"
    readonly property string volLowS: "M15.5 8.5 A5 5 0 0 1 15.5 15.5"
    readonly property string muteS: "M16 9.5 L21 14.5 M21 9.5 L16 14.5"

    readonly property string queueS: "M3 6 L21 6 M3 12 L21 12 M3 18 L14 18"
    // Setlist: linhas de uma lista com uma nota musical ao lado.
    readonly property string setlistS: "M3 6 L13 6 M3 11 L13 11 M3 16 L10 16 M19.5 17 L19.5 6 L22.5 7.5 M17 14.5 A2.5 2.5 0 1 0 17 19.5 A2.5 2.5 0 1 0 17 14.5 Z"
    readonly property string lyricsS: "M12 3 C10.3 3 9 4.3 9 6 L9 11 C9 12.7 10.3 14 12 14 C13.7 14 15 12.7 15 11 L15 6 C15 4.3 13.7 3 12 3 Z M5.5 11 C5.5 14.6 8.4 17.5 12 17.5 C15.6 17.5 18.5 14.6 18.5 11 M12 17.5 L12 21"
    readonly property string clockS: "M12 3 A9 9 0 1 0 12 21 A9 9 0 1 0 12 3 Z M12 7 L12 12 L15.5 14"
    readonly property string chevronLeftS: "M15 5 L8 12 L15 19"
    readonly property string chevronRightS: "M9 5 L16 12 L9 19"
    readonly property string closeS: "M6 6 L18 18 M18 6 L6 18"
    readonly property string noteS: "M9 18 L9 5 L20 3 L20 16 M9 18 A3 3 0 1 1 3 18 A3 3 0 1 1 9 18 Z M20 16 A3 3 0 1 1 14 16 A3 3 0 1 1 20 16 Z"
    readonly property string devicesS: "M3 5 L15 5 L15 15 L3 15 Z M7 19 L11 19 M9 15 L9 19 M18 8 L21 8 L21 19 L18 19 Z"
    readonly property string nowPlayingS: "M3 4 L21 4 L21 20 L3 20 Z M15 4 L15 20"
    readonly property string personS: "M12 3 A4 4 0 1 0 12 11 A4 4 0 1 0 12 3 Z M4 21 C4 16.6 7.6 14 12 14 C16.4 14 20 16.6 20 21"
    readonly property string logoutS: "M15 4 L19 4 L19 20 L15 20 M10 8 L14 12 L10 16 M14 12 L4 12"
    readonly property string dotsF: "M5 10.5 A1.5 1.5 0 1 0 5 13.5 A1.5 1.5 0 1 0 5 10.5 Z M12 10.5 A1.5 1.5 0 1 0 12 13.5 A1.5 1.5 0 1 0 12 10.5 Z M19 10.5 A1.5 1.5 0 1 0 19 13.5 A1.5 1.5 0 1 0 19 10.5 Z"
    readonly property string refreshS: "M20 12 A8 8 0 1 1 17.7 6.3 M20 4 L20 9 L15 9"
    readonly property string checkS: "M5 12.5 L10 17.5 L19.5 7"
}
