import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 960
    height: 680
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: "ModernP2P"
    color: "#f3f4f6"

    function fmtSize(n) {
        if (n < 1024) return n + " B"
        if (n < 1048576) return (n / 1024).toFixed(1) + " KB"
        if (n < 1073741824) return (n / 1048576).toFixed(1) + " MB"
        return (n / 1073741824).toFixed(2) + " GB"
    }

    Connections {
        target: app
        function onRequestArrived() { root.alert(0) } // flashes the taskbar button
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Download folder")
        onAccepted: app.setDownloadDir(selectedFolder)
    }

    header: Pane {
        padding: 12
        background: Rectangle { color: "#ffffff"; Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#e5e7eb" } }
        RowLayout {
            anchors.fill: parent
            Rectangle { width: 10; height: 10; radius: 5; color: app.online ? "#22c55e" : "#f59e0b" }
            Label { text: app.status; elide: Text.ElideRight; Layout.fillWidth: true; color: "#374151" }
            Button { text: qsTr("Folder…"); onClicked: folderDialog.open(); ToolTip.visible: hovered; ToolTip.text: app.downloadDir }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // ---- staging area -------------------------------------------------
        DropArea {
            id: drop
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 2
            onDropped: (d) => { if (d.hasUrls) app.addFiles(d.urls) }

            Rectangle {
                anchors.fill: parent
                radius: 12
                color: drop.containsDrag ? "#e8f0fe" : "#ffffff"
                border.width: 2
                border.color: drop.containsDrag ? "#4285f4" : "#d1d5db"

                Label {
                    anchors.centerIn: parent
                    visible: app.staging.count === 0
                    text: qsTr("Drop files here")
                    color: "#6b7280"
                    font.pixelSize: 18
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    visible: app.staging.count > 0
                    RowLayout {
                        Label {
                            text: qsTr("%1 file(s), %2").arg(app.staging.count).arg(root.fmtSize(app.staging.totalSize))
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        Button { text: qsTr("Clear"); flat: true; onClicked: app.clearStaging() }
                    }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: app.staging
                        ScrollBar.vertical: ScrollBar {}
                        delegate: RowLayout {
                            width: ListView.view.width
                            Label { text: model.name; elide: Text.ElideMiddle; Layout.fillWidth: true }
                            Label { text: root.fmtSize(model.size); color: "#6b7280" }
                            ToolButton { text: "✕"; onClicked: app.removeStaged(index) }
                        }
                    }
                }
            }
        }

        // ---- transfers ----------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: transferList.count > 0 ? 220 : 0
            visible: transferList.count > 0
            radius: 12
            color: "#ffffff"
            border.color: "#e5e7eb"

            ListView {
                id: transferList
                anchors.fill: parent
                anchors.margins: 12
                clip: true
                spacing: 8
                model: app.transfers
                ScrollBar.vertical: ScrollBar {}
                delegate: ColumnLayout {
                    width: ListView.view.width
                    spacing: 2
                    RowLayout {
                        Layout.minimumHeight: 36
                        Label { text: (model.sending ? "↑ " : "↓ ") + model.name; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        Label {
                            text: model.state + (model.state === "Transferring" && model.speed > 0 ? " · " + root.fmtSize(model.speed) + "/s" : "")
                            color: model.state === "Failed" ? "#dc2626" : "#6b7280"
                        }
                        Button { text: qsTr("Cancel"); flat: true; visible: model.active; onClicked: app.cancelTransfer(model.transferId) }
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: Math.max(model.size, 1)
                        value: model.done
                    }
                    Label { visible: model.error !== ""; text: model.error; color: "#dc2626"; font.pixelSize: 12 }
                }
            }
        }
    }

    // ---- footer: peer code + requests (left), my code (right) ----------------
    footer: Pane {
        padding: 12
        background: Rectangle { color: "#ffffff"; Rectangle { width: parent.width; height: 1; color: "#e5e7eb" } }
        RowLayout {
            anchors.fill: parent
            spacing: 10

            TextField {
                id: peerCode
                objectName: "peerCode"
                placeholderText: qsTr("Peer code")
                maximumLength: 8
                font.family: "Consolas"
                inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                Layout.preferredWidth: 150
                onAccepted: connectButton.clicked()
            }
            Button {
                id: connectButton
                text: qsTr("Send")
                enabled: peerCode.text.length === 8 && app.online
                onClicked: app.connectToPeer(peerCode.text)
            }
            Button {
                text: app.requests.count > 0 ? qsTr("Requests (%1)").arg(app.requests.count) : qsTr("Requests")
                highlighted: app.requests.count > 0
                onClicked: requestsPopup.open()
            }

            Item { Layout.fillWidth: true }

            Label { text: qsTr("My code"); color: "#6b7280" }
            Label {
                text: app.localCode === "" ? "········" : app.localCode
                font.family: "Consolas"
                font.bold: true
                font.pixelSize: 20
            }
            Button { text: qsTr("New"); flat: true; enabled: !app.busy; onClicked: app.regenerateCode() }
        }
    }

    Popup {
        id: requestsPopup
        objectName: "requestsPopup"
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(root.width - 48, 520)
        height: Math.min(root.height - 96, 420)
        padding: 16
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        ColumnLayout {
            anchors.fill: parent
            Label { text: qsTr("Incoming requests"); font.bold: true; font.pixelSize: 16 }
            Label {
                visible: app.requests.count === 0
                text: qsTr("No pending requests")
                color: "#6b7280"
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 24
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 10
                model: app.requests
                delegate: Rectangle {
                    width: ListView.view.width
                    height: col.implicitHeight + 20
                    radius: 8
                    color: "#f9fafb"
                    border.color: "#e5e7eb"
                    ColumnLayout {
                        id: col
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
                        Label { text: qsTr("From %1").arg(model.from); font.bold: true; font.family: "Consolas" }
                        Label {
                            text: qsTr("%1 file(s), %2").arg(model.fileCount).arg(root.fmtSize(model.totalSize))
                            color: "#6b7280"
                        }
                        Label { text: model.names; elide: Text.ElideRight; Layout.fillWidth: true; font.pixelSize: 12 }
                        RowLayout {
                            Item { Layout.fillWidth: true }
                            Button { text: qsTr("Reject"); onClicked: app.rejectRequest(model.requestId) }
                            Button { text: qsTr("Accept"); highlighted: true; onClicked: app.acceptRequest(model.requestId) }
                        }
                    }
                }
            }
            Button { text: qsTr("Close"); Layout.alignment: Qt.AlignRight; onClicked: requestsPopup.close() }
        }
    }
}
