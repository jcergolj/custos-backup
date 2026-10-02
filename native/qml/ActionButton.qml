import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    required property var style
    font.family: style.bodyFontFamily
    font.pixelSize: style.bodyTypeSize
    font.weight: Font.Normal
    Layout.preferredHeight: 36
}
