import QtQuick
import Quickshell.Io

Item {
    id: service

    property string inputMethod: ""
    readonly property string monitorPath: Qt.resolvedUrl("fcitx-state-monitor").toString().replace(/^file:\/\//, "")

    Process {
        command: [service.monitorPath]
        running: true

        stdout: SplitParser {
            onRead: function(line) {
                const state = line.trim()
                if (state === "keyboard-us" || state === "lotus")
                    service.inputMethod = state
            }
        }
    }
}
