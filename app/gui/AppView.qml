import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Controls.Material 2.2
import QtQuick.Layouts 1.3

import CustomGameModel 1.0
import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0

Page {
    property int computerIndex
    property CustomGameModel gameModel: createModel()
    property int gameTileMinWidth: 210
    property int gameTileGap: 16
    property real posterAspectRatio: 1.5 // 2:3 width:height -> height = width * 1.5
    property int gameFooterHeight: 84
    property int gameColumns: Math.max(1, Math.floor((gameList.width + gameTileGap) / (gameTileMinWidth + gameTileGap)))

    id: gameListPage
    focus: true

    signal focusToolbarRequested()

    function createModel() {
        var model = Qt.createQmlObject('import CustomGameModel 1.0; CustomGameModel {}', gameListPage, '')
        model.initialize(ComputerManager, computerIndex)
        return model
    }

    function launchDesktop() {
        var component = Qt.createComponent("StreamSegue.qml")
        var segue = component.createObject(stackView, {
            "appName": qsTr("Desktop"),
            "session": gameListPage.gameModel.createDesktopSession(),
            "gameModel": gameListPage.gameModel,
            "gameIndex": -1,
            "isResume": false
        })
        if (segue) {
            stackView.push(segue)
        }
    }

    StackView.onActivated: {
        gameList.forceActiveFocus()
        if (gameList.count > 0 && SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            gameList.currentIndex = 0
        }
    }

    header: null

    BusyIndicator {
        anchors.centerIn: parent
        running: gameModel.loading
        visible: gameModel.loading
    }

    Label {
        anchors.centerIn: parent
        text: gameModel.errorString
        color: "red"
        font.pointSize: 14
        wrapMode: Text.Wrap
        width: parent.width * 0.8
        horizontalAlignment: Text.AlignHCenter
        visible: !gameModel.loading && gameModel.errorString.length > 0
    }

    Label {
        anchors.centerIn: parent
        text: qsTr("No games found")
        font.pointSize: 16
        visible: !gameModel.loading && gameModel.errorString.length === 0 && gameList.count === 0
    }

    GridView {
        id: gameList
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Math.floor(gameListPage.gameTileGap / 2)
        anchors.rightMargin: Math.floor(gameListPage.gameTileGap / 2)
        anchors.bottomMargin: Math.floor(gameListPage.gameTileGap / 2)
        visible: !gameModel.loading && gameModel.errorString.length === 0
        model: gameModel
        focus: true
        clip: true
        cellWidth: Math.max(1, Math.floor(width / gameListPage.gameColumns))
        cellHeight: Math.floor((cellWidth - gameListPage.gameTileGap - 24) * gameListPage.posterAspectRatio) + gameListPage.gameFooterHeight + gameListPage.gameTileGap

        ScrollBar.vertical: ScrollBar {}

        Keys.onUpPressed: function(event) {
            if (gameList.currentIndex < gameListPage.gameColumns) {
                gameListPage.focusToolbarRequested()
                event.accepted = true
            }
        }

        delegate: ItemDelegate {
            id: gameDelegate
            width: gameList.cellWidth - gameListPage.gameTileGap
            height: gameList.cellHeight - gameListPage.gameTileGap
            highlighted: gameList.currentIndex === index
            focus: true
            x: Math.floor(gameListPage.gameTileGap / 2)
            y: Math.floor(gameListPage.gameTileGap / 2)

            background: Rectangle {
                radius: 10
                color: gameDelegate.highlighted ? Qt.darker(Material.backgroundColor, 1.2) : Qt.darker(Material.backgroundColor, 1.08)
                border.color: gameDelegate.highlighted ? Material.accentColor : Material.dividerColor
                border.width: gameDelegate.highlighted ? 2 : 1
            }

            Keys.onReturnPressed: launchGame()
            Keys.onEnterPressed: launchGame()
            Keys.onUpPressed: function(event) {
                if (index < gameListPage.gameColumns) {
                    gameListPage.focusToolbarRequested()
                    event.accepted = true
                } else {
                    gameList.moveCurrentIndexUp()
                    event.accepted = true
                }
            }
            onClicked: {
                gameList.currentIndex = index
                launchGame()
            }

            function launchGame() {
                // Notify the backend to launch the selected game
                gameListPage.gameModel.postGameLaunch(model.gameId)

                // Start streaming the Desktop app on the host
                var component = Qt.createComponent("StreamSegue.qml")
                var segue = component.createObject(stackView, {
                    "appName": model.name,
                    "session": gameListPage.gameModel.createSessionForGame(index),
                    "gameModel": gameListPage.gameModel,
                    "gameIndex": index,
                    "isResume": false
                })
                if (segue) {
                    stackView.push(segue)
                }
            }

            contentItem: ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.floor((gameDelegate.width - 24) * gameListPage.posterAspectRatio)

                    Rectangle {
                        anchors.fill: parent
                        radius: 8
                        color: Qt.rgba(1, 1, 1, 0.05)
                        border.color: Material.dividerColor
                        border.width: 1
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: model.posterUrl
                            visible: !!model.posterUrl && model.posterUrl.length > 0
                            asynchronous: true
                            cache: true
                            fillMode: Image.PreserveAspectCrop
                        }

                        Image {
                            anchors.centerIn: parent
                            source: "qrc:/res/ic_videogame_asset_white_48px.svg"
                            visible: !model.posterUrl || model.posterUrl.length === 0
                            width: 36
                            height: 36
                            opacity: 0.9
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        text: model.name
                        font.pointSize: 13
                        font.bold: true
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        verticalAlignment: Text.AlignVCenter
                    }

                    Label {
                        text: model.source
                        font.pointSize: 10
                        color: Material.accent
                        opacity: 0.8
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        Layout.minimumWidth: implicitWidth
                    }
                }
            }
        }
    }
}
