#include "binaryreader.h"

BinaryReader::BinaryReader(const QString &path)
{
    open(path);
}

BinaryReader::~BinaryReader()
{
    close();
}

bool BinaryReader::open(const QString &path)
{
    close();

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Failed to open file: %1").arg(path));
        return false;
    }

    m_fileSize = static_cast<size_t>(m_file.size());
    return true;
}

void BinaryReader::close()
{
    if (m_file.isOpen()) {
        m_file.close();
    }
    m_fileSize = 0;
    clearError();
}

uint8_t BinaryReader::readU8()
{
    uint8_t value = 0;
    qint64 n = m_file.read(reinterpret_cast<char *>(&value), 1);
    if (n != 1) setError(QStringLiteral("Failed to read U8"));
    return value;
}

uint16_t BinaryReader::readU16()
{
    uint16_t value = 0;
    qint64 n = m_file.read(reinterpret_cast<char *>(&value), 2);
    if (n != 2) setError(QStringLiteral("Failed to read U16"));
    return value;
}

uint32_t BinaryReader::readU32()
{
    uint32_t value = 0;
    qint64 n = m_file.read(reinterpret_cast<char *>(&value), 4);
    if (n != 4) setError(QStringLiteral("Failed to read U32"));
    return value;
}

uint64_t BinaryReader::readU64()
{
    uint64_t value = 0;
    qint64 n = m_file.read(reinterpret_cast<char *>(&value), 8);
    if (n != 8) setError(QStringLiteral("Failed to read U64"));
    return value;
}

int8_t BinaryReader::readS8() { return static_cast<int8_t>(readU8()); }
int16_t BinaryReader::readS16() { return static_cast<int16_t>(readU16()); }
int32_t BinaryReader::readS32() { return static_cast<int32_t>(readU32()); }

QString BinaryReader::readString()
{
    uint16_t length = readU16();
    if (m_error) return QString();
    return readString(length);
}

QString BinaryReader::readString(size_t length)
{
    if (length == 0) return QString();

    size_t rem = remaining();
    if (length > rem) {
        setError(QStringLiteral("String length %1 exceeds remaining file size %2").arg(length).arg(rem));
        return QString();
    }

    QByteArray buf(static_cast<int>(length), Qt::Uninitialized);
    qint64 n = m_file.read(buf.data(), static_cast<qint64>(length));
    if (n != static_cast<qint64>(length)) {
        setError(QStringLiteral("Failed to read string"));
        return QString();
    }
    return QString::fromLatin1(buf);
}

std::vector<uint8_t> BinaryReader::readBytes(size_t count)
{
    size_t rem = remaining();
    if (count > rem) {
        setError(QStringLiteral("Byte count %1 exceeds remaining file size %2").arg(count).arg(rem));
        return {};
    }

    std::vector<uint8_t> result(count);
    if (count > 0) {
        qint64 n = m_file.read(reinterpret_cast<char *>(result.data()), static_cast<qint64>(count));
        if (n != static_cast<qint64>(count)) {
            setError(QStringLiteral("Failed to read bytes"));
            result.clear();
        }
    }
    return result;
}

size_t BinaryReader::tell() const
{
    qint64 pos = m_file.pos();
    if (pos < 0) return static_cast<size_t>(-1);
    return static_cast<size_t>(pos);
}

bool BinaryReader::seek(size_t position)
{
    return m_file.seek(static_cast<qint64>(position));
}

bool BinaryReader::skip(size_t bytes)
{
    return m_file.seek(m_file.pos() + static_cast<qint64>(bytes));
}

size_t BinaryReader::remaining() const
{
    size_t pos = tell();
    if (pos == static_cast<size_t>(-1) || pos > m_fileSize) return 0;
    return m_fileSize - pos;
}

bool BinaryReader::eof() const
{
    return m_file.atEnd() || remaining() == 0;
}

void BinaryReader::setError(const QString &message)
{
    m_error = true;
    m_errorMessage = message;
}

void BinaryReader::clearError()
{
    m_error = false;
    m_errorMessage.clear();
}
