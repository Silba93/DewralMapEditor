#ifndef NODEFILEREADER_H
#define NODEFILEREADER_H

#include <QByteArray>
#include <QString>
#include <QVector>
#include <cstdint>

class BinaryNode
{
public:
    bool getU8(uint8_t &value);
    bool getU16(uint16_t &value);
    bool getU32(uint32_t &value);
    bool getString(QString &value);
    bool skip(qsizetype count);
    bool readBytes(qsizetype count, QByteArray &out);

    qsizetype bytesRemaining() const;
    const QVector<BinaryNode> &children() const { return m_children; }

private:
    QByteArray m_data;
    QVector<BinaryNode> m_children;
    qsizetype m_readOffset = 0;

    friend class NodeFileReader;
};

class NodeFileReader
{
public:
    bool loadFile(const QString &path, const QVector<QByteArray> &acceptedIdentifiers);

    bool isOk() const { return m_ok; }
    QString errorString() const { return m_errorString; }
    BinaryNode &rootNode() { return m_root; }
    const BinaryNode &rootNode() const { return m_root; }

private:
    enum Marker : uint8_t {
        Start = 0xFE,
        End = 0xFF,
        Escape = 0xFD
    };

    // depth: biezaca glebokosc zagniezdzenia. Poprawne pliki OTBM/OTB maja
    // drzewa plytkie (mapa ~5 poziomow + kontenery w kontenerach), ale format
    // pozwala zagniezdzac bez ograniczen - spreparowany plik z tysiacami
    // otwartych 0xFE moglby przepelnic stos przez te wzajemna rekurencje.
    static constexpr int kMaxDepth = 512;
    bool parseNode(const QByteArray &data, qsizetype &pos, BinaryNode &node, int depth);
    bool parseChildNodes(const QByteArray &data, qsizetype &pos, BinaryNode &node, int depth);
    void setError(const QString &message);

    BinaryNode m_root;
    bool m_ok = false;
    QString m_errorString;
};

#endif // NODEFILEREADER_H
