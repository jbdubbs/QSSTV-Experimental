pragma Singleton
import QtQuick
import QSSTV

// The design tokens of the phone/tablet interface: Material 3 colour roles (light and dark), sizes and type. The accent is the cyan of the app icon.
QtObject {
    readonly property bool dark: AppModel.colorScheme === 2
                                 || (AppModel.colorScheme === 0 && Application.styleHints.colorScheme === Qt.ColorScheme.Dark)

    readonly property color primary: dark ? "#5dd5fc" : "#00677d"
    readonly property color primaryOn: dark ? "#003543" : "#ffffff"
    readonly property color primaryContainer: dark ? "#004d61" : "#b8eaff"
    readonly property color primaryContainerOn: dark ? "#b8eaff" : "#001f28"
    readonly property color surface: dark ? "#0f1417" : "#f6fafd"
    readonly property color surfaceContainer: dark ? "#1b2023" : "#eaeff2"
    readonly property color surfaceHigh: dark ? "#262b2e" : "#dfe4e7"
    readonly property color surfaceOn: dark ? "#dee3e6" : "#171c1f"
    readonly property color surfaceVariantOn: dark ? "#bfc8cc" : "#40484c"
    readonly property color outline: dark ? "#899296" : "#70787d"
    readonly property color outlineVariant: dark ? "#40484c" : "#c0c8cd"
    readonly property color error: dark ? "#ffb4ab" : "#ba1a1a"
    readonly property color errorContainer: dark ? "#93000a" : "#ffdad6"
    readonly property color good: dark ? "#8bd8a0" : "#1b6d3a"
    readonly property color goodContainer: dark ? "#0f5227" : "#c3efce"
    readonly property color warn: dark ? "#ffb86a" : "#8a5100"
    readonly property color warnContainer: dark ? "#693c00" : "#ffddb8"
    readonly property color scrim: dark ? "#99000000" : "#66000000"
    readonly property color waterfallBackground: dark ? "#05090c" : "#0b1a24"

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

    readonly property string fontFamily: Qt.platform.os === "android" ? "Roboto" : Qt.application.font.family
}
