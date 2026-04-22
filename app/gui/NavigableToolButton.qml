import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

ToolButton {
    property string iconSource

    Keys.priority: Keys.BeforeItem

    function focusNextVisibleInChain(forward) {
        var next = nextItemInFocusChain(forward)
        var safetyCounter = 0

        while (next && next !== this && safetyCounter < 64) {
            if (next.visible && next.enabled && next.activeFocusOnTab) {
                next.forceActiveFocus(Qt.TabFocus)
                return
            }

            next = next.nextItemInFocusChain(forward)
            safetyCounter++
        }
    }

    activeFocusOnTab: true

    icon.source: iconSource
    icon.width: background.width
    icon.height: background.height

    // This determines the size of the Material highlight. We increase it
    // from the default because we use larger than normal icons for TV readability.
    Layout.preferredHeight: parent.height

    Keys.onReturnPressed: {
        clicked()
    }

    Keys.onEnterPressed: {
        clicked()
    }

    Keys.onRightPressed: function(event) {
        focusNextVisibleInChain(true)
        event.accepted = true
    }

    Keys.onLeftPressed: function(event) {
        focusNextVisibleInChain(false)
        event.accepted = true
    }

    Keys.onTabPressed: function(event) {
        focusNextVisibleInChain(true)
        event.accepted = true
    }

    Keys.onBacktabPressed: function(event) {
        focusNextVisibleInChain(false)
        event.accepted = true
    }
}
