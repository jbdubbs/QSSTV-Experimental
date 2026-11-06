import QtQuick

// A picture that fills the space and can be pinched larger, moved around when it is, and zoomed with a double tap
Item {
    id: zoom
    property url source
    readonly property bool zoomed: img.scale > 1.02
    signal tapped()

    Image {
        id: img
        width: zoom.width; height: zoom.height
        source: zoom.source
        asynchronous: true
        fillMode: Image.PreserveAspectFit
        smooth: true
        cache: false
    }

    // keep the picture covering the space: at scale s it may move (s - 1) / 2 of its size either way
    function settle() {
        if (img.scale <= 1.02) {
            reset.start()
            return
        }
        // the picture itself, not the space around it, must keep covering the space (a picture lower than the screen stays centered)
        var mx = Math.max(0, (img.paintedWidth * img.scale - zoom.width) / 2)
        var my = Math.max(0, (img.paintedHeight * img.scale - zoom.height) / 2)
        img.x = Math.max(-mx, Math.min(mx, img.x))
        img.y = Math.max(-my, Math.min(my, img.y))
    }
    ParallelAnimation {
        id: reset
        NumberAnimation { target: img; property: "scale"; to: 1; duration: 150 }
        NumberAnimation { target: img; property: "x"; to: 0; duration: 150 }
        NumberAnimation { target: img; property: "y"; to: 0; duration: 150 }
    }
    function zoomTo(point) {
        if (img.scale > 1.02) { reset.start(); return }
        var s = 2.5
        var cx = zoom.width / 2, cy = zoom.height / 2
        img.scale = s
        img.x = (cx - point.x) * (s - 1)
        img.y = (cy - point.y) * (s - 1)
        settle()
    }

    PinchHandler {
        target: img
        minimumScale: 1
        maximumScale: 8
        minimumRotation: 0
        maximumRotation: 0
        onActiveChanged: if (!active) zoom.settle()
    }
    DragHandler {
        target: img
        enabled: zoom.zoomed
        onActiveChanged: if (!active) zoom.settle()
    }
    // one tap shows or hides the bars, two taps zoom: the first waits to see if a second follows
    Timer { id: wait; interval: 260; onTriggered: zoom.tapped() }
    TapHandler {
        onTapped: (point) => {
            if (wait.running) { wait.stop(); zoom.zoomTo(point.position) }
            else wait.start()
        }
    }
    Accessible.role: Accessible.Graphic
}
