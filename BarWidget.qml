import QtQuick
import qs.Ui

BarWidget {
    id: root
    moduleName: "nkarl.en-vi-indicator"

    readonly property var inputService: bar && bar.shell
        ? bar.shell.serviceFor("nkarl.en-vi-indicator")
        : null
    readonly property string inputMethod: inputService ? inputService.inputMethod : ""

    readonly property string language: {
        switch (inputMethod) {
        case "keyboard-us":
            return "EN"
        case "lotus":
            return "VI"
        default:
            return "??"
        }
    }

    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    WidgetButton {
        id: button
        anchors.fill: parent
        bar: root.bar
        text: root.language
        tooltipText: "Input method: " + (root.inputMethod || "unavailable")
    }
}
