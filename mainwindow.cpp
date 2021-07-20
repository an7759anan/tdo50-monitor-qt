#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSerialPortInfo>
#include <QMessageBox>

#define C1 0x41; // чтение конфигурационных байт из EEPROM
#define C2 0x42; // запись конфигурационных байт в EEPROM
#define C3 0x43; // чтение текущего состояния датчиков
#define C4 0x44; // чтение заводского номера устройства

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QList<QSerialPortInfo> portInfffoList = QSerialPortInfo::availablePorts();
    ui->comboBox_comport->addItem("Выберите COM-порт");
    for (QSerialPortInfo portInfo: portInfffoList){
        ui->comboBox_comport->addItem(portInfo.portName());
    }

    for (int i=1; i<256; i++) {
        ui->comboBox_modbus_address->addItem(QString::number(i),i);
    }

    for (int i=0; i<50; i++){
        QComboBox *combo = new QComboBox(ui->tableWidget);
        combo->addItem("Отключено",0);
        combo->addItem("На замыкание",1);
        combo->addItem("На размыкание",2);
        ui->tableWidget->setCellWidget(i,1,combo);

        QIcon icon(":/icon-gray");
        QTableWidgetItem *icon_item = new QTableWidgetItem;
        icon_item->setIcon(icon);

        ui->tableWidget->setItem(i,0,icon_item);
    }

    connect(this->ui->comboBox_comport, SIGNAL(currentIndexChanged(int)), this, SLOT(onComboboxPortselectorChange(int)));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onComboboxPortselectorChange(int val){
    if (serialPort.isOpen()){
        serialPort.close();
    }
    if (val > 0){
        QString new_port_name = ui->comboBox_comport->currentText();
        QSerialPortInfo *serialPortInfo = new QSerialPortInfo(new_port_name);
        if (!serialPortInfo->isNull() && !serialPortInfo->isBusy()){
            serialPort.setPort(*serialPortInfo);
            serialPort.setParity(QSerialPort::Parity::NoParity);
            serialPort.setDataBits(QSerialPort::DataBits::Data8);
            serialPort.setBaudRate(QSerialPort::BaudRate::Baud9600);
            serialPort.setStopBits(QSerialPort::StopBits::OneStop);
            serialPort.setFlowControl(QSerialPort::FlowControl::NoFlowControl);
            serialPort.setReadBufferSize(25);
            if (serialPort.open(QIODevice::ReadWrite)){
                serialPort.clear();
                enableControls(true);
            } else {
                QMessageBox::warning(this,"Ошибка открытия COM-порта",serialPort.errorString());
                serialPort.clearError();
            }
        } else {
            QMessageBox::warning(this,"Ошибка открытия COM-порта","Port not exists or busy");
        }
    } else {

    }
}

void MainWindow::enableControls(bool enabled){
    ui->comboBox_modbus_address->setEnabled(enabled);
    ui->pushButton_C1->setEnabled(enabled);
    ui->pushButton_C2->setEnabled(enabled);
    ui->pushButton_C3->setEnabled(enabled);
    ui->pushButton_C4->setEnabled(enabled);
}

bool MainWindow::readFromSerialPort(int size){
    data_in.clear();
    if (serialPort.waitForReadyRead(1000)){
        data_in = serialPort.readAll();
        while (serialPort.waitForReadyRead(100)) data_in += serialPort.readAll();
        if (data_in.length() < size){
            QMessageBox::warning(this,"Нет ответа","Нет ответа от устройства");
            return false;
        } else {
            return true;
        }
    }
    QMessageBox::warning(this,"Нет ответа","Нет ответа от устройства");
    return false;
}

// чтение конфигурационных байт из EEPROM ======================>
void MainWindow::on_pushButton_C1_clicked(){
    serialPort.clear();
    enableControls(false);
    prepare_C1_request();
    serialPort.write(data, 4);
    if (readFromSerialPort(25)){
        perform_C1_answer();
    }
    enableControls(true);
}

void MainWindow::prepare_C1_request(){
    data[0] = ui->comboBox_modbus_address->currentData().toInt();
    data[1] = C1;
    CRC16(data, 2);
}

void MainWindow::perform_C1_answer(){
    for(int byteNum = 0; byteNum < 7; byteNum++){
        for (int bitNum = 0; bitNum < 8; bitNum++){
            int modelIdx = 8 * byteNum + bitNum;
            if (modelIdx < 50){
                QComboBox *combo = (QComboBox*)ui->tableWidget->cellWidget(modelIdx, 1);
                if (data_in[2 + byteNum + 7] & 1<<bitNum){ // usedSignals
                    if (data_in[2 + byteNum] & 1<<bitNum){ // polarities
                        // используется - на Замыкание
                        combo->setCurrentIndex(1);
                    } else {
                        // используется - на Размыкание
                        combo->setCurrentIndex(2);
                    }
                } else {
                    // не используется
                    combo->setCurrentIndex(0);
                }
            }
        }
    }
}
// ==============================================================

// запись конфигурационных байт в EEPROM =======================>
void MainWindow::on_pushButton_C2_clicked(){
    serialPort.clear();
    enableControls(false);
    prepare_C2_request();
    serialPort.write(data, 25);
    serialPort.waitForBytesWritten(1000);
    enableControls(true);
}
void MainWindow::prepare_C2_request(){
//    let buf = [modbusAddress,C2,...data.polarities,...data.usedSignals,...data.typeSignals];
    data[0] = ui->comboBox_modbus_address->currentData().toInt();
    data[1] = C2;
    for(int byteNum = 0; byteNum < 7; byteNum++){
        data[2 + byteNum] = 0; // polarities
        data[2 + byteNum + 7] = 0; // usedSignals
        data[2 + byteNum + 14] = 0; // typeSignals
        for (int bitNum = 0; bitNum < 8; bitNum++){
            int modelIdx = 8 * byteNum + bitNum;
            if (modelIdx < 50){
                QComboBox *combo = (QComboBox*)ui->tableWidget->cellWidget(modelIdx, 1);
                switch (combo->currentIndex()){
                case 0: // не используется
                    break;
                case 1: // используется - на Замыкание
                    data[2 + byteNum + 7] = data[2 + byteNum + 7] | 1<<bitNum; // usedSignals
                    data[2 + byteNum] = data[2 + byteNum] | 1<<bitNum; // polarities
                    break;
                case 2: // используется - на Размыкание
                    data[2 + byteNum + 7] = data[2 + byteNum + 7] | 1<<bitNum; // usedSignals
                    break;
                default:
                    break;
                }
                QString signal_type = ui->tableWidget->item(modelIdx,2)->text();
                if (signal_type == "ЭАС"){
                    data[2 + byteNum + 14] = data[2 + byteNum + 14] | 1<<bitNum; // typeSignals
                }
            }
        }
    }
    CRC16(data, 23);
}
// ==============================================================

// чтение текущего состояния датчиков
void MainWindow::on_pushButton_C3_clicked(){
    serialPort.clear();
    enableControls(false);
    prepare_C3_request();
    serialPort.write(data, 4);
    if (readFromSerialPort(17)){
        perform_C3_answer();
    }
    enableControls(true);
}

void MainWindow::prepare_C3_request(){
    data[0] = ui->comboBox_modbus_address->currentData().toInt();
    data[1] = C3;
    CRC16(data, 2);
}

const QString CIRCLES[] = {":/icon-gray", ":/icon-gray", ":/icon-green", ":/icon-red"};

void MainWindow::perform_C3_answer(){
    const int dataLength = 13;
    for (int idx = 0; idx < 13; idx++){
        int idx1 = 4*idx;
        ui->tableWidget->item(idx1,0)->setIcon(*new QIcon(CIRCLES[0x3 & data_in[2 + idx]])); // memory leak ?
        ui->tableWidget->item(idx1+1,0)->setIcon(*new QIcon(CIRCLES[0x3 & data_in[2 + idx]>>2])); // memory leak ?
        if (idx < dataLength - 1){
            ui->tableWidget->item(idx1+2,0)->setIcon(*new QIcon(CIRCLES[0x3 & data_in[2 + idx]>>4])); // memory leak ?
            ui->tableWidget->item(idx1+3,0)->setIcon(*new QIcon(CIRCLES[0x3 & data_in[2 + idx]>>6])); // memory leak ?
        }
    }
}

// ==============================================================

// чтение заводского номера устройства ==================>
void MainWindow::on_pushButton_C4_clicked(){
    ui->label_serial_number->setText("Заводской номер:");
    serialPort.clear();
    enableControls(false);
    prepare_C4_request();
    serialPort.write(data, 4);
    if (readFromSerialPort(8)){
        perform_C4_answer();
    }
    enableControls(true);
}

void MainWindow::prepare_C4_request(){
    data[0] = ui->comboBox_modbus_address->currentData().toInt();
    data[1] = C4;
    CRC16(data, 2);
}

void MainWindow::perform_C4_answer(){
    QString text = "";
    for (int idx = 0; idx < 4; idx++){
        if (idx < 3){
            text += QString("%1").arg((ushort)data_in[2 + idx], 2, 16, QChar('0'));
        } else {
            text += QString("%1").arg((ushort)data_in[2 + idx]>>4, 1, 16);
        }
    }
    ui->label_serial_number->setText("Заводской номер: " + text);
}
// =======================================================

void MainWindow::CRC16(char* pcBlock, int len){
    unsigned int crc = 0xFFFF;
    int j = 0;
    int _len = len;
    while (_len--)
    {
        crc ^= pcBlock[j++] << 8;
        for (int i = 0; i < 8; i++)
        crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    pcBlock[len] = 0xff & crc;
    pcBlock[len + 1] = 0xff & crc>>8;
}
