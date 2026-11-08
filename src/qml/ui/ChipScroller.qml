import QtQuick

// Chips on ONE line. When they are wider than the screen the row scrolls sideways (the chips run to the screen edge, a cut-off last chip says
// there is more). The 48 dp height keeps the taps a chip takes outside its 32 dp (a clipped Flickable would cut them off).
// reveal(chip) brings a chip into view: call it when a chip becomes the chosen one.
Flickable {
    id: scroller
    default property alias chips: row.data
    height: Theme.tap
    contentWidth: row.width + 2 * Theme.margin
    contentHeight: height
    flickableDirection: Flickable.HorizontalFlick
    boundsBehavior: Flickable.StopAtBounds
    interactive: contentWidth > width
    clip: true

    Row {
        id: row
        x: Theme.margin
        spacing: 8
        height: parent.height
    }

    NumberAnimation { id: slide; target: scroller; property: "contentX"; duration: Theme.ms(180); easing.type: Easing.OutCubic }

    // scroll so that the chip lies inside the view with a margin to spare (as far as the content allows)
    function reveal(chip) {
        if (!chip || !chip.visible || width <= 0) return
        row.forceLayout()   // a chip that just appeared or changed its text has no place yet: the Row lays out at the next render
        const left = row.x + chip.x, right = left + chip.width
        let x = contentX
        if (left - Theme.margin < x) x = left - Theme.margin
        else if (right + Theme.margin > x + width) x = right + Theme.margin - width
        x = Math.max(0, Math.min(x, contentWidth - width))
        if (x === contentX) return
        slide.stop()
        slide.to = x
        slide.start()
    }
}
