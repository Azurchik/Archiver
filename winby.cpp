#include "winby.h"

#include <QDebug>

#include <QFile>
#include <QDir>
#include <QMap>
#include <QFileInfo>
#include <QMultiMap>
#include <QByteArray>
#include <QStringList>
#include <QMessageBox>
#include <QTextCodec>

WinBY::WinBY(QObject *parent) :
    QThread(parent),
    HFF_suffix("hff"),
    LZW_suffix("lzw"),
    FLD_suffix("fld"),
    BY_suffix("by"),
    FileSeparator(':'),
    EndHead_f('|'),
    BYTE(8)
{
    suffixList = QStringList {
        "zip", "rar", "7z", "tar", "wim", "pdf",
        "jpg", "gif", "avi", "mpg", "mp3", BY_suffix,
        "mp4"
    };
    Error = false;
}

void WinBY::initialize(QStringList filePathList, bool compress)
{
    list = filePathList;
    itCompress = compress;
}

void WinBY::compress(const QStringList &filePathList)
{
    Error = false;
    // Cписок Всіх файлів.
    QFileInfoList fileList = getAllFiles(filePathList);
    // Списки для конкретних методів стискання.
    QFileInfoList lzw_list;
    QFileInfoList hff_list;

    emit bw_mainMethod(1, fileList.size());
    int counter = 0;
    // Сортуємо по спискам.
    for (QFileInfo info : fileList) {
        emit bw_filePath(info.filePath());

        if (suffixList.indexOf(info.suffix()) == -1) {
            // Перевіряємо, чи великий файл.
            if (info.size() * BYTE <= 235000) {
                lzw_list.push_back(info);
                emit bw_done(counter++);
            } else {
                hff_list.push_back(info);

                emit bw_done(counter++);
            }
        } else {
            hff_list.push_back(info);

            emit bw_done(counter++);
        }
    }
    fileList.clear();
    // Список для вже зжатих файлів.
    QFileInfoList filesCreated;

    emit bw_mainMethod(2, lzw_list.size());
    counter = 0;
    // Аналіз списку "lzw".
    for (QFileInfo info : lzw_list) {
        if (isInterruptionRequested() || Error) {
            removeAll(filesCreated);
            emit bw_mainMethod(-1);
            return;
        }
        emit bw_filePath(info.filePath());

        qint64 lzw_adv = lzw_test(info);
        qint64 hff_adv = hff_test(info);
        if (Error)
            continue;

        if (lzw_adv > hff_adv && lzw_adv > 0) {

            QFile in_file (info.filePath());
            if (!in_file.open(QIODevice::ReadOnly)) {
                emit ErrorTrigged(2, info.filePath());
                Error = true;
                continue;
            }
            QString path = info.filePath() + '.' + LZW_suffix;

            QFile out_file (path);
            if (!out_file.open(QIODevice::WriteOnly)) {
                emit ErrorTrigged(2, path);
                Error = true;
                continue;
            }
            lzw_coding(in_file, out_file);
            filesCreated.push_back(QFileInfo(out_file));

            emit bw_done(counter++);
        } else {
            hff_list.push_back(info);

            emit bw_done(counter++);
        }
    }    
    if (Error || isInterruptionRequested()) {
        removeAll(filesCreated);
        return;
    }

    emit bw_mainMethod(3);
    // Аналізуємо файли зі списку "hff".
    bool usedHff = false;
    quint64 weight[0x100];

    // Зчитуємо всі файли, щоб дізнатись вагу кожного байту.
    initializeHffWeigths(hff_list, weight);

    if (Error || isInterruptionRequested()) {
        removeAll(filesCreated);
        emit bw_mainMethod(-1);
        return;
    }

    if (hff_test(hff_list, weight) > 0) {
        if (Error || isInterruptionRequested()) {
            removeAll(filesCreated);
            emit bw_mainMethod(-1);
            return;
        }

        filesCreated.append(hff_coding(hff_list, weight));
        usedHff = true;

        if (Error || isInterruptionRequested()) {
            removeAll(filesCreated);
            emit bw_mainMethod(-1);
            return;
        }

    } else {
        if (Error || isInterruptionRequested()) {
            removeAll(filesCreated);
            emit bw_mainMethod(-1);
            return;
        }
        emit bw_mainMethod(3, hff_list.size());
        counter = 0;

        QFileInfo info;
        QFileInfoList buff;
        // Знову відкидуємо файли з нестискаючими форматами
        for (QFileInfo info : hff_list) {
            emit bw_filePath(info.filePath());

            if (suffixList.indexOf(info.suffix()) == -1) {
                buff.push_back(info);

                emit bw_done(counter++);
            } else {
                fileList.push_back(info);

                emit bw_done(counter++);
            }
        }
        if (isInterruptionRequested()) {
            removeAll(filesCreated);
            emit bw_mainMethod(-1);
            return;
        }

        initializeHffWeigths(buff, weight);

        if (Error || isInterruptionRequested()) {
            removeAll(filesCreated);
            emit bw_mainMethod(-1);
            return;
        }

        if (hff_test(buff, weight)) {
            if (Error || isInterruptionRequested()) {
                removeAll(filesCreated);
                emit bw_mainMethod(-1);
                return;
            }
            filesCreated.append(hff_coding(buff, weight));
            usedHff = true;
            if (Error || isInterruptionRequested()) {
                removeAll(filesCreated);
                emit bw_mainMethod(-1);
                return;
            }
        } else {
            if (Error || isInterruptionRequested()) {
                removeAll(filesCreated);
                emit bw_mainMethod(-1);
                return;
            }
            fileList.append(buff);
        }
    }
    hff_list.clear();
    lzw_list.clear();

    emit bw_mainMethod(4);

    // Визначаємо шлях до файлу
    QString path;
    if (filePathList.size() == 1) {
        QFileInfo info(filePathList.first());
        path = info.path() + '/' + info.baseName();
    } else {
        QFileInfo info(filePathList.last());
        QString buff = info.path().split('/').takeLast();
        path = info.path() + '/' + buff;
    }
    path += '.' + BY_suffix;
    correctFilePath(path, QDir::Files);

    QFile arch(path);
    if (!arch.open(QIODevice::WriteOnly)) {
        emit ErrorTrigged(3, arch.fileName());
        emit bw_mainMethod(-1);
        removeAll(filesCreated);
        return;
    }
    // Запусуємо основну інформацію.
    arch.write(QString(BY_suffix + FileSeparator).toStdString().data());
    if (usedHff) {
        arch.write(QString(HFF_suffix + FileSeparator).toStdString().data());
    }
    // Архівуємо папки.
    for (QString path__ : filePathList) {
        QFileInfo info(path__);
        if (info.isDir()) {
            QFile file(path__ + '.' + FLD_suffix);
            if (!file.open(QIODevice::WriteOnly)) {
                emit ErrorTrigged(3, file.fileName());
                emit bw_mainMethod(-1);
                removeAll(filesCreated);
                return;
            }
            compressDir(file, QDir(info.filePath()), filesCreated, fileList);

            if (Error || isInterruptionRequested()) {
                arch.close();
                filesCreated.push_back(QFileInfo(arch));
                removeAll(filesCreated);
                emit bw_mainMethod(-1);
                return;
            }
        }
    }
    QFileInfoList allFiles = fileList + filesCreated;
    // Запсуємо інф-ю. файлів.
    writeArchiveFiles(arch, allFiles);

    if (usedHff)
        writeHffWeight(arch, weight);

    if (isInterruptionRequested()) {        
        arch.close();
        filesCreated.push_back(QFileInfo(arch));
        removeAll(filesCreated);
        emit bw_mainMethod(-1);
        return;
    }
    // Записуємо дані файлів.
    writeFilesData(arch, allFiles);

    if (Error || isInterruptionRequested()) {
        arch.close();
        filesCreated.push_back(QFileInfo(arch));
        removeAll(filesCreated);
        emit bw_mainMethod(-1);
        return;
    }

    emit bw_mainMethod(5, filesCreated.size());
    counter = 0;

    for (QFileInfo info : filesCreated) {
        emit bw_filePath(info.filePath());
        QFile::remove(info.filePath());
        emit bw_done(counter++);
    }

    emit bw_mainMethod(0);
    emit archiveFilePath(arch.fileName());
}

void WinBY::decompress(const QString &filePath)
{
    Error = false;

    QFile a_file(filePath);
    if (!a_file.open(QIODevice::ReadOnly)) {
        emit ErrorTrigged(2, filePath);
        return;
    }

    emit bw_mainMethod(6, 2);

    bool haveDict = false;
    QStringList fileList;
    {
        QByteArray buff;
        // Зчитуєм формат архіву в файлі, для ініціалізації.
        while (!a_file.atEnd()) {
            char ch;
            a_file.read(&ch, sizeof (ch));

            if (ch == FileSeparator) {
                if (buff != BY_suffix) {
                    emit ErrorTrigged(5, filePath);
                    emit bw_mainMethod(-1);
                    return;
                } else {
                    buff.clear();
                    break;
                }
            }
            buff.append(ch);
        }

        emit bw_done(0);

        // Перевіряємо, чи використовується Hff.
        while (!a_file.atEnd()) {
            char ch;
            a_file.read(&ch, sizeof (ch));

            if (ch == FileSeparator) {
                haveDict = buff == HFF_suffix;
                if (!haveDict) {
                    fileList.push_back(fromWindowsType(buff));
                }
                break;
            }
            buff.append(ch);
        }
    }
    emit bw_done(1);

    emit bw_mainMethod(7);

    // Зчитуєм назви файлів.
    fileList.append(readArchiveFiles(a_file));

    if (isInterruptionRequested()) {
        return;
    }

    quint64 weight[0x100] = {0};
    if (haveDict)
        readHffWeightt(a_file, weight);

    // Створюємо нерозжаті файли.
    QStringList notUnpackedFiles = createArchiveFile(
                a_file, QFileInfo(a_file).path(), fileList);

    if (Error || isInterruptionRequested()) {
        removeAll(notUnpackedFiles);
        emit bw_mainMethod(-1);
        return;
    }

    emit bw_mainMethod(8, notUnpackedFiles.size());
    int counter = 0;

    // Розстискаємо файли.
    for (int i = 0; i < notUnpackedFiles.size(); ++i) {
        if (isInterruptionRequested() || Error) {
            removeAll(notUnpackedFiles);
            emit bw_mainMethod(-1);
            return;
        }

        QString buff;
        QStringList buff__ = notUnpackedFiles[i].split('.');

        QFile in_file(notUnpackedFiles[i]);
        if (!in_file.open(QIODevice::ReadOnly)) {
            emit ErrorTrigged(2, notUnpackedFiles[i]);
            Error = true;
            continue;
        }        

        emit bw_filePath(in_file.fileName());

        if (buff__.last() == LZW_suffix) {
            buff__.pop_back();
            buff = buff__.join('.');
            correctFilePath(buff, QDir::Files);

            QFile out_file(buff);
            if (!out_file.open(QIODevice::WriteOnly)) {
                emit ErrorTrigged(3, out_file.fileName());
                Error = true;
                continue;
            }
            lzw_decoding(in_file, out_file);
            if (Error)
                continue;

            emit bw_done(counter++);
        }
        else if (buff__.last() == HFF_suffix) {
            buff__.pop_back();
            buff = buff__.join('.');
            correctFilePath(buff, QDir::Files);

            QFile out_file(buff);
            if (!out_file.open(QIODevice::WriteOnly)) {
                emit ErrorTrigged(3, out_file.fileName());
                Error = true;
                continue;
            }
            hff_decoding(in_file, out_file, weight);

            emit bw_done(counter++);
        }
        else if (buff__.last() == FLD_suffix) {
            buff__.pop_back();
            buff = buff__.join('.');
            correctFilePath(buff, QDir::Dirs);

            if (!QDir().mkpath(buff)) {
                emit ErrorTrigged(3, buff);
                Error = true;
                continue;
            }
            notUnpackedFiles.append(decompressDir(in_file, buff));            

            if (Error)
                continue;

            emit bw_mainMethod(8, notUnpackedFiles.size());
            emit bw_done(counter++);
        }
        else {
            // Стираємо зі списку ті, які не були зжаті.
            notUnpackedFiles.removeAt(i);
            --i;

            emit bw_done(--counter);
        }
    }

    emit bw_mainMethod(9, notUnpackedFiles.size());
    counter = 0;

    for (QString filePath : notUnpackedFiles) {
        QFile().remove(filePath);
        emit bw_done(counter++);
    }

    emit bw_mainMethod(0);
}


void WinBY::run()
{
    if (itCompress) {
        compress(list);
    } else {
        decompress(list.last());
    }
}

void WinBY::compressDir(QFile &file, const QDir &dir, QFileInfoList &filesCreated, QFileInfoList &fileList)
{
    QFileInfoList filesDir;
    {
        QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        // Дивимось, чи є папки в цій директорії.
        for (QFileInfo &info : list) {
            if (info.isDir()) {
                QFile arch__(info.filePath() + '.' + FLD_suffix);
                if (!arch__.open(QIODevice::WriteOnly)) {
                    emit ErrorTrigged(3, arch__.fileName());
                    Error = true;
                    return;
                }
                // Робимо рекурсію.
                compressDir(arch__, QDir(info.filePath()), filesCreated, fileList);
                if (Error)
                    return;
            }
        }
        // Робимо список всіх файлів в директорії.
        list = dir.entryInfoList(QDir::Files, QDir::Name);

        QFileInfoList allFiles = fileList + filesCreated;
        // Записуємо тільки ті, які використовуємо, і які є в цій папці.
        for (QFileInfo info : list) {
            if (allFiles.indexOf(info) != -1)
                filesDir << info;
        }
    }
    // Запсуємо інф-ю. файлів.
    writeArchiveFiles(file, filesDir);

    // Записуємо дані файлів.
    writeFilesData(file, filesDir);
    if (Error)
        return;

    // Видаляємо використані файли зі списку.
    for (QFileInfo info : filesDir) {
        int index = filesCreated.indexOf(info);
        // А ті, що створили, видаляємо з комп.
        if (index != -1) {
            QFile::remove(info.filePath());
            filesCreated.removeAt(index);
        } else {
            index = fileList.indexOf(info);
            fileList.removeAt(index);
        }
    }
    filesCreated.push_back(QFileInfo(file));
}

QStringList WinBY::decompressDir(QFile &arch, const QString &dirPath)
{
    QStringList fileList;
    fileList.append(readArchiveFiles(arch));

    QStringList list = createArchiveFile(arch, dirPath, fileList);

    return list;
}

QStringList WinBY::readArchiveFiles(QFile &arch)
{
    emit bw_auxiliaryMethod(10, 0);

    QStringList list;
    QByteArray buff;
    while (!arch.atEnd()) {
        char ch;
        arch.read(&ch, sizeof (ch));

        if (ch == EndHead_f)
            break;
        if (ch == FileSeparator) {
            list.push_back(fromWindowsType(buff));
            buff.clear();
            continue;
        }
        buff.append(ch);
    }
    return list;
}

void WinBY::writeArchiveFiles(QFile &arch, QFileInfoList &filesDir)
{
    emit bw_auxiliaryMethod(8, filesDir.size());
    int counter = 0;

    if (filesDir.size() != 1) {
        for (QFileInfo info : filesDir) {
            emit bw_filePath(info.filePath());

            QString str = info.fileName() + '.'
                    + QString::number(info.size()) + FileSeparator;
            arch.write(toWindowsType(str));

            emit bw_done(counter++);
        }
    } else {
        emit bw_filePath(filesDir.last().filePath());

        QString str = filesDir.last().fileName() + FileSeparator;
        arch.write(toWindowsType(str));

        emit bw_done(counter++);
    }
    arch.write(QByteArray(1, EndHead_f));
}

void WinBY::writeFilesData(QFile &file, QFileInfoList &files)
{
    emit bw_auxiliaryMethod(9, files.size());
    int counter = 0;

    for (QFileInfo info : files) {
        emit bw_filePath(info.filePath());

        QFile file__(info.filePath());
        if (!file__.open(QIODevice::ReadOnly)) {
            emit ErrorTrigged(2, info.filePath());
            Error = true;
            return;
        }
        file.write(file__.readAll());

        emit bw_done(counter++);
    }
}

QStringList WinBY::createArchiveFile(QFile &arch, const QString dirPath, const QStringList fileList)
{
    emit bw_auxiliaryMethod(12, fileList.size());

    QStringList list;

    if (fileList.count() == 1) {
        QString path = dirPath + '/' + fileList.last();
        {
            QString suff = fileList.last().split('.').last();
            if (suff != HFF_suffix || suff != LZW_suffix ||
                    suff != FLD_suffix) {
                correctFilePath(path, QDir::Files);
            }
        }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            emit ErrorTrigged(3, file.fileName());
            Error = true;
            return list;
        }
        while (!arch.atEnd()) {
            file.write(arch.read(1));
        }
        list.push_back(file.fileName());

        emit bw_done(0);
    }
    else {
        int counter = 0;
        for (QString filePath : fileList) {
            QStringList buff__ = filePath.split('.');
            qint64 size = buff__.takeLast().toLongLong();
            QString path = dirPath + '/' + buff__.join('.');
            {
                QString suff = fileList.last().split('.').last();
                if (suff != HFF_suffix || suff != LZW_suffix ||
                        suff != FLD_suffix) {
                    correctFilePath(path, QDir::Files);
                }
            }
            QFile file (path);
            if (!file.open(QIODevice::WriteOnly)) {
                emit ErrorTrigged(3, file.fileName());
                Error = true;
                return list;
            }
            if (size)
                file.write(arch.read(size));

            list.push_back(file.fileName());

            emit bw_done(counter++);
        }
    }
    return list;
}

void WinBY::writeHffWeight(QFile &file, const quint64 weight[])
{
    emit bw_auxiliaryMethod(7, 0);

    int size = 0;
    qsizetype bits_per_weight = 0;
    for (int i = 0; i < 0x100; ++i) {
        if (weight[i]) {
            ++size;
            if (bits_per_weight < maxBitsFromNumber(weight[i]))
                bits_per_weight = maxBitsFromNumber(weight[i]);
        }
    }
    // Віднімаємо 1 тому, що максимальне число може бути 256.
    quint8 characters = static_cast<quint8>(--size);
    file.write(reinterpret_cast<char*>(&characters), sizeof(characters));

    // Записуємо скільки біт буде займати вага кожно байту.
    char ch = static_cast<char>(bits_per_weight);
    file.write(&ch, sizeof(ch));

    // Немає сенсу записувати всі байти.
    if (characters != 255) {
        // Записуєм байти в файл.
        for (qsizetype i = 0; i  < 0x100; ++i) {
            if (weight[i]) {
                char ch = static_cast<char>(i);
                file.write(&ch, sizeof (ch));
            }
        }
    }
    // Записуєм вагу байтів.
    QList<bool> weights;
    for (qsizetype i = 0; i  < 0x100; ++i) {
        if (weight[i]) {
            decToBin(weight[i], weights, bits_per_weight);
        }
    }
    // Необхідно, щоб біти були кратні 8(байту).
    correctBoolList(weights);

    // Запис ваги в файл
    size = weights.size() / BYTE;
    for (int i = 0; i < size; i++) {
        char ch = 0;
        for (int j = 0; j < BYTE; ++j) {
            ch |= weights[i * BYTE + j] << j;
        }
        file.write(&ch, sizeof(ch));
    }
}

void WinBY::readHffWeightt(QFile &file, quint64 weight[])
{
    emit bw_auxiliaryMethod(11, 0);

    char ch;
    file.read(&ch, sizeof(ch));
    qsizetype characters = static_cast<quint8>(ch);

    file.read(&ch, sizeof (ch));
    // Кількість біт, для запису ваги.
    qsizetype bits_per_weight = static_cast<qsizetype>(ch);

    QVector<quint8> bytes;
    // Не забуваєм +1.
    if (characters++ != 255) {
        for (int i = 0; i < characters ; ++i) {
            file.read(&ch, sizeof (ch));
            bytes.push_back(static_cast<quint8>(ch));
        }
    }
    qsizetype weights_size = characters * bits_per_weight;
    // Підрахунок кількості байтів, виділених для запису ваги.
    qsizetype weights_size_in_bytes = weights_size / BYTE;
    weights_size_in_bytes += weights_size % BYTE ? 1 : 0;

    QList<bool> weights;
    for (int i = 0; i < weights_size_in_bytes; ++i) {
        file.read(&ch, sizeof (ch));
        for (int j = 0; j < BYTE; ++j)
            weights.push_back(ch & (1 << j));
    }
    // Видаляємо лишні біти.
    while (weights.size() != weights_size)
        weights.pop_back();

    if (characters == 256) {
        for (int i = 0; i < 0x100; ++i)
            weight[i] = binToDec(weights, bits_per_weight);
    } else {
        for (int i = 0; i < bytes.size(); ++i)
            weight[bytes[i]] = binToDec(weights, bits_per_weight);
    }
}


void WinBY::correctFilePath(QString &filePath, const QDir::Filter filter)
{
    QStringList list = filePath.split('/');
    QString fileName = list.takeLast();
    QString suffix = nullptr;

    if (filter == QDir::Files) {
        QStringList list__ = fileName.split('.');
        fileName = list__.takeFirst();
        suffix = list__.join('.');
    }
    QString mainPath = list.join('/');
    QFileInfoList infoList = QDir(mainPath).entryInfoList(filter | QDir::NoDotAndDotDot, QDir::Name);
    QString tempName;
    int counter = 0;

    if (filter == QDir::Files) {
         tempName = fileName + '.' + suffix;
    } else {
        tempName = fileName;
    }

    QListIterator<QFileInfo> i(infoList);
    while (i.hasNext()) {
        if (i.next().fileName().toUpper() == tempName.toUpper()) {
            if (filter == QDir::Files) {
                tempName = fileName + '-' + QString::number(counter++) + '.' + suffix;
            } else {
                tempName = fileName + '-' + QString::number(counter++);
            }
            i.toFront();
        }
    }

    filePath = mainPath + '/' + tempName;
}

void WinBY::initializeHff(const quint64 weight[]
                          , QMap<quint8, qint16> &charMap
                          , QVector<WinBY::Node> &tree)
{
    // Сортування вузлів по вазі.
    QMultiMap<quint64 /*вага*/, qint16 /*індекс в дереві*/> sortedWeight;

    for (qsizetype i = 0;  i < 0x100; ++i) {
        if (weight[i]) {
            tree.push_back(Node{i, i, -1, false});
            charMap[i] = static_cast<qint16>(tree.size() - 1);
            sortedWeight.insert(weight[i], static_cast<qint16>(tree.size() - 1));
        }
    }
    // Заповнюємо дерево новими вузлами.
    while (sortedWeight.size() > 1) {
        quint64 w0  = sortedWeight.begin().key();
        qint16 n0 = sortedWeight.begin().value();
        sortedWeight.erase(sortedWeight.begin());
        quint64 w1  = sortedWeight.begin().key();
        qint16 n1 = sortedWeight.begin().value();
        sortedWeight.erase(sortedWeight.begin());

        tree.push_back(Node{n0, n1, -1, false});
        tree[n0].parent = static_cast<qint16>(tree.size() - 1);
        tree[n0].branch = false;
        tree[n1].parent = static_cast<qint16>(tree.size() - 1);
        tree[n1].branch = true;
        sortedWeight.insert(w0 + w1, static_cast<qint16>(tree.size() - 1));
    }
}

void WinBY::lzw_coding(QFile &in_file, QFile &out_file)
{
    QByteArrayList bytesList;
    initializeBytesList(bytesList);

    QList<bool> data;
    qsizetype numberBits = BYTE;
    QByteArray previous;
    QByteArray current;
    QByteArray pre_cur;

    do {
        if (bytesList.size() > maxFromNumberBits(numberBits) + 1)
            numberBits++;

        current = in_file.read(1);
        pre_cur = previous + current;

        if (bytesList.indexOf(pre_cur) == -1)  {
            bytesList.push_back(pre_cur);
            decToBin(bytesList.indexOf(previous), data, numberBits);
            previous = current;
        } else {
            previous = pre_cur;
        }

    } while (!in_file.atEnd());

    decToBin(bytesList.indexOf(previous), data, numberBits);
    correctBoolList(data);
    qint64 size = data.size() / 8;

    for (int i = 0; i < size; ++i) {
        char ch = 0;
        for (int j = BYTE - 1; j >= 0; --j) {
            if (data.takeFirst())
                ch |= (1 << j);
        }
        out_file.write(&ch, sizeof(ch));
    }
}

void WinBY::lzw_decoding(QFile &in_file, QFile &out_file)
{
    emit bw_auxiliaryMethod(2, -1);

    QByteArrayList bytesList;
    initializeBytesList(bytesList);

    QList<bool> data;
    const int mainListSize = bytesList.size();
    qsizetype numberBits = BYTE;

    while (!in_file.atEnd()) {
        if (bytesList.size() > maxFromNumberBits(numberBits) + 1)
            numberBits++;
        // reading bytes
        int bitsLeft = numberBits - data.size();
        // Проверяем, достаточно ли нам битов.
        if (bitsLeft) {
            int n_bytes = bitsLeft / BYTE;
            n_bytes += bitsLeft % BYTE ? 1 : 0;

            char *buff = new char[n_bytes];
            in_file.read(buff, n_bytes);

            for (int i = 0; i < n_bytes; ++i) {
                for (int j = BYTE - 1; j >= 0; --j)
                    data.push_back(buff[i] & (1 << j));
            }
        }
        // decoding
        qint64 index = binToDec(data, numberBits);
        if (index >= bytesList.count()) {
            out_file.close();
            QFile::remove(out_file.fileName());
            emit ErrorTrigged(5, in_file.fileName());
            Error = true;
            return;
        }
        QByteArray current = bytesList[index];
        out_file.write(current);

        if (bytesList.size() != mainListSize) {
            char ch_cur = current.toStdString()[0];
            bytesList[bytesList.size() - 1] += ch_cur;
        }
        bytesList.push_back(current);
    }
}

qint64 WinBY::hff_test(const QFileInfo &fileInfo)
{
    emit bw_auxiliaryMethod(4, 0);

    QFileInfoList list;
    list << fileInfo;

    quint64 weight[0x100];
    initializeHffWeigths(list, weight);

    return hff_test(list, weight);
}

qint64 WinBY::lzw_test(QFile &file)
{
    QByteArrayList bytesList;
    initializeBytesList(bytesList);

    quint64 bits_size = 0;

    quint64 numberBits = BYTE;
    QByteArray previous;
    QByteArray current;
    QByteArray pre_cur;
    do {
        if (bytesList.size() > maxFromNumberBits(numberBits) + 1)
            numberBits++;

        current = file.read(1);
        pre_cur = previous + current;

        if (bytesList.indexOf(pre_cur) == -1)  {
            bytesList.push_back(pre_cur);
            bits_size += numberBits;
            previous = current;
        } else {
            previous = pre_cur;
        }

    } while (!file.atEnd());
    bits_size += numberBits;

    qint64 dataSize = bits_size % BYTE ? (bits_size / 8) + 1
                                       : bits_size / 8;
    file.reset();
    return file.size() - dataSize;
}

qint64 WinBY::lzw_test(const QFileInfo &fileInfo)
{
    emit bw_auxiliaryMethod(1, 0);

    QFile file(fileInfo.filePath());
    if (!file.open(QIODevice::ReadOnly)) {
        Error = true;
        emit ErrorTrigged(2, file.fileName());
        return  -1;
    }
    return lzw_test(file);
}

qint64 WinBY::hff_test(const QFileInfoList &list, const quint64 weight[])
{
    emit bw_auxiliaryMethod(4, list.size());
    int counter = 0;

    QFileInfo info;
    qint64 hff_s = 0;
    qint64 file_s = 0;   

    foreach (info, list) {
        emit bw_filePath(info.filePath());

        QFile file(info.filePath());
        if (!file.open(QIODevice::ReadOnly)) {
            Error = true;
            emit ErrorTrigged(2, info.filePath());
            return 0;
        }
        QVector<Node> tree;
        tree.reserve(256 + 255);
        // карта, для нахождения символа в дереве
        QMap<quint8/*number char*/, qint16 /*index in the tree*/> charMap;

        initializeHff(weight, charMap, tree);

        quint64 dataSize = 0;
        QMapIterator<quint8, qint16> cm(charMap);
        while (cm.hasNext()) {
            cm.next();

            Node n = tree[cm.value()];
            int bits_size = 0;
            while (n.parent != -1) {
                ++bits_size;
                n = tree[n.parent];
            }
            dataSize += weight[cm.key()] * bits_size;
        }

        hff_s += dataSize % BYTE ? (dataSize / BYTE) + 1 : dataSize / BYTE;
        file_s += info.size();

        emit bw_done(counter++);
    }

    int bits_per_weight = 0;
    int usedBytes = 0;
    for (int i = 0; i < 0x100; ++i) {
        if (weight[i])
            ++usedBytes;
        if (bits_per_weight < maxBitsFromNumber(weight[i]))
            bits_per_weight = maxBitsFromNumber(weight[i]);
    }

    int dictSize = usedBytes == 256 ? bits_per_weight * usedBytes
                                      : (bits_per_weight + BYTE) * usedBytes;
    hff_s += dictSize % BYTE ? (dictSize / BYTE) + 1 : dictSize / BYTE;

    return file_s - hff_s;
}

void WinBY::hff_coding(QFile &in_file, QFile &out_file, const quint64 weight[])
{
    // Дерево
    QVector<Node> tree;
    tree.reserve(256 + 255);
    // Kарта, для знахождения байту в дереві.
    QMap<quint8/*номер байту*/, qint16 /*індекс в дереві*/> charMap;
    // Максимальна кількість бітів, яка необхідна для запису вагів листя.
    initializeHff(weight, charMap, tree);

    QList<bool> data;
    // Кодуємо дані.
    while (!in_file.atEnd()) {
        QByteArray bff = in_file.read(1);
        Node n = tree[charMap[static_cast<quint8>(bff.back())]];

        QList<bool> bit;
        while (n.parent != -1) {
            bit.push_back(n.branch);
            n = tree[n.parent];
        }
        // Записуємо в зворотньому напрямі.
        int size = bit.size();
        for (int i = 0; i < size; ++i)
            data.push_back(bit.takeLast());
    }

    qsizetype index = data.size();
    // Кількість добавлених біт.
    int times = correctBoolList(data);

    // Якщо нема - добавляєм біти самі.
    if (!times) {
        for (int i = 0; i < BYTE; ++i)
            data.push_back(0);
    }
    // Перший - "1", щоб визначити кінець даних, а все інше 0.
    data[index] = 1;

    // Записуємо дані в файл.
    for (qsizetype i = 0; i < data.size() / BYTE; ++i) {
        char ch = 0;
        for (int j = 0; j < BYTE; ++j) {
            if (data[i * BYTE + j])
               ch |= (1 << j);
        }
        out_file.write(&ch, sizeof (ch));
    }
}

void WinBY::hff_decoding(QFile &in_file, QFile &out_file, const quint64 weight[])
{
    emit bw_auxiliaryMethod(6, -1);

    // Дерево.
    QVector<Node> tree;
    // Карта.
    QMap<quint8/*number char*/, qint16 /*index in the tree*/> charMap;

    initializeHff(weight, charMap, tree);

    char ch;
    QList<bool> data;
    while (!in_file.atEnd()) {
        in_file.read(&ch, sizeof (ch));

        for (int i = 0; i < BYTE; ++i)
            data.push_back((ch & (1 << i)) != 0);
    }
    // Видаляємо лишні біти.
    while (data.takeLast() != 1);

    // Декодуємо.
    qsizetype n = tree.size() - 1;
    for (int i = 0; i < data.size(); ++i) {
        if (data[i]) {
            n = tree[n].one;
        } else {
            n = tree[n].zero;
        }
        if (tree[n].zero == tree[n].one) {
            ch = static_cast<char>(tree[n].one);
            out_file.write(&ch, sizeof(ch));
            n = tree.size() - 1;
        }
    }
}

QFileInfoList WinBY::hff_coding(const QFileInfoList &list, const quint64 weight[])
{
    emit bw_auxiliaryMethod(5, list.size());
    int counter = 0;

    QFileInfo info;
    QFileInfoList result;
    foreach (info, list) {
        emit bw_filePath(info.filePath());

        QFile in_file(info.filePath());
        if (!in_file.open(QIODevice::ReadOnly)) {
            emit ErrorTrigged(2, info.filePath());
            Error = true;
            break;
        }

        QFile out_file(info.filePath() + '.' + HFF_suffix);
        if (!out_file.open(QIODevice::WriteOnly)) {
            emit ErrorTrigged(3, out_file.fileName());
            Error = true;
            break;
        }
        hff_coding(in_file, out_file, weight);
        result.push_back(QFileInfo(out_file));

        emit bw_done(counter++);
    }
    return result;
}

void WinBY::initializeHffWeigths(const QFileInfoList &list, quint64 weight[])
{
    emit bw_auxiliaryMethod(3, list.size());
    int i;

    for (i = 0; i < 0x100; ++i)
        weight[i] = 0;

    i = 0;
    QFileInfo info;
    foreach (info, list) {
        emit bw_filePath(info.filePath());

        QFile file (info.filePath());
        if (!file.open(QIODevice::ReadOnly)) {
            emit ErrorTrigged(2, info.filePath());
            Error = true;
            return;
        }
        char ch;
        while (!file.atEnd()) {
            file.read(&ch, 1);
            ++weight[static_cast<quint8>(ch)];
        }

        emit bw_done(i++);
    }
    return;
}

void WinBY::initializeBytesList(QByteArrayList &charsList)
{
    charsList.clear();
    for (int i = 0; i < 0x100; ++i) {
        charsList.push_back(QByteArray(1, static_cast<char>(i)) );
    }
}

QFileInfoList WinBY::getAllFiles(const QStringList &filePathList)
{
    QFileInfoList result;
    QStringList list = filePathList;

    for (int i = 0; i < list.size(); ++i) {
        QFileInfo info(list[i]);
        if (!info.isDir()) {
            result.push_back(info);
        }
        else {
            QFileInfoList infolist = QDir(list[i]).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot
                                                                 , QDir::DirsFirst | QDir::Name);
            for (QFileInfo info__ : infolist)
                list << info__.filePath();
        }
    }
    return result;
}

QByteArray WinBY::toWindowsType(const QString &text)
{
    return QTextCodec::codecForName("Windows-1251")->fromUnicode(text);
}

QString WinBY::fromWindowsType(const QByteArray &arr)
{
    QTextCodec* defaultTextCodec = QTextCodec::codecForName("Windows-1251");
    QTextDecoder *decoder = new QTextDecoder(defaultTextCodec);
    return  decoder->toUnicode(arr);
}

void WinBY::removeAll(QFileInfoList list)
{
    for (QFileInfo inf : list)
        QFile().remove(inf.filePath());
}

void WinBY::removeAll(QStringList list)
{
    for (QString path : list)
        QFile().remove(path);
}

void WinBY::decToBin(const quint64 number, QList<bool> &data, const qsizetype numberBits)
{
    QList<bool> answer;
    quint64 result = number;
    do {
        answer.push_back(result % 2);
        result /= 2;
    } while (result >= 2);
    answer.push_back(result);

    qsizetype size = answer.size();
    for (int i = 0; i < numberBits - size; ++i)
        data.push_back(0);

    for (int i = 0; i < size; ++i)
        data.push_back(answer.takeLast());
}


qint64 WinBY::binToDec(QList<bool> &data, const qsizetype numberBits, const bool back)
{
    qint64 result = 0;
    if (back) {
        for (int i = 0; i < numberBits; ++i) {
            if (data.takeLast())
                result += qPow(2, i);
        }
    } else {
        for (int i = numberBits - 1; i >= 0; --i) {
            if (data.takeFirst())
                result += qPow(2, i);
        }
    }
    return result;
}

int WinBY::correctBoolList(QList<bool> &data)
{
    int times = BYTE - (data.size() % BYTE);
    // Если число кратно 8, "data.size() % BYTE == 0".
    if (times != BYTE) {
        // Заполняем нулями.
        for (int i = 0; i < times; ++i)
            data.push_back(0);
    }
    return (times == BYTE) ? 0 : times;
}

qsizetype WinBY::maxBitsFromNumber(const qsizetype number)
{
    qsizetype result = 0;
    qsizetype buff = number;
    do {
        buff = buff / 2;
        ++result;
    }
    while (buff >= 2);
    return ++result;
}

qsizetype WinBY::maxFromNumberBits(const qsizetype numberBits)
{
    qsizetype result = 0;
    for (qsizetype i = 0; i < numberBits; ++i) {
        result += pow(2, i);
    }
    return result;
}

