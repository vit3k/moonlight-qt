import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import ComputerModel 1.0

import ComputerManager 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import SdlGamepadKeyNavigation 1.0

CenteredGridView {
    property ComputerModel computerModel : createModel()

    // Wake-on-LAN flow state
    property int wakingComputerIndex: -1
    property string wakingComputerName: ""
    property int wakeAttempts: 0
    property bool wakingPaired: false

    function cancelWake() {
        wakeRetryTimer.stop()
        wakingComputerIndex = -1
        wakingComputerName = ""
        wakeAttempts = 0
        wakingPaired = false
    }

    Timer {
        id: wakeRetryTimer
        interval: 5000
        repeat: false
        onTriggered: {
            if (pcGrid.wakingComputerIndex < 0) return
            if (pcGrid.wakeAttempts >= 2) {
                // Gave up after two wake attempts
                errorDialog.text = qsTr("Unable to wake '%1'. Please ensure the PC is powered and Wake-on-LAN is configured.").arg(pcGrid.wakingComputerName)
                errorDialog.helpText = ""
                errorDialog.open()
                pcGrid.cancelWake()
            } else {
                pcGrid.wakeAttempts++
                computerModel.wakeComputer(pcGrid.wakingComputerIndex)
                wakeRetryTimer.start()
            }
        }
    }

    Connections {
        target: computerModel
        function onDataChanged(topLeft, bottomRight, roles) {
            if (pcGrid.wakingComputerIndex < 0) return
            if (topLeft.row <= pcGrid.wakingComputerIndex && pcGrid.wakingComputerIndex <= bottomRight.row) {
                if (computerModel.isComputerOnline(pcGrid.wakingComputerIndex)) {
                    var idx = pcGrid.wakingComputerIndex
                    var name = pcGrid.wakingComputerName
                    var paired = pcGrid.wakingPaired
                    pcGrid.cancelWake()
                    if (paired) {
                        var component = Qt.createComponent("AppView.qml")
                        var appView = component.createObject(stackView, {"computerIndex": idx, "objectName": name})
                        stackView.push(appView)
                    } else {
                        var pin = computerModel.generatePinString()
                        computerModel.pairComputer(idx, pin)
                        pairDialog.pin = pin
                        pairDialog.open()
                    }
                }
            }
        }
    }

    function ensureCurrentSelection() {
        if (count > 0 && currentIndex < 0) {
            currentIndex = 0
        }

        // Keep navigation actionable by focusing the current delegate item.
        if (activeFocus && currentItem) {
            currentItem.forceActiveFocus(Qt.TabFocus)
        }
    }

    id: pcGrid
    focus: true
    activeFocusOnTab: true
    cellWidth: Math.max(360, Math.min(width - 80, 760))
    cellHeight: 80
    // +1 ensures availableWidth < cellWidth so CenteredGridView uses minMargin (not
    // the remainder formula), keeping the single-column list properly centered.
    minMargin: Math.max(0, Math.ceil((width - cellWidth) / 2) + 1)
    topMargin: Math.max(20, (height - (count * cellHeight)) / 2)
    bottomMargin: topMargin
    clip: true
    objectName: qsTr("Computers")

    Component.onCompleted: {
        // Don't show any highlighted item until interacting with them.
        // We do this here instead of onActivated to avoid losing the user's
        // selection when backing out of a different page of the app.
        currentIndex = -1
    }

    // Note: Any initialization done here that is critical for streaming must
    // also be done in CliStartStreamSegue.qml, since this code does not run
    // for command-line initiated streams.
    StackView.onActivated: {
        // Setup signals on CM
        ComputerManager.computerAddCompleted.connect(addComplete)

        // Ensure directional navigation has a selected item.
        // This keeps keyboard/gamepad navigation working even without the toolbar.
        ensureCurrentSelection()
    }

    onCountChanged: ensureCurrentSelection()

    onActiveFocusChanged: {
        if (activeFocus) {
            ensureCurrentSelection()
        }
    }

    StackView.onDeactivating: {
        ComputerManager.computerAddCompleted.disconnect(addComplete)
        cancelWake()
    }

    function pairingComplete(error)
    {
        // Close the PIN dialog
        pairDialog.close()

        // Display a failed dialog if we got an error
        if (error !== undefined) {
            errorDialog.text = error
            errorDialog.helpText = ""
            errorDialog.open()
        }
    }

    function addComplete(success, detectedPortBlocking)
    {
        if (!success) {
            errorDialog.text = qsTr("Unable to connect to the specified PC.")

            if (detectedPortBlocking) {
                errorDialog.text += "\n\n" + qsTr("This PC's Internet connection is blocking Moonlight. Streaming over the Internet may not work while connected to this network.")
            }
            else {
                errorDialog.helpText = qsTr("Click the Help button for possible solutions.")
            }

            errorDialog.open()
        }
    }

    function createModel()
    {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', parent, '')
        model.initialize(ComputerManager)
        model.pairingCompleted.connect(pairingComplete)
        model.connectionTestCompleted.connect(testConnectionDialog.connectionTestComplete)
        return model
    }

    Row {
        anchors.centerIn: parent
        spacing: 5
        visible: pcGrid.count === 0

        BusyIndicator {
            id: searchSpinner
            visible: StreamingPreferences.enableMdns
            running: visible
        }

        Label {
            height: searchSpinner.height
            elide: Label.ElideRight
            text: StreamingPreferences.enableMdns ? qsTr("Searching for compatible hosts on your local network...")
                                                  : qsTr("Automatic PC discovery is disabled. Add your PC manually.")
            font.pointSize: 20
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }

    // Dark gradient background for the PC list page
    Rectangle {
        anchors.fill: parent
        z: -1
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#1a1a2e" }
            GradientStop { position: 1.0; color: "#16213e" }
        }
    }

    model: computerModel

    delegate: NavigableItemDelegate {
        id: pcDelegate
        width: pcGrid.cellWidth
        height: 72
        grid: pcGrid
        leftPadding: 0
        rightPadding: 0

        // Transparent clickable background — highlight is on the name only
        background: Item {}

        property alias pcContextMenu : pcContextMenuLoader.item

        Item {
            anchors.left: parent.left
            anchors.right: statusArea.left
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            height: pcNameText.height + 6

            Label {
                id: pcNameText
                text: model.name
                color: pcDelegate.highlighted ? "#ffffff" : "#cccccc"

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                font.pointSize: 24
                font.bold: pcDelegate.highlighted
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.NoWrap
                elide: Text.ElideRight

                Behavior on color { ColorAnimation { duration: 120 } }
            }

            // Accent underline that appears when highlighted
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                width: pcDelegate.highlighted ? Math.min(pcNameText.implicitWidth + 24, parent.width) : 0
                height: 2
                radius: 1
                color: "#4fc3f7"
                opacity: pcDelegate.highlighted ? 1.0 : 0.0

                Behavior on width    { NumberAnimation  { duration: 180; easing.type: Easing.OutCubic } }
                Behavior on opacity  { NumberAnimation  { duration: 180 } }
            }
        }

        Row {
            id: statusArea
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            BusyIndicator {
                id: checkingSpinner
                width: 24
                height: 24
                visible: model.statusUnknown || pcGrid.wakingComputerIndex === index
                running: visible
            }

            Image {
                id: stateIcon
                visible: !model.statusUnknown && pcGrid.wakingComputerIndex !== index
                source: model.online ? "qrc:/res/baseline-check_circle_outline-24px.svg"
                                     : "qrc:/res/baseline-warning-24px.svg"
                sourceSize.width: 24
                sourceSize.height: 24
            }
        }

        Loader {
            id: pcContextMenuLoader
            asynchronous: true
            sourceComponent: NavigableMenu {
                id: pcContextMenu
                initiator: pcContextMenuLoader.parent
                MenuItem {
                    text: qsTr("PC Status: %1").arg(model.online ? qsTr("Online") : qsTr("Offline"))
                    font.bold: true
                    enabled: false
                }
                NavigableMenuItem {
                    text: qsTr("View All Apps")
                    onTriggered: {
                        var component = Qt.createComponent("AppView.qml")
                        var appView = component.createObject(stackView, {"computerIndex": index, "objectName": model.name, "showHiddenGames": true})
                        stackView.push(appView)
                    }
                    visible: model.online && model.paired
                }
                NavigableMenuItem {
                    text: qsTr("Wake PC")
                    onTriggered: computerModel.wakeComputer(index)
                    visible: !model.online && model.wakeable
                }
                NavigableMenuItem {
                    text: qsTr("Test Network")
                    onTriggered: {
                        computerModel.testConnectionForComputer(index)
                        testConnectionDialog.open()
                    }
                }

                NavigableMenuItem {
                    text: qsTr("Rename PC")
                    onTriggered: {
                        renamePcDialog.pcIndex = index
                        renamePcDialog.originalName = model.name
                        renamePcDialog.open()
                    }
                }
                NavigableMenuItem {
                    text: qsTr("Delete PC")
                    onTriggered: {
                        deletePcDialog.pcIndex = index
                        deletePcDialog.pcName = model.name
                        deletePcDialog.open()
                    }
                }
                NavigableMenuItem {
                    text: qsTr("View Details")
                    onTriggered: {
                        showPcDetailsDialog.pcDetails = model.details
                        showPcDetailsDialog.open()
                    }
                }
            }
        }

        onClicked: {
            if (model.online) {
                if (!model.serverSupported) {
                    errorDialog.text = qsTr("The version of GeForce Experience on %1 is not supported by this build of Moonlight. You must update Moonlight to stream from %1.").arg(model.name)
                    errorDialog.helpText = ""
                    errorDialog.open()
                }
                else if (model.paired) {
                    // go to game view
                    var component = Qt.createComponent("AppView.qml")
                    var appView = component.createObject(stackView, {"computerIndex": index, "objectName": model.name})
                    stackView.push(appView)
                }
                else {
                    var pin = computerModel.generatePinString()

                    // Kick off pairing in the background
                    computerModel.pairComputer(index, pin)

                    // Display the pairing dialog
                    pairDialog.pin = pin
                    pairDialog.open()
                }
            } else if (!model.online) {
                if (model.wakeable) {
                    pcGrid.wakingComputerIndex = index
                    pcGrid.wakingComputerName = model.name
                    pcGrid.wakingPaired = model.paired
                    pcGrid.wakeAttempts = 1
                    computerModel.wakeComputer(index)
                    wakeRetryTimer.start()
                }
            }
        }

        onPressAndHold: {
            // popup() ensures the menu appears under the mouse cursor
            if (pcContextMenu.popup) {
                pcContextMenu.popup()
            }
            else {
                // Qt 5.9 doesn't have popup()
                pcContextMenu.open()
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton;
            onClicked: {
                parent.pressAndHold()
            }
        }

        Keys.onMenuPressed: {
            // We must use open() here so the menu is positioned on
            // the ItemDelegate and not where the mouse cursor is
            pcContextMenu.open()
        }

        Keys.onDeletePressed: {
            deletePcDialog.pcIndex = index
            deletePcDialog.pcName = model.name
            deletePcDialog.open()
        }
    }

    ErrorMessageDialog {
        id: errorDialog

        // Using Setup-Guide here instead of Troubleshooting because it's likely that users
        // will arrive here by forgetting to enable GameStream or not forwarding ports.
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide"
    }

    NavigableMessageDialog {
        id: pairDialog
        closePolicy: Popup.CloseOnEscape

        // don't allow edits to the rest of the window while open
        property string pin : "0000"
        text:qsTr("Please enter %1 on your host PC. This dialog will close when pairing is completed.").arg(pin)+"\n\n"+
             qsTr("If your host PC is running Sunshine, navigate to the Sunshine web UI to enter the PIN.")
        standardButtons: Dialog.Cancel
        onRejected: {
            // FIXME: We should interrupt pairing here
        }
    }

    NavigableMessageDialog {
        id: deletePcDialog
        // don't allow edits to the rest of the window while open
        property int pcIndex : -1
        property string pcName : ""
        text: qsTr("Are you sure you want to remove '%1'?").arg(pcName)
        standardButtons: Dialog.Yes | Dialog.No

        onAccepted: {
            computerModel.deleteComputer(pcIndex)
        }
    }

    NavigableMessageDialog {
        id: testConnectionDialog
        closePolicy: Popup.CloseOnEscape
        standardButtons: Dialog.Ok

        onAboutToShow: {
            testConnectionDialog.text = qsTr("Moonlight is testing your network connection to determine if any required ports are blocked.") + "\n\n" + qsTr("This may take a few seconds…")
            showSpinner = true
        }

        function connectionTestComplete(result, blockedPorts)
        {
            if (result === -1) {
                text = qsTr("The network test could not be performed because none of Moonlight's connection testing servers were reachable from this PC. Check your Internet connection or try again later.")
                imageSrc = "qrc:/res/baseline-warning-24px.svg"
            }
            else if (result === 0) {
                text = qsTr("This network does not appear to be blocking Moonlight. If you still have trouble connecting, check your PC's firewall settings.") + "\n\n" + qsTr("If you are trying to stream over the Internet, install the Moonlight Internet Hosting Tool on your gaming PC and run the included Internet Streaming Tester to check your gaming PC's Internet connection.")
                imageSrc = "qrc:/res/baseline-check_circle_outline-24px.svg"
            }
            else {
                text = qsTr("Your PC's current network connection seems to be blocking Moonlight. Streaming over the Internet may not work while connected to this network.") + "\n\n" + qsTr("The following network ports were blocked:") + "\n"
                text += blockedPorts
                imageSrc = "qrc:/res/baseline-error_outline-24px.svg"
            }

            // Stop showing the spinner and show the image instead
            showSpinner = false
        }
    }

    NavigableDialog {
        id: renamePcDialog
        property string label: qsTr("Enter the new name for this PC:")
        property string originalName
        property int pcIndex : -1;

        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: {
            // Force keyboard focus on the textbox so keyboard navigation works
            editText.forceActiveFocus()
        }

        onClosed: {
            editText.clear()
        }

        onAccepted: {
            if (editText.text) {
                computerModel.renameComputer(pcIndex, editText.text)
            }
        }

        ColumnLayout {
            Label {
                text: renamePcDialog.label
                font.bold: true
            }

            TextField {
                id: editText
                placeholderText: renamePcDialog.originalName
                Layout.fillWidth: true
                focus: true

                Keys.onReturnPressed: {
                    renamePcDialog.accept()
                }

                Keys.onEnterPressed: {
                    renamePcDialog.accept()
                }
            }
        }
    }

    NavigableMessageDialog {
        id: showPcDetailsDialog
        property string pcDetails : "";
        text: showPcDetailsDialog.pcDetails
        imageSrc: "qrc:/res/baseline-help_outline-24px.svg"
        standardButtons: Dialog.Ok
    }

    ScrollBar.vertical: ScrollBar {}
}
