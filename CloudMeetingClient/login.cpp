#include "login.h"
#include "widget.h"
#include <qhostaddress.h>
#include "NetHeader.h"
#include <qdebug.h>
#include <QtEndian>
#include <qregexp.h>
#include <qmessagebox.h>
#include <qinputdialog.h>
#include <QRegExpValidator>

#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif

extern QUEUE_DATA<MESG> queue_send;

Login::Login(QWidget*parent): QWidget(parent),ui(new Ui::LoginClass())
{
    ui->setupUi(this);
    qRegisterMetaType<MSG_TYPE>();
    ui->username->setText("cba");
    ui->password->setText("123456");
    ui->login->setDisabled(false);
    ui->reg->setDisabled(false);
    ip_ = "139.9.74.30";
    port_ = "8888";
    ui->status->setText("server ipport need to setup!");

    regui = new Register;
    mainWidget = new Widget;
    _mytcpSocket = new TcpSock;

    connect(this, &Login::sendOK2Widget, mainWidget, &Widget::recvOKfromLogin);
    connect(mainWidget, &Widget::send_quit, this, [=]() {
        show();
        mainWidget->hide();
        ui->status->setText("quit success");
        });
    connect(mainWidget, &Widget::send_L, this, [=](bool isnet) {    //从Widget传来的send_L信号, 带参数isnet
        if (isnet) {
            hide(); //隐藏login
            mainWidget->show();     //显示widget
        }
        else {
            ui->status->setText("网络未连接");
        }
        });
    connect(_mytcpSocket, &TcpSock::send_L, this, &Login::dataDeal);    //从TcpSock传来的send_L信号->dataDeal

    regui->hide();

    connect(regui, &Register::sendok, this, [=]() {
        show();
        regui->hide();
        });
    connect(regui, SIGNAL(send_L(QString)),this,SLOT(recv_R(QString)));
}

Login::~Login()
{
    if (_mytcpSocket->isRunning()) {
        _mytcpSocket->stopImmediately();
        _mytcpSocket->wait();
    }
    delete _mytcpSocket;
    delete regui;
    delete mainWidget;
    delete ui;
}

void Login::on_login_clicked() {    //点击登录
    //判断用户名或密码为空
    if (ui->username->text().isNull() || ui->password->text().isNull()) {
        ui->status->setText("用户名或密码为空");
        return;
    }

    //判断是否已连接网络
    if(!_mytcpSocket->isRunning()){
        if(!_mytcpSocket->connectToServer(ip_, port_)){
            ui->status->setText("连接失败!");
            ui->login->setDisabled(false);
            return;
        }
    }
    ui->status->setText("正在发送登录请求...");
    ui->login->setDisabled(true);
    ui->reg->setDisabled(true);

    //构造登录报文，发送到 send消息队列
    QString msg = ui->username->text() + "&" + ui->password->text();
    MESG* send = (MESG*)malloc(sizeof(MESG));
    if (send == nullptr){
        qDebug() << __FILE__ << __LINE__ << "LOGIN malloc fail";
    }
    else {
        send->msg_type = LOGIN;
        send->len = msg.size();
        if(send->len>0) {
            send->data = (uchar*)malloc(send->len);
            if (send->data == nullptr){
                qDebug() << __FILE__ << __LINE__ << "LOGIN send.data malloc error";
                free(send);
            }
            else {
                memset(send->data, 0, send->len);
                memcpy(send->data, msg.toUtf8().data(), send->len);
                queue_send.push_msg(send);
            }
        }
    }
}

void Login::on_reg_clicked() {
    if(!_mytcpSocket->isRunning()){
        ui->status->setText("服务器未连接!");
        return;
    }
    hide();
    regui->show();
}

void Login::on_connect_clicked(){
    if(_mytcpSocket->isRunning()){
        ui->status->setText("网络已连接");
        return;
    }
    if(_mytcpSocket->connectToServer(ip_, port_)){
        ui->status->setText("连接成功");
    }
    else{
        ui->status->setText("网络连接失败");
        ui->login->setDisabled(false);
        ui->reg->setDisabled(false);
    }
}

void Login::on_ipport_clicked() {
    bool ok = false;
    QString text = QInputDialog::getText(this, "Server Setup", "IP and PORT:", QLineEdit::Normal, "139.9.74.30:8888", &ok);
    int i = text.indexOf(":");
    QString ip = text.mid(0, i);
    QString port = text.mid(i + 1, text.size() - i - 1);
    QRegExp ipreg("((2{2}[0-3]|2[01][0-9]|1[0-9]{2}|0?[1-9][0-9]|0{0,2}[1-9])\\.)((25[0-5]|2[0-4][0-9]|[01]?[0-9]{0,2})\\.){2}(25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})");
    QRegExp portreg("^([0-9]|[1-9]\\d|[1-9]\\d{2}|[1-9]\\d{3}|[1-5]\\d{4}|6[0-4]\\d{3}|65[0-4]\\d{2}|655[0-2]\\d|6553[0-5])$");
    QRegExpValidator ipvalidate(ipreg), portvalidate(portreg);
    int pos = 0;
    if (ipvalidate.validate(ip, pos) != QValidator::Acceptable)
    {
        QMessageBox::warning(this, "Input Error", "Ip Error", QMessageBox::Yes, QMessageBox::Yes);
        return;
    }
    if (portvalidate.validate(port, pos) != QValidator::Acceptable)
    {
        QMessageBox::warning(this, "Input Error", "Port Error", QMessageBox::Yes, QMessageBox::Yes);
        return;
    }
    ip_ = ip;
    port_ = port;
    ui->status->setText("server ipport:" + text);
}

void Login::recv_R(QString msg){ //接收框里的注册信息
    MESG* send = (MESG*)malloc(sizeof(MESG));
    if (send == nullptr){   //分配失败
        qDebug() << __FILE__ << __LINE__ << "REGISTER malloc fail";
    }
    else {
        send->msg_type = REGISTER;
        QByteArray byteArray = msg.toUtf8();   //转为ByteArray
        const char* sourceData = reinterpret_cast<const char*>(byteArray.data());   //转为const char*
        send->len = strlen(sourceData);
        if(send->len > 0) {
            send->data = (uchar*)malloc(send->len);
            if (send->data == nullptr){
                qDebug() << __FILE__ << __LINE__ << "malloc error";
                free(send);
            }
            else{
//                memset(send->data, 0, send->len);
//                qDebug() << send->data << " " <<send->len;
                memcpy(send->data, msg.toUtf8().data(), send->len);

//                qDebug()<<"type:"<<send->msg_type<<"len:"<<send->len<<"data:";
//                for(int i = 0;i<send->len;i++){
//                    qDebug()<<QString::number(send->data[i], 16)<<" size:"<<sizeof(send->data[i]);
//                }
                queue_send.push_msg(send);
            }
        }
    }
}

void Login::dataDeal(MESG* msg){    //TcpSocket 收到来自服务端的信息 msg
    QString recvmsg=QString::fromLatin1((char*)msg->data, msg->len);
    if (msg->msg_type == LOGIN_RESPONSE){
        ui->login->setDisabled(false);
        if (recvmsg.contains("login success")) {
            _mytcpSocket->disconnectionFromHost();
            _mytcpSocket->wait();
            emit sendOK2Widget(ip_,port_);
        }
        ui->status->setText(recvmsg);
    }
    else if (msg->msg_type == REGISTER_RESPONSE){
        connect(this, &Login::send_R, regui, &Register::recv_L);
        emit send_R(recvmsg);
    }
    else
        qDebug() << "LOGIN msgtype error";
}
