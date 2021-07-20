#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QSerialPort serialPort;
    QByteArray data_in;
    char data[25];
    bool readFromSerialPort(int size);

    void enableControls(bool);

    void prepare_C1_request();
    void prepare_C2_request();
    void prepare_C3_request();
    void prepare_C4_request();

    void perform_C1_answer();
    void perform_C3_answer();
    void perform_C4_answer();

//    void CRC16(QByteArray, int);
    void CRC16(char*, int);

public slots:
    void onComboboxPortselectorChange(int);
    void on_pushButton_C1_clicked();
    void on_pushButton_C2_clicked();
    void on_pushButton_C3_clicked();
    void on_pushButton_C4_clicked();
};

#endif // MAINWINDOW_H
