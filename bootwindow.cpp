#include "bootwindow.h"
#include "ui_bootwindow.h"

BootWindow::BootWindow(QWidget *parent) :
    QDialog(parent, Qt::MSWindowsFixedSizeDialogHint |
            Qt::WindowCloseButtonHint),
    Complete(false),
    ui(new Ui::BootWindow)
{
    ui->setupUi(this);    

    ui->lbl_main->clear();
    ui->lbl_fPath->clear();
    ui->lbl_auxiliary->clear();
    ui->lbl_Remains->setText("Залишилось:");
    ui->progressBar->setValue(0);
    ui->progressBar_2->setRange(0, 5);

    setWindowTitle("Архівація файлів");
}

BootWindow::~BootWindow()
{
    delete ui;
}

void BootWindow::initialize(bool compress)
{
    Compress = compress;

    if (!compress) {
        setWindowTitle("Розархівація файлів");
        ui->progressBar_2->setRange(0, 4);
    }
}

bool BootWindow::isComplete()
{
    return Complete;
}

void BootWindow::set_mainMethod(int m_index, int size)
{
    if (m_index == -1) {
        close();
    }

    QString text;
    switch (m_index) {
    case 1:
        text = "Сортування файлів по спискам методів";
        break;
    case 2:
        text = "Аналіз списку метода LZW";
        break;
    case 3:
        text = "Аналіз списку метода HFF";
        break;
    case 4:
        text = "Запис файлів файлів до архіву";
        break;
    case 5:
        text = "Видалення допоміжних файлів";
        break;
    case 6:
        text = "Ініціалізація архіву";
        break;
    case 7:
        text = "Зчитення архіву";
        break;
    case 8:
        text = "Розархівування файлів";
        break;
    case 9:
        text = "Видалення допоміжних файлів";
        break;
    case 0:
        text = "Завершенно";
        break;
    }
    ui->lbl_main->setText(text);
    ui->lbl_auxiliary->clear();
    ui->lbl_fPath->clear();

    ui->progressBar->setRange(0, size);
    ui->progressBar->setValue(0);

    if (m_index == 0) {
        int res = Compress ? 5 : 4;

        ui->lbl_Remains->clear();
        ui->progressBar->setVisible(false);
        ui->progressBar_2->setValue(res);
        ui->btn_complete->setEnabled(true);
        Complete = true;
    }
    else if (Compress) {
        ui->progressBar_2->setValue(m_index - 1);
    } else {
        ui->progressBar_2->setValue(m_index - 6);
    }
}

void BootWindow::set_auxiliaryMethod(int m_index, int size)
{
    QString text;
    switch (m_index) {
    case 0:
        text = "Кодування файлів метомдом LZW:";
        break;
    case 1:
        text = "Тестування файлів методом LZW:";
        break;
    case 2:
        text = "Декодування файлів методом LZW:";
        break;
    case 3:
        text = "Ініціалізація ваг для метода HFF";
        break;
    case 4:
        text = "Тестуваня файлів методом HFF:";
        break;
    case 5:
        text = "Кодування файлів методом HFF:";
        break;
    case 6:
        text = "Декодування файлів методом HFF:";
        break;
    case 7:
        text = "Запис ваг у архів";
        break;
    case 8:
        text = "Запис інформації про файлів у архів";
        break;
    case 9:
        text = "Запис даних файлів у архів";
        break;
    case 10:
        text = "Читання інформації про файли з архіву";
        break;
    case 11:
        text = "Читання ваг з архіву";
        break;
    case 12:
        text = "Створення допоміжних файлів";
        break;
    }
    ui->lbl_auxiliary->setText(text);
    ui->lbl_fPath->clear();

    if (size != -1) {
        ui->progressBar->setRange(0, size);
        ui->progressBar->setValue(0);
    }
}

void BootWindow::set_filePath(QString filePath)
{
    ui->lbl_fPath->setText(filePath);
}

void BootWindow::set_done(int f_index)
{
    ui->progressBar->setValue(f_index + 1);
}
