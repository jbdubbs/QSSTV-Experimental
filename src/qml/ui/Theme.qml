pragma Singleton
import QtQuick
import QSSTV

// The design tokens of the phone/tablet interface: Material 3 colour roles, sizes and type. The accent is the cyan of the app icon.
// Five looks: light and dark (the phone's own setting, or chosen), Dim (a softer dark for a dark shack) and Night (red on black, so that no
// blue or green light reaches an eye that has got used to the dark). Settings > Appearance chooses.
QtObject {
    // 1 light, 2 dark, 3 dim, 4 night
    readonly property int mode: AppModel.theme !== 0 ? AppModel.theme : (Application.styleHints.colorScheme === Qt.ColorScheme.Dark ? 2 : 1)
    readonly property bool dark: mode >= 2
    readonly property bool dim: mode === 3
    readonly property bool night: mode === 4
    // the pictures are red too (Night, when asked for)
    readonly property bool tint: night && AppModel.tintPictures
    function pick(light, darkc, dimc, nightc) { return mode === 1 ? light : mode === 2 ? darkc : mode === 3 ? dimc : nightc }

    readonly property color primary: pick("#00677d", "#5dd5fc", "#86c9e3", "#ff6a55")
    readonly property color primaryOn: pick("#ffffff", "#003543", "#00303d", "#1c0300")
    readonly property color primaryContainer: pick("#b8eaff", "#004d61", "#173a47", "#4a1511")
    readonly property color primaryContainerOn: pick("#001f28", "#b8eaff", "#a9d6e6", "#ffb5a9")
    readonly property color surface: pick("#f6fafd", "#0f1417", "#121618", "#080000")
    readonly property color surfaceContainer: pick("#eaeff2", "#1b2023", "#1a1f22", "#130404")
    readonly property color surfaceHigh: pick("#dfe4e7", "#262b2e", "#22282b", "#1e0a09")
    readonly property color surfaceOn: pick("#171c1f", "#dee3e6", "#b7bfc3", "#f0705c")
    readonly property color surfaceVariantOn: pick("#40484c", "#bfc8cc", "#8f989c", "#cc5a4a")
    readonly property color outline: pick("#70787d", "#899296", "#6c7478", "#a04739")
    readonly property color outlineVariant: pick("#c0c8cd", "#40484c", "#323a3e", "#3a1612")
    readonly property color error: pick("#ba1a1a", "#ffb4ab", "#e59a92", "#ff5a48")
    readonly property color errorContainer: pick("#ffdad6", "#93000a", "#5a1d18", "#5a0f09")
    // the states that were green and amber: in Night they are told by their word and their icon, not by the colour
    readonly property color good: pick("#1b6d3a", "#8bd8a0", "#86bd98", "#f08a76")
    readonly property color goodContainer: pick("#c3efce", "#0f5227", "#18402a", "#3a1410")
    readonly property color warn: pick("#8a5100", "#ffb86a", "#d9a468", "#ff9a85")
    readonly property color warnContainer: pick("#ffddb8", "#693c00", "#4a3014", "#4a1b13")
    readonly property color scrim: pick("#66000000", "#99000000", "#99000000", "#b3000000")
    readonly property color waterfallBackground: pick("#0b1a24", "#05090c", "#06090b", "#050000")
    // the text and the shade on a picture (a mode badge on a thumbnail, the top of the viewer): white is a bright blue-green light in Night
    readonly property color onPicture: night ? "#ff8a75" : "#ffffff"
    readonly property color onPictureVariant: night ? "#cc5a4a" : "#b3ffffff"
    readonly property color pictureShade: "#a3000000"
    // the snackbar is the reverse of the screen (a light bar on a dark screen)
    readonly property color snackbar: pick("#2f3133", "#e3e2e6", "#2e373b", "#3a1612")
    readonly property color snackbarOn: pick("#f1f0f4", "#2f3133", "#c9d0d3", "#ff8a75")
    readonly property color snackbarAction: pick("#8fdcf5", "#00677d", "#86c9e3", "#ffb5a9")

    // sizes in dp (Qt Quick's own units); text is scaled with the system's text size setting
    readonly property int margin: 16
    readonly property int tap: 48
    readonly property int appBarHeight: 56
    readonly property int navBarHeight: 80
    readonly property int railWidth: 80
    readonly property int radius: 12
    readonly property real fontScale: AppModel.fontScale
    function sp(n) { return Math.round(n * fontScale) }
    // the margin that makes a place that takes taps at least 48 x 48 (a TapHandler takes taps this far outside its item)
    function tapMargin(item) { return item ? Math.max(0, (tap - Math.min(item.width, item.height)) / 2) : 0 }

    // the length of a movement of the interface in ms; 0 where the phone's "Remove animations" is on. What shows the live state (the picture
    // that grows, the waterfall, the level and the progress) is not a movement of the interface and keeps going.
    readonly property bool reducedMotion: AppModel.reducedMotion
    function ms(n) { return reducedMotion ? 0 : n }

    readonly property string fontFamily: Qt.platform.os === "android" ? "Roboto" : Qt.application.font.family
}
