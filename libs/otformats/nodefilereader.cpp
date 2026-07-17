#include "nodefilereader.h"

#include <QFile>

namespace {

uint16_t readLe16(const QByteArray &data, qsizetype offset)
{
    const auto *raw = reinterpret_cast<const uchar *>(data.constData());
    return static_cast<uint16_t>(raw[offset])
         | static_cast<uint16_t>(raw[offset + 1] << 8);
}

uint32_t readLe32(const QByteArray &data, qsizetype offset)
{
    const auto *raw = reinterpret_cast<const uchar *>(data.constData());
    return static_cast<uint32_t>(raw[offset])
         | (static_cast<uint32_t>(raw[offset + 1]) << 8)
         | (static_cast<uint32_t>(raw[offset + 2]) << 16)
         | (static_cast<uint32_t>(raw[offset + 3]) << 24);
}

} // namespace

bool BinaryNode::getU8(uint8_t &value)
{
    if (m_readOffset + 1 > m_data.size()) {
        m_readOffset = m_data.size();
        return false;
    }

    value = static_cast<uint8_t>(m_data.at(m_readOffset));
    ++m_readOffset;
    return true;
}

bool BinaryNode::getU16(uint16_t &value)
{
    if (m_readOffset + 2 > m_data.size()) {
        m_readOffset = m_data.size();
        return false;
    }

    value = readLe16(m_data, m_readOffset);
    m_readOffset += 2;
    return true;
}

bool BinaryNode::getU32(uint32_t &value)
{
    if (m_readOffset + 4 > m_data.size()) {
        m_readOffset = m_data.size();
        return false;
    }

    value = readLe32(m_data, m_readOffset);
    m_readOffset += 4;
    return true;
}

bool BinaryNode::getString(QString &value)
{
    uint16_t length = 0;
    if (!getU16(length)) {
        return false;
    }

    QByteArray bytes;
    if (!readBytes(length, bytes)) {
        return false;
    }

    value = QString::fromLatin1(bytes);
    return true;
}

bool BinaryNode::skip(qsizetype count)
{
    if (count < 0 || count > bytesRemaining()) {
        m_readOffset = m_data.size();
        return false;
    }

    m_readOffset += count;
    return true;
}

bool BinaryNode::readBytes(qsizetype count, QByteArray &out)
{
    if (count < 0 || count > bytesRemaining()) {
        m_readOffset = m_data.size();
        return false;
    }

    out = m_data.mid(m_readOffset, count);
    m_readOffset += count;
    return true;
}

qsizetype BinaryNode::bytesRemaining() const
{
    return m_readOffset < m_data.size() ? m_data.size() - m_readOffset : 0;
}

bool NodeFileReader::loadFile(const QString &path, const QVector<QByteArray> &acceptedIdentifiers)
{
    m_root = BinaryNode();
    m_ok = false;
    m_errorString.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Nie mozna otworzyc pliku: %1").arg(path));
        return false;
    }

    const QByteArray data = file.readAll();
    if (data.size() < 5) {
        setError(QStringLiteral("Plik jest za maly, by byc poprawnym plikiem node"));
        return false;
    }

    const QByteArray identifier = data.left(4);
    const bool wildcardIdentifier = identifier == QByteArray(4, '\0');
    bool accepted = wildcardIdentifier || acceptedIdentifiers.isEmpty();
    for (const QByteArray &candidate : acceptedIdentifiers) {
        if (candidate.size() == 4 && identifier == candidate) {
            accepted = true;
            break;
        }
    }

    if (!accepted) {
        setError(QStringLiteral("Nieprawidlowy identyfikator pliku"));
        return false;
    }

    qsizetype pos = 4;
    if (static_cast<uint8_t>(data.at(pos)) != Start) {
        setError(QStringLiteral("Nieprawidlowa struktura pliku node"));
        return false;
    }
    ++pos;

    if (!parseNode(data, pos, m_root)) {
        return false;
    }

    m_ok = true;
    return true;
}

bool NodeFileReader::parseNode(const QByteArray &data, qsizetype &pos, BinaryNode &node)
{
    while (pos < data.size()) {
        uint8_t byte = static_cast<uint8_t>(data.at(pos));
        ++pos;

        if (byte == Escape) {
            if (pos >= data.size()) {
                setError(QStringLiteral("Przerwany escape w pliku node"));
                return false;
            }
            node.m_data.append(data.at(pos));
            ++pos;
            continue;
        }

        if (byte == Start) {
            --pos;
            return parseChildNodes(data, pos, node);
        }

        if (byte == End) {
            return true;
        }

        node.m_data.append(static_cast<char>(byte));
    }

    setError(QStringLiteral("Nieoczekiwany koniec pliku node"));
    return false;
}

bool NodeFileReader::parseChildNodes(const QByteArray &data, qsizetype &pos, BinaryNode &node)
{
    while (pos < data.size()) {
        uint8_t marker = static_cast<uint8_t>(data.at(pos));
        ++pos;

        if (marker == Start) {
            BinaryNode child;
            if (!parseNode(data, pos, child)) {
                return false;
            }
            node.m_children.append(std::move(child));
            continue;
        }

        if (marker == End) {
            return true;
        }

        setError(QStringLiteral("Nieprawidlowy marker dziecka w pliku node"));
        return false;
    }

    setError(QStringLiteral("Nieoczekiwany koniec listy dzieci w pliku node"));
    return false;
}

void NodeFileReader::setError(const QString &message)
{
    m_errorString = message;
    m_ok = false;
}
