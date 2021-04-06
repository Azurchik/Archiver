#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>

#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>

#include <QFileDialog>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    model = new QFileSystemModel(this);
    model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    model->setRootPath("");
    ui->listFiles->setModel(model);
    ui->lbl_path->setText("\" \"");

    archiver = new WinBY(this);
    bootWindow = new BootWindow(this);

    connect(archiver, SIGNAL(ErrorTrigged(int, QString)),
            this, SLOT(handleErrore(int, QString)) );

    connect(archiver, SIGNAL(archiveFilePath(QString)),
            this, SLOT(selectArchive(QString)) );


    connect(archiver, SIGNAL(bw_mainMethod(int, int)),
            bootWindow, SLOT(set_mainMethod(int, int)) );

    connect(archiver, SIGNAL(bw_auxiliaryMethod(int, int)),
            bootWindow, SLOT(set_auxiliaryMethod(int, int)) );

    connect(archiver, SIGNAL(bw_filePath(QString)),
            bootWindow, SLOT(set_filePath(QString)) );

    connect(archiver, SIGNAL(bw_done(int)),
            bootWindow, SLOT(set_done(int)) );

    setAcceptDrops(true);
}

MainWindow::~MainWindow()
{
    archiver->requestInterruption();
    archiver->wait(60000);
    delete ui;
}

void MainWindow::start(const QStringList fileList, const bool compress)
{
    QFileInfo info(fileList.last());
    on_listFiles_doubleClicked(model->index(info.path()) );

    bootWindow->initialize(compress);
    bootWindow->setModal(true);
    bootWindow->show();

    archiver->initialize(fileList, compress);
    archiver->start();

    if (!bootWindow->exec()) {
        if (!bootWindow->isComplete()) {
            archiver->requestInterruption();
            return;
        }
    }
}

void MainWindow::switchButtons(bool state)
{
    ui->btn_zip->setEnabled(state);
    ui->btn_unzip->setEnabled(state);
}

void MainWindow::on_btn_zip_clicked()
{
    if (ui->lbl_path->text() == "\" \"") {
        QMessageBox::warning(this, "Увага!",
                             "Не можна заархівувати дані/ий файл(и)!");
        return;
    }
    QModelIndexList indexes = ui->listFiles->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
        QMessageBox::warning(this, "Увага!",
                             "Будь ласка, виберіть файл(и).");
        return;
    }
    QStringList list;
    for (QModelIndex index : indexes)
        list << model->filePath(index);

    start(list, true);
}

void MainWindow::on_btn_unzip_clicked()
{    
    QModelIndexList index = ui->listFiles->selectionModel()->selectedIndexes();
    if (index.isEmpty() || index.size() > 1) {
        QMessageBox::warning(this, "Увага!",
                             "Будь ласка, виберіть тільки один файл.");
        return;
    }
    QString path = model->filePath(index.takeLast());
    if (QFileInfo(path).suffix() != archiver->BY_suffix) {
        QMessageBox::warning(this, "Увага!",
                             "Будь ласка, виберіть файл з розширенням \".by\".");
        return;
    }
    QStringList filePath;
    filePath << path;

    start(filePath, false);
}


void MainWindow::handleErrore(const int errorIndex, const QString &filePath)
{
    QString title = "Помилка";
    QMessageBox::Icon icon = QMessageBox::Critical;
    if (errorIndex > 3) {
        title = "Увага!";
        icon = QMessageBox::Warning;
    }
    QString text;
    QString informativeText;


    switch (errorIndex) {
    case 1:
        text = "Помилка Знаходження Файлу.";
        informativeText = filePath + " не існує!";
        break;
    case 2:
        text = "Помилка Читання Файлу.";
        informativeText = filePath + " не було прочитанно!";
        break;
    case 3:
        text = "Помилка Створення Файлу.";
        informativeText = filePath + " не було створено!";
        break;
    case 4:
        text = filePath + " не є файлом \".by\"";
        break;
    case 5:
        text = "Помилка Декодування Файлу.";
        informativeText = filePath + " пошкоджений!";
        break;
    case 6:
        text = filePath + " не є файлом!";
        break;
    case 7:
        text = filePath + " вже є зтиснутий!";
        break;
    case 8:
        text = filePath + " не можливо зтиснути даним методом";
        break;
    case 9:
        text = filePath + " не можливо видалити файл!";
        break;
    }

    QMessageBox msg;
    msg.setWindowTitle(title);
    msg.setIcon(icon);
    msg.setText(text);
    msg.setInformativeText(informativeText);
    msg.setModal(true);
    msg.exec();
}

void MainWindow::selectArchive(const QString path)
{
    QModelIndex index = model->index(path);
    ui->listFiles->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
    ui->listFiles->scrollTo(index, QAbstractItemView::PositionAtCenter);
}


void MainWindow::on_btn_left_clicked()
{
    QFileInfo fileInfo = model->fileInfo(ui->listFiles->rootIndex());
    QDir dir(fileInfo.filePath());
    if (!dir.cdUp()) {
        ui->btn_left->setEnabled(false);
        ui->btn_home->setEnabled(false);
        ui->listFiles->setRootIndex(model->index("") );
        ui->lbl_path->setText("\" \"");
    }
    else {
        ui->listFiles->setRootIndex(model->index(dir.absolutePath()) );
        ui->lbl_path->setText(dir.absolutePath());
    }
}

void MainWindow::on_btn_home_clicked()
{
    ui->listFiles->setRootIndex(model->index(""));
    ui->btn_left->setEnabled(false);
    ui->btn_home->setEnabled(false);

    ui->lbl_path->setText("\" \"");
}


void MainWindow::on_listFiles_doubleClicked(const QModelIndex &index)
{
    QFileInfo fileInfo = model->fileInfo(index);
    if (fileInfo.isDir()) {
        ui->listFiles->setRootIndex(index);

        ui->btn_home->setEnabled(true);
        ui->btn_left->setEnabled(true);

        ui->lbl_path->setText(fileInfo.filePath());
    }
}

void MainWindow::on_act_about_triggered()
{
    QMessageBox msg(this);
    msg.setWindowTitle("Про WinBY");
    msg.setIconPixmap(QPixmap(":/Qt-logo-large.png").scaled(250, 250));
    msg.setText("Дана програма призначенна для архівації файлів і папок,\n"
                "методом Хаффмана(HFF) і алгориитмом Лемпеля-Зіва-Велча(LZW).\n"
                "Написанна мовою:\n"
                "Qt С++ 5.12.2\n"
                "На платформі: Qt Creator 4.9.0\n"
                "Для операційних систем:\n"
                "Windows 7 - 10\n"
                "Розробив: Базько Юрій (Azurchik)");


    msg.setStandardButtons(QMessageBox::Yes);
    msg.setButtonText(QMessageBox::Yes, "Добре");
    msg.exec();
}

void MainWindow::on_act_help_triggered()
{
    QMessageBox msg(this);
    msg.setWindowTitle("Допомога");
    msg.setText("Щоб заархівувати файл(и) є декілька способів:\n"
                "  1. Вибрати файл(и) в списку, також можна вибрати папку, і натиснути на клавішу\"Архівувати\";\n"
                "  2. У вкладці \"Вибрати файл(и)\" вибрати вкладку \"Архівувати\" і вибрати шукані файли.\n"
                "Увага! Другим методом Не можливо заархівувати папки, тільки файли!\n"
                "Щоб розархівувати файл використовуються тіж самі способи, але:\n"
                "  1. Вибрати необіхдно тільки 1 файл;\n"
                "  2. Файл необіхдно вибрати з розширенням \".by\";\n"
                "  3. Замість клавіши/вкладки \"Архівувати\" необхідно вибрати \"Розархівувати\".\n"
                "Також є можливість заархівувати файли і папки, чи розархівувати файл, просто перетянушви їх до вікна програми.\n"
                "  Якщо перетягнути декілька файлів і/або папок, то програма почне архівування.\n"
                "  Якщо перетянути 1 файл, то програма перевірить, чи має це файл розширення \".by\","
                "  якщо так, то почнеться розархівування файлу, в інакшому випадку файл буде заархівовано.\n"
                "Для навігації по списку файлів є 2 кнопки:\n"
                "  1. \"<-\" - повернутись на директорію назад;\n"
                "  2. \"#\" - повернутись на початкову директорію.\n"
                "Після архівації файлу, не важливо: через вкладку, чи за допомогою списку файлів і кнопок,"
                " в списку файлів буде виділений архів, який був створенний.");

    msg.setStandardButtons(QMessageBox::Yes);
    msg.setButtonText(QMessageBox::Yes, "Добре");
    msg.exec();
}

void MainWindow::on_act_zip_triggered()
{
    QFileDialog dialog(this, tr("Виберіть файл для архівації"),
                       "", "All Files (*.*)");
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setViewMode(QFileDialog::Detail);

    QStringList fileList;
    if (dialog.exec()) {
        fileList = dialog.selectedFiles();
    } else {
        return;
    }

    start(fileList, true);
}

void MainWindow::on_act_unzip_triggered()
{
    QFileDialog dialog(this, tr("Виберіть файл для архівації"),
                       "", "BY Files (*.by)");
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setViewMode(QFileDialog::Detail);

    QStringList filePath;
    if (dialog.exec()) {
        filePath = dialog.selectedFiles();
    } else {
        return;
    }

    start(filePath, false);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *e)
{
    QStringList fileList;
    foreach (const QUrl &url, e->mimeData()->urls()) {
        fileList << url.toLocalFile();
    }

    if (fileList.size() == 1) {
        if (QFileInfo(fileList.last()).suffix() == archiver->BY_suffix) {
            start(fileList, false);
        } else {
            start(fileList, true);
        }
    }
    else {
        start(fileList, true);
    }
}
