#include "documentmanager.h"

#include "otbmreader.h"

#include <QFileInfo>
#include <QVariantMap>

#include <algorithm>

DocumentManager::DocumentManager(QObject *parent)
    : QObject(parent)
{
    newDocument();   // niezmiennik: zawsze >= 1 dokument (current() nigdy null)
}

void DocumentManager::setCurrentIndex(int i)
{
    if (i < 0 || i >= m_docs.size() || i == m_current) return;
    m_current = i;
    emit currentChanged();
}

OtbmReader *DocumentManager::current() const
{
    return m_docs.value(m_current, nullptr);
}

QVariantList DocumentManager::tabs() const
{
    QVariantList out;
    for (const OtbmReader *doc : m_docs) {
        QVariantMap t;
        const QString path = doc->filePath();
        t.insert(QStringLiteral("title"),
                 path.isEmpty() ? QStringLiteral("(nowa mapa)") : QFileInfo(path).fileName());
        t.insert(QStringLiteral("dirty"), doc->isDirty());
        t.insert(QStringLiteral("loaded"), doc->isLoaded());
        out.push_back(t);
    }
    return out;
}

OtbmReader *DocumentManager::newDocument()
{
    auto *doc = new OtbmReader(this);
    hookDocument(doc);
    m_docs.push_back(doc);
    m_current = static_cast<int>(m_docs.size()) - 1;
    emit tabsChanged();
    emit currentChanged();
    return doc;
}

bool DocumentManager::closeDocument(int i)
{
    if (i < 0 || i >= m_docs.size()) return false;

    // Zapamietaj AKTYWNY dokument PRZED usunieciem: takeAt(i) przesuwa w dol indeksy
    // wszystkich kart za i, wiec samo trzymanie m_current jako liczby przelaczaloby
    // po cichu na inna mape gdy zamkniemy karte LEZACA PRZED aktywna. Zamiast liczyc
    // na indeksie, po usunieciu odnajdujemy ten sam wskaznik.
    OtbmReader *cur = m_docs.value(m_current, nullptr);

    OtbmReader *doc = m_docs.takeAt(i);
    // deleteLater, nie delete: MapView/bindingi QML moga jeszcze trzymac wskaznik
    // dopoki currentChanged sie nie przepropaguje - kasujemy po powrocie do petli.
    doc->deleteLater();

    if (m_docs.isEmpty()) {
        newDocument();   // emituje oba sygnaly
        return true;     // nic zaladowanego - QML wraca na ekran startowy
    }
    // Aktywna karta nadal istnieje (zamknieto inna) -> zostajemy na niej, tylko z
    // poprawionym indeksem. Zamknieto aktywna -> klamrujemy do dostepnego zakresu.
    const int idx = (cur && cur != doc) ? static_cast<int>(m_docs.indexOf(cur)) : -1;
    m_current = idx >= 0 ? idx
                         : std::min(m_current, static_cast<int>(m_docs.size()) - 1);
    emit tabsChanged();
    emit currentChanged();

    for (const OtbmReader *d : m_docs)
        if (d->isLoaded()) return false;
    return true;
}

int DocumentManager::indexOfPath(const QString &path) const
{
    if (path.isEmpty()) return -1;
    for (int i = 0; i < m_docs.size(); ++i)
        if (m_docs[i]->filePath() == path) return i;
    return -1;
}

void DocumentManager::hookDocument(OtbmReader *doc)
{
    // Kazda zmiana tozsamosci/stanu dokumentu odswieza liste kart.
    connect(doc, &OtbmReader::filePathChanged, this, &DocumentManager::tabsChanged);
    connect(doc, &OtbmReader::dirtyChanged, this, &DocumentManager::tabsChanged);
    connect(doc, &OtbmReader::loadedChanged, this, &DocumentManager::tabsChanged);
}
