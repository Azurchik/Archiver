#ifndef WINBY_H
#define WINBY_H

#include <QDir>
#include <QtMath>
#include <QObject>
#include <QString>
#include <QThread>

class WinBY : public QThread
{
    Q_OBJECT
public:
    explicit WinBY(QObject *parent = nullptr);

    void initialize(QStringList filePathList, bool compress);

    void compress(const QStringList &filePathList);
    void decompress(const QString &filePath);

protected:
    virtual void run();

signals:
    void ErrorTrigged(int index, QString Path);

    void archiveFilePath(QString path);

    void bw_mainMethod(int m_index, int size = 0);
    void bw_auxiliaryMethod(int m_index, int size);
    void bw_filePath(QString filePath);
    void bw_done(int f_index);

private:
    void compressDir(QFile &file, const QDir &dir, QFileInfoList &filesCreated,
                     QFileInfoList &fileList);
    QStringList decompressDir(QFile &arch, const QString &dirPath);

    qint64 lzw_test(QFile &file);
    qint64 lzw_test(const QFileInfo &fileInfo);
    void lzw_coding(QFile &in_file, QFile &out_file);
    void lzw_decoding(QFile &in_file, QFile &out_file);

    qint64 hff_test(const QFileInfo &fileInfo);
    qint64 hff_test(const QFileInfoList &list, const quint64 weight[0x100]);
    void hff_coding(QFile &in_file, QFile &out_file, const quint64 weight[0x100]);  
    void hff_decoding(QFile &in_file, QFile &out_file, const quint64 weight[0x100]);
    QFileInfoList hff_coding(const QFileInfoList &list, const quint64 weight[0x100]);    

    QStringList readArchiveFiles(QFile &arch);
    void writeArchiveFiles(QFile &arch, QFileInfoList &filesDir);
    void writeFilesData(QFile &file, QFileInfoList &files);
    QStringList createArchiveFile(QFile &arch, const QString dirPath,
                                  const QStringList fileList);
    void readHffWeightt(QFile &file, quint64 weight[0x100]);
    void writeHffWeight(QFile &file, const quint64 weight[0x100]);

    struct Node {
            qsizetype zero; // Індекс вузла по вітці 0.
            qsizetype one;  // Індекс вузла по вітці 1.
            qint16 parent; // Індекс вузла батька.
            bool branch; // Номер вітки, який вузол віддає(0/1).
            // Функція порівняння.
            friend bool operator==(const Node &lhs, const Node &rhs)
            {
                return (lhs.one == rhs.one) && (lhs.zero == rhs.zero) &&
                       (lhs.branch == rhs.branch) && (lhs.parent == rhs.parent);
            }
    };

private:
    void initializeHffWeigths(const QFileInfoList &list, quint64 weight[0x100]);
    void initializeBytesList(QByteArrayList &charsList);

    QFileInfoList getAllFiles(const QStringList &filePathList);

    void correctFilePath(QString &filePath, const QDir::Filter filter);
    int correctBoolList(QList<bool> &data);

    qint64 binToDec(QList<bool> &data, const qsizetype numberBits, const bool back = false);
    void decToBin(const quint64 number, QList<bool> &data, const qsizetype numberBits);    

    qsizetype maxBitsFromNumber(const qsizetype number);
    qsizetype maxFromNumberBits(const qsizetype numberBits);

    void initializeHff(const quint64 weight[0x100], QMap<quint8, qint16> &charMap,
                              QVector<Node> &tree);

    QByteArray toWindowsType(const QString &text);
    QString fromWindowsType(const QByteArray &arr);

    void removeAll(QFileInfoList list);
    void removeAll(QStringList list);

private:
    // Формати файлів, які вже зжаті.
    QStringList suffixList;
    // Типи файлів, при архівації.
    const QString HFF_suffix;
    const QString LZW_suffix;
    const QString FLD_suffix;

public:
    // Формат данного архіву.
    const QString BY_suffix;

private:
    // Дані для ініціалізації потоку.
    QStringList list;
    bool itCompress;
    // Датчик помилки.
    bool Error;
    // Роздільники.
    const char FileSeparator;
    const char EndHead_f;
    // Біти в байті.
    const int BYTE;
};

#endif // WINBY_H
