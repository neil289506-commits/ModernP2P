import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 900
    height: 620
    minimumWidth: 640
    minimumHeight: 440
    visible: true
    title: "ModernP2P"

    // Staging area: drop files here before sending
    DropArea {
        id: staging
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: footer.top; margins: 16 }

        Rectangle {
            anchors.fill: parent
            radius: 12
            color: staging.containsDrag ? "#e8f0fe" : "#f6f7f9"
            border.width: 2
            border.color: staging.containsDrag ? "#4285f4" : "#c9ced6"

            Label {
                anchors.centerIn: parent
                text: "Drop files here"
                color: "#6b7280"
                font.pixelSize: 18
            }
        }
    }

    footer: Pane {
        id: footer
        padding: 16

        RowLayout {
            anchors.fill: parent
            spacing: 12

            // Bottom-left: peer code input + Requests
            TextField {
                id: peerCode
                placeholderText: "Peer code"
                maximumLength: 8
                inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                Layout.preferredWidth: 160
            }
            Button { text: "Connect"; enabled: peerCode.text.length === 8 }
            Button { text: "Requests" }

            Item { Layout.fillWidth: true }

            // Bottom-right: this device's code
            Label { text: "My code:" }
            Label {
                text: localCode
                font.family: "monospace"
                font.bold: true
                font.pixelSize: 18
            }
        }
    }
}
