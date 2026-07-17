#ifndef DOCUMENTMANAGER_H
#define DOCUMENTMANAGER_H

#include "otbmreader.h"

#include <QObject>
#include <QVariantList>
#include <QVector>

// -----------------------------------------------------------------------------
// DocumentManager
//
// System kart map (jak RME): kazda otwarta mapa to osobny OtbmReader (wlasne
// kafle, undo/redo, dirty, sciezka). Manager trzyma liste i wskazuje aktywny.
//
// Podzial odpowiedzialnosci:
//  - C++ (tutaj): cykl zycia readerow, lista kart, biezacy indeks.
//  - main.cpp: context property "otbmReader" przepiete na current() przy kazdym
//    currentChanged - dzieki temu CALY istniejacy QML (menu, dialogi) dziala bez
//    zmian, bo bindingi na context property sa re-ewaluowane po podmianie.
//  - QML: pasek kart (klik/zamkniecie), pytanie o zapis przy dirty, przeladowanie
//    danych klienta gdy mapa w karcie jest z innej wersji.
//
// Zawsze istnieje co najmniej jeden dokument - "otbmReader" nigdy nie jest null,
// wiec zaden binding w QML nie musi sie bronic przed brakiem readera.
// -----------------------------------------------------------------------------
class DocumentManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY tabsChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(OtbmReader *current READ current NOTIFY currentChanged)
    // Lista kart dla QML: [{ title, dirty, loaded }] - odswiezana przy kazdej
    // zmianie sciezki/dirty ktoregokolwiek dokumentu.
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)

public:
    explicit DocumentManager(QObject *parent = nullptr);

    int count() const { return static_cast<int>(m_docs.size()); }
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int i);
    OtbmReader *current() const;
    QVariantList tabs() const;

    // Nowy pusty dokument; staje sie biezacym. Zwracany reader jest wlasnoscia
    // managera (parent) - QML go nie przejmuje.
    Q_INVOKABLE OtbmReader *newDocument();

    // Zamyka dokument BEZ pytania o zapis (pytanie to sprawa QML - tu tylko cykl
    // zycia). Gdy zamknieto ostatni, tworzy swiezy pusty, zeby current() zyl.
    // Zwraca true, gdy po zamknieciu nie zostal zaden ZALADOWANY dokument
    // (QML wraca wtedy na ekran startowy).
    Q_INVOKABLE bool closeDocument(int i);

    // Indeks dokumentu z ta sciezka albo -1. Otwarcie juz otwartej mapy przelacza
    // na jej karte (jak RME), zamiast tworzyc duplikat.
    Q_INVOKABLE int indexOfPath(const QString &path) const;

signals:
    void currentChanged();
    void tabsChanged();

private:
    void hookDocument(OtbmReader *doc);

    QVector<OtbmReader *> m_docs;
    int m_current = 0;
};

#endif // DOCUMENTMANAGER_H
