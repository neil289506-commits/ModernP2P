import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 980
    height: 700
    minimumWidth: 780
    minimumHeight: 540
    visible: true
    title: "ModernP2P"
    color: cBg

    // ---- theme -----------------------------------------------------------
    // Follows the system light/dark setting. `forceDark` is only defined by the
    // screenshot tool.
    readonly property bool dark: (typeof forceDark !== "undefined")
        ? forceDark
        : (Application.styleHints !== undefined && Application.styleHints.colorScheme === Qt.Dark)

    readonly property color cBg: dark ? "#0E1018" : "#F4F5FA"
    readonly property color cCard: dark ? "#171A26" : "#FFFFFF"
    readonly property color cText: dark ? "#ECEEF8" : "#141726"
    readonly property color cMuted: dark ? "#9097AD" : "#6A7088"
    readonly property color cBorder: dark ? "#2A2E42" : "#E2E5EF"
    readonly property color cButton: dark ? "#232738" : "#EDEFF6"
    readonly property color cAccent: dark ? "#8C8CFF" : "#5B5BD6"
    readonly property color cAccentText: dark ? "#0E1018" : "#FFFFFF"
    readonly property color cGreen: dark ? "#4ADE80" : "#16A34A"
    readonly property color cRed: dark ? "#F87171" : "#DC2626"
    readonly property color cAmber: dark ? "#FBBF24" : "#D97706"

    palette.window: cBg
    palette.windowText: cText
    palette.base: cCard
    palette.text: cText
    palette.button: cButton
    palette.buttonText: cText
    palette.highlight: cAccent
    palette.highlightedText: cAccentText
    palette.placeholderText: cMuted
    palette.mid: cBorder

    // ---- helpers ---------------------------------------------------------
    property bool copied: false

    function fmtSize(n) {
        if (n < 1024) return n + " B"
        if (n < 1048576) return (n / 1024).toFixed(1) + " KB"
        if (n < 1073741824) return (n / 1048576).toFixed(1) + " MB"
        return (n / 1073741824).toFixed(2) + " GB"
    }
    function ext(name) {
        const i = name.lastIndexOf(".")
        return (i > 0 && i < name.length - 1) ? name.slice(i + 1, i + 5).toUpperCase() : "FILE"
    }
    function stateColor(state) {
        if (state === "Done") return cGreen
        if (state === "Failed") return cRed
        if (state === "Cancelled") return cAmber
        return cAccent
    }
    function copyCode() {
        clipboardHelper.text = app.localCode
        clipboardHelper.selectAll()
        clipboardHelper.copy()
        copied = true
        copyTimer.restart()
    }

    TextEdit { id: clipboardHelper; visible: false }
    Timer { id: copyTimer; interval: 1600; onTriggered: root.copied = false }

    Connections {
        target: app
        function onRequestArrived() { root.alert(0) } // flashes the taskbar button
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Download folder")
        onAccepted: app.setDownloadDir(selectedFolder)
    }
    FileDialog {
        id: fileDialog
        title: qsTr("Choose files to send")
        fileMode: FileDialog.OpenFiles
        onAccepted: app.addFiles(selectedFiles)
    }

    // ---- reusable pieces --------------------------------------------------
    component AppButton: Button {
        id: btn
        property bool primary: false
        property bool subtle: false
        implicitHeight: 40
        leftPadding: 18
        rightPadding: 18
        hoverEnabled: true
        font.pixelSize: 14
        font.weight: Font.DemiBold

        contentItem: Text {
            text: btn.text
            font: btn.font
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: !btn.enabled ? root.cMuted
                 : btn.primary ? root.cAccentText
                 : btn.subtle ? root.cMuted : root.cText
            opacity: btn.enabled ? 1 : 0.6
        }
        background: Rectangle {
            radius: 10
            border.width: (btn.primary || btn.subtle) ? 0 : 1
            border.color: root.cBorder
            color: {
                if (!btn.enabled)
                    return btn.primary ? root.cBorder : (btn.subtle ? "transparent" : root.cButton)
                if (btn.primary)
                    return btn.down ? Qt.darker(root.cAccent, 1.2) : (btn.hovered ? Qt.lighter(root.cAccent, 1.1) : root.cAccent)
                if (btn.subtle)
                    return btn.hovered ? root.cButton : "transparent"
                return btn.down ? Qt.darker(root.cButton, 1.1) : (btn.hovered ? Qt.lighter(root.cButton, 1.05) : root.cButton)
            }
        }
    }

    component Chip: Rectangle {
        id: chip
        property alias text: chipLabel.text
        property color tone: root.cAccent
        implicitWidth: chipLabel.implicitWidth + 18
        implicitHeight: 24
        radius: 12
        color: Qt.rgba(tone.r, tone.g, tone.b, 0.16)
        Text {
            id: chipLabel
            anchors.centerIn: parent
            color: chip.tone
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
    }

    component FileBadge: Rectangle {
        id: badge
        property string label: "FILE"
        property color tone: root.cAccent
        implicitWidth: 40
        implicitHeight: 40
        radius: 10
        color: Qt.rgba(tone.r, tone.g, tone.b, 0.14)
        Text {
            anchors.centerIn: parent
            text: badge.label
            color: badge.tone
            font.pixelSize: badge.label.length > 3 ? 10 : 12
            font.weight: Font.Bold
        }
    }

    // ---- header -----------------------------------------------------------
    header: Rectangle {
        height: 68
        color: root.cCard
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.cBorder }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 20
            spacing: 12

            Rectangle {
                width: 36; height: 36; radius: 10
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "#6366F1" }
                    GradientStop { position: 1; color: "#A855F7" }
                }
                Text { anchors.centerIn: parent; text: "⇄"; color: "white"; font.pixelSize: 20; font.bold: true }
            }
            Text {
                text: "ModernP2P"
                color: root.cText
                font.pixelSize: 20
                font.weight: Font.Bold
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.maximumWidth: 420
                implicitWidth: statusRow.implicitWidth + 28
                implicitHeight: 32
                radius: 16
                color: root.cButton
                Row {
                    id: statusRow
                    anchors.centerIn: parent
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: app.online ? root.cGreen : root.cAmber
                    }
                    Text {
                        text: app.status
                        color: root.cMuted
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        width: Math.min(implicitWidth, 340)
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            AppButton {
                text: qsTr("Folder")
                subtle: true
                onClicked: folderDialog.open()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Saving to: %1").arg(app.downloadDir)
            }
        }
    }

    // ---- main area --------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        DropArea {
            id: drop
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 2
            Layout.minimumHeight: 170
            readonly property bool compact: height < 380
            onDropped: (d) => { if (d.hasUrls) app.addFiles(d.urls) }

            Rectangle {
                anchors.fill: parent
                radius: 18
                color: drop.containsDrag ? Qt.rgba(root.cAccent.r, root.cAccent.g, root.cAccent.b, 0.1) : root.cCard
                border.width: drop.containsDrag ? 2 : 1
                border.color: drop.containsDrag ? root.cAccent : root.cBorder
                Behavior on color { ColorAnimation { duration: 120 } }

                // empty state
                ColumnLayout {
                    anchors.centerIn: parent
                    visible: app.staging.count === 0
                    spacing: drop.compact ? 8 : 14

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        visible: !drop.compact
                        Layout.preferredWidth: 84; Layout.preferredHeight: 84
                        radius: 42
                        color: Qt.rgba(root.cAccent.r, root.cAccent.g, root.cAccent.b, 0.14)
                        Text { anchors.centerIn: parent; text: "↓"; color: root.cAccent; font.pixelSize: 40; font.weight: Font.Bold }
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Drop files here to send")
                        color: root.cText
                        font.pixelSize: 22
                        font.weight: Font.Bold
                    }
                    AppButton {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Browse files…")
                        onClicked: fileDialog.open()
                    }
                    Row {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 10
                        visible: !drop.compact
                        spacing: 28
                        Repeater {
                            model: [qsTr("Add files"), qsTr("Enter your friend's code"), qsTr("Press Send")]
                            delegate: Row {
                                spacing: 8
                                Rectangle {
                                    width: 22; height: 22; radius: 11
                                    color: root.cButton
                                    anchors.verticalCenter: parent.verticalCenter
                                    Text { anchors.centerIn: parent; text: index + 1; color: root.cMuted; font.pixelSize: 12; font.weight: Font.Bold }
                                }
                                Text { text: modelData; color: root.cMuted; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                            }
                        }
                    }
                }

                // staged files
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    visible: app.staging.count > 0
                    spacing: 10

                    RowLayout {
                        Text {
                            text: qsTr("%1 file(s) · %2").arg(app.staging.count).arg(root.fmtSize(app.staging.totalSize))
                            color: root.cText
                            font.pixelSize: 16
                            font.weight: Font.Bold
                            Layout.fillWidth: true
                        }
                        AppButton { text: qsTr("Add more"); subtle: true; onClicked: fileDialog.open() }
                        AppButton { text: qsTr("Clear"); subtle: true; onClicked: app.clearStaging() }
                    }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 8
                        model: app.staging
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 60
                            radius: 12
                            color: root.dark ? "#1C2030" : "#F7F8FC"
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 6
                                spacing: 12
                                FileBadge { label: root.ext(model.name) }
                                Text {
                                    text: model.name
                                    color: root.cText
                                    font.pixelSize: 14
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }
                                Text { text: root.fmtSize(model.size); color: root.cMuted; font.pixelSize: 13 }
                                AppButton {
                                    text: "✕"
                                    subtle: true
                                    implicitWidth: 40
                                    leftPadding: 0; rightPadding: 0
                                    onClicked: app.removeStaged(index)
                                }
                            }
                        }
                    }
                }
            }
        }

        // ---- transfers ------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: transferList.count > 0 ? Math.min(320, 64 + transferList.count * 72) : 0
            visible: transferList.count > 0
            radius: 18
            color: root.cCard
            border.color: root.cBorder

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8
                Text { text: qsTr("Transfers"); color: root.cText; font.pixelSize: 15; font.weight: Font.Bold }

                ListView {
                    id: transferList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 10
                    model: app.transfers
                    ScrollBar.vertical: ScrollBar {}
                    delegate: RowLayout {
                        id: row
                        width: ListView.view.width
                        height: Math.max(54, implicitHeight)
                        spacing: 12
                        readonly property color tone: root.stateColor(model.state)

                        Rectangle {
                            width: 40; height: 40; radius: 20
                            color: Qt.rgba(row.tone.r, row.tone.g, row.tone.b, 0.15)
                            Text {
                                anchors.centerIn: parent
                                text: model.state === "Done" ? "✓" : (model.sending ? "↑" : "↓")
                                color: row.tone
                                font.pixelSize: 20
                                font.weight: Font.Bold
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            RowLayout {
                                Text {
                                    text: model.name
                                    color: root.cText
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: (model.sending ? qsTr("to ") : qsTr("from ")) + model.peer
                                    color: root.cMuted
                                    font.pixelSize: 12
                                    font.family: "Consolas"
                                }
                                Chip { text: model.state; tone: row.tone }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 6
                                radius: 3
                                color: root.cBorder
                                Rectangle {
                                    width: parent.width * Math.min(1, model.size > 0 ? model.done / model.size : 1)
                                    height: parent.height
                                    radius: 3
                                    color: row.tone
                                }
                            }
                            Text {
                                visible: text !== ""
                                height: visible ? implicitHeight : 0
                                text: model.error !== "" ? model.error
                                    : (model.state === "Transferring" && model.speed > 0
                                       ? root.fmtSize(model.done) + " / " + root.fmtSize(model.size) + " · " + root.fmtSize(model.speed) + "/s" : "")
                                color: model.error !== "" ? root.cRed : root.cMuted
                                font.pixelSize: 12
                            }
                        }
                        AppButton {
                            opacity: model.active ? 1 : 0
                            enabled: model.active
                            text: "✕"
                            subtle: true
                            implicitWidth: 40
                            leftPadding: 0; rightPadding: 0
                            onClicked: app.cancelTransfer(model.transferId)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Cancel")
                        }
                    }
                }
            }
        }
    }

    // ---- footer: send (left) and my code (right) ---------------------------
    footer: Rectangle {
        height: 96
        color: root.cCard
        Rectangle { width: parent.width; height: 1; color: root.cBorder }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 24

            ColumnLayout {
                spacing: 6
                Text { text: qsTr("SEND TO"); color: root.cMuted; font.pixelSize: 11; font.weight: Font.Bold; font.letterSpacing: 1 }
                RowLayout {
                    spacing: 10
                    TextField {
                        id: peerCode
                        objectName: "peerCode"
                        placeholderText: qsTr("Friend's code")
                        maximumLength: 8
                        font.family: "Consolas"
                        font.pixelSize: 18
                        font.letterSpacing: 2
                        font.capitalization: text.length > 0 ? Font.AllUppercase : Font.MixedCase
                        color: root.cText
                        placeholderTextColor: root.cMuted
                        inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                        implicitWidth: 190
                        implicitHeight: 44
                        leftPadding: 14
                        verticalAlignment: Text.AlignVCenter
                        onAccepted: sendButton.clicked()
                        background: Rectangle {
                            radius: 10
                            color: root.dark ? "#10121B" : "#F7F8FC"
                            border.width: peerCode.activeFocus ? 2 : 1
                            border.color: peerCode.activeFocus ? root.cAccent : root.cBorder
                        }
                    }
                    AppButton {
                        id: sendButton
                        text: qsTr("Send")
                        primary: true
                        implicitHeight: 44
                        implicitWidth: 96
                        enabled: peerCode.text.length === 8 && app.online
                        onClicked: app.connectToPeer(peerCode.text)
                    }
                    AppButton {
                        text: qsTr("Requests")
                        implicitHeight: 44
                        onClicked: requestsPopup.open()

                        Rectangle {
                            visible: app.requests.count > 0
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.rightMargin: -6
                            anchors.topMargin: -6
                            width: Math.max(22, badgeText.implicitWidth + 10)
                            height: 22
                            radius: 11
                            color: root.cRed
                            border.width: 2
                            border.color: root.cCard
                            Text {
                                id: badgeText
                                anchors.centerIn: parent
                                text: app.requests.count
                                color: "white"
                                font.pixelSize: 12
                                font.weight: Font.Bold
                            }
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            ColumnLayout {
                spacing: 6
                Text {
                    text: qsTr("YOUR CODE")
                    color: root.cMuted
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    font.letterSpacing: 1
                    Layout.alignment: Qt.AlignRight
                }
                RowLayout {
                    spacing: 10
                    Text {
                        text: app.localCode === "" ? "········" : app.localCode
                        color: root.cText
                        font.family: "Consolas"
                        font.pixelSize: 26
                        font.weight: Font.Bold
                        font.letterSpacing: 3
                    }
                    AppButton {
                        text: root.copied ? qsTr("Copied ✓") : qsTr("Copy")
                        enabled: app.localCode !== ""
                        implicitHeight: 38
                        onClicked: root.copyCode()
                    }
                    AppButton {
                        text: qsTr("New")
                        subtle: true
                        implicitHeight: 38
                        enabled: !app.busy
                        onClicked: app.regenerateCode()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Get a new code")
                    }
                }
            }
        }
    }

    // ---- incoming requests --------------------------------------------------
    Popup {
        id: requestsPopup
        objectName: "requestsPopup"
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(root.width - 48, 540)
        height: Math.min(root.height - 96, 460)
        padding: 20
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 18
            color: root.cCard
            border.color: root.cBorder
        }
        Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, root.dark ? 0.6 : 0.35) }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Text { text: qsTr("Incoming requests"); color: root.cText; font.pixelSize: 19; font.weight: Font.Bold }

            Text {
                visible: app.requests.count === 0
                text: qsTr("No pending requests")
                color: root.cMuted
                font.pixelSize: 14
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 32
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 12
                model: app.requests
                delegate: Rectangle {
                    width: ListView.view.width
                    height: col.implicitHeight + 28
                    radius: 14
                    color: root.dark ? "#1C2030" : "#F7F8FC"
                    border.color: root.cBorder
                    ColumnLayout {
                        id: col
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
                        spacing: 6
                        RowLayout {
                            Text { text: qsTr("From"); color: root.cMuted; font.pixelSize: 13 }
                            Text { text: model.from; color: root.cText; font.pixelSize: 16; font.family: "Consolas"; font.weight: Font.Bold; font.letterSpacing: 1 }
                            Item { Layout.fillWidth: true }
                            Chip { text: qsTr("%1 file(s) · %2").arg(model.fileCount).arg(root.fmtSize(model.totalSize)) }
                        }
                        Text { text: model.names; color: root.cMuted; font.pixelSize: 13; elide: Text.ElideRight; Layout.fillWidth: true }
                        RowLayout {
                            Layout.topMargin: 4
                            Item { Layout.fillWidth: true }
                            AppButton { text: qsTr("Reject"); onClicked: app.rejectRequest(model.requestId) }
                            AppButton { text: qsTr("Accept"); primary: true; onClicked: app.acceptRequest(model.requestId) }
                        }
                    }
                }
            }
            AppButton { text: qsTr("Close"); subtle: true; Layout.alignment: Qt.AlignRight; onClicked: requestsPopup.close() }
        }
    }
}
