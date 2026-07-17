import QtQuick
import QtQuick.Controls

// Rozwijane menu (File/Edit/... i menu kontekstowe PPM) w stylu classic Tibia UI -
// zamiennik drop-in dla QtQuick.Controls Menu. UWAGA: delegate stylizuje tylko
// pozycje tworzone z Action (lub modelu). Jawne/Instantiatorowe MenuItem OMIJAJA
// delegate - tam uzywaj bezposrednio TibiaMenuItem (ten sam wyglad).
// Tlo: texture.png kafelkowane + cienka ramka.
Menu {
    id: root
    // Minimum 160, inaczej dopasuj do najszerszej pozycji (implicitContentWidth liczy
    // sie z implicitWidth delegatow - stabilniej niz contentWidth).
    implicitWidth: Math.max(160, implicitContentWidth + leftPadding + rightPadding)
    padding: 1
    overlap: 0

    background: Item {
        implicitWidth: 150
        Image {
            anchors.fill: parent
            source: (uiTheme.tex + "texture.png")
            fillMode: Image.Tile
            smooth: false
        }
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: 1
            border.color: "#6e6e6e"   // zawsze 1px szara ramka wokol menu
        }
    }

    // Delegat INLINE (nie referencja do siostrzanego TibiaMenuItem) - w qrc
    // rozwiazanie siostry z wnetrza tego pliku bywa zawodne i menu spadalo na
    // domyslny MenuItem (zolte podswietlenie, centrowany tekst). Ta sama stylistyka
    // co TibiaMenuItem.qml, tu wpisana wprost.
    delegate: MenuItem {
        id: menuItem
        implicitHeight: 24
        padding: 0
        spacing: 0

        contentItem: Item {
            implicitWidth: itemText.implicitWidth + 10 + (menuItem.subMenu !== null ? 22 : 10)
            implicitHeight: 24
            Text {
                id: itemText
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: menuItem.text
                color: !menuItem.enabled ? "#777" : (menuItem.highlighted ? "#eaffea" : "#dcdcdc")
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
            }
        }
        indicator: Item {}
        arrow: Text {
            visible: menuItem.subMenu !== null
            text: "❯"
            color: menuItem.highlighted ? "#eaffea" : "#999"
            font.pixelSize: 10
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
        }
        background: Rectangle {
            // UWAGA: QML uzywa #AARRGGBB (alfa NA POCZATKU), nie CSS #RRGGBBAA -
            // dlatego wczesniejsze "#ffffff30" to bylo RGB(255,255,48) = zolty!
            // Szare pol-przezroczyste zaznaczenie + 1px szara ramka (tekst czytelny).
            color: menuItem.highlighted ? "#807a7d82" : "transparent"
            border.width: menuItem.highlighted ? 1 : 0
            border.color: "#9a9a9a"
        }
    }
}
