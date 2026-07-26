import QtQuick

Image {
    id: icon

    property string name: ""

    source: name.length > 0 ? "qrc:/ui/github/" + name + ".svg" : ""
    fillMode: Image.PreserveAspectFit
    smooth: true
    mipmap: true
    cache: true
}
