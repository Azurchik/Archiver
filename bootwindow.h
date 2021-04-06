#ifndef BOOTWINDOW_H
#define BOOTWINDOW_H

#include <QDialog>

namespace Ui {
class BootWindow;
}

class BootWindow : public QDialog
{
    Q_OBJECT

public:
    explicit BootWindow(QWidget *parent = nullptr);
    ~BootWindow();

    void initialize(bool compress);
    bool isComplete();

public slots:
    void set_mainMethod(int m_index, int size);
    void set_auxiliaryMethod(int m_index, int size);
    void set_filePath(QString filePath);
    void set_done(int f_index);

private:
    bool Compress;
    bool Complete;
    Ui::BootWindow *ui;
};

#endif // BOOTWINDOW_H
