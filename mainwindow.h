#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDir>
#include <QWidget>
#include <QThread>
#include <QMainWindow>
#include <QFileSystemModel>

#include "winby.h"
#include "bootwindow.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void start(const QStringList fileList, const bool compress);

private slots:
    void on_btn_zip_clicked();
    void on_btn_unzip_clicked();

    void handleErrore(const int errorIndex, const QString &filePath);
    void selectArchive(const QString path);

    void on_btn_left_clicked();
    void on_btn_home_clicked();

    void on_listFiles_doubleClicked(const QModelIndex &index);

    void on_act_about_triggered();
    void on_act_help_triggered();
    void on_act_zip_triggered();
    void on_act_unzip_triggered();

private:
    void switchButtons(bool state);

private:
    Ui::MainWindow *ui;

    QFileSystemModel *model;
    WinBY *archiver;
    BootWindow *bootWindow;

    // QWidget interface
protected:
    virtual void dragEnterEvent(QDragEnterEvent *event);
    virtual void dropEvent(QDropEvent *event);
};

#endif
