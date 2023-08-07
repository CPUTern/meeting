#include "widget.h"
#include "ui_widget.h"
#include "TcpSock.h"
#include "RecvDeal.h"
#include "SendText.h"
#include "SendImg.h"
#include "AudioInput.h"
#include "AudioOutput.h"
#include "MyVideoSurface.h"
#include <qmessagebox.h>
#include <qhostaddress.h>
#include <qdatetime.h>
#include <qsound.h>
#include <qscrollbar.h>
#include <qfiledialog.h>
#include <qtextcodec.h>

#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif

extern QUEUE_DATA<MESG> queue_send;
extern QUEUE_DATA<MESG> queue_recv;
extern QUEUE_DATA<MESG> audio_recv;

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    qRegisterMetaType<MSG_TYPE>();

    layout=new QVBoxLayout(ui->scrollAreaWidgetContents);
    layout->setSpacing(3);

    _createmeet = false;
    _openCamera = false;
    _joinmeet = false;

    ui->exitmeetBtn->setDisabled(true);
    ui->joinmeetBtn->setDisabled(true);
    ui->createmeetBtn->setDisabled(true);
    ui->openAudio->setDisabled(true);
    ui->openVideo->setDisabled(true);
    ui->sendmsg->setDisabled(false);
    ui->sendimg->setDisabled(true);

    _sendImg = new SendImg();
    _imgThread = new QThread();
    _sendImg->moveToThread(_imgThread); //sendImg放到imgThread线程中执行
    _sendImg->start();  //start sendImg线程

    _mytcpSocket = new TcpSock();
    connect(_mytcpSocket,&TcpSock::sendTextOver,this,&Widget::textSend);

    _sendText = new SendText();
    _textThread = new QThread();
    _sendText->moveToThread(_textThread);
    _textThread->start();
    _sendText->start();
    connect(this, SIGNAL(sigPushTextMsgToTextQueue(MSG_TYPE, QString)), _sendText, SLOT(pushTextMsgToTextQueue(MSG_TYPE, QString)));

    _camera = new QCamera(this);
    connect(_camera, SIGNAL(error(QCamera::Error)), this, SLOT(cameraError(QCamera::Error)));
    _imagecapture = new QCameraImageCapture(_camera);
    _myvideosurface = new MyVideoSurface(this);

    connect(_myvideosurface, SIGNAL(frameAvailable(QVideoFrame)), this, SLOT(cameraImageCapture(QVideoFrame)));
    connect(this, SIGNAL(sigPushToImgQueue(QImage)), _sendImg, SLOT(ImageCapture(QImage)));
    connect(_imgThread, SIGNAL(finished()), _sendImg, SLOT(clearImgQueue()));

    _recvThread = new RecvDeal();
    connect(_recvThread, SIGNAL(sigDataFromRecvQueue(MESG*)), this, SLOT(dealDataFromRecvQueue(MESG*)), Qt::BlockingQueuedConnection);
    _recvThread->start();

    _camera->setViewfinder(_myvideosurface);
    _camera->setCaptureMode(QCamera::CaptureStillImage);

    _ainput = new AudioInput();
    _ainputThread = new QThread();
    _ainput->moveToThread(_ainputThread);

    _aoutput = new AudioOutput();
    _ainputThread->start();
    _aoutput->start();

    connect(this, SIGNAL(startMicrophone()), _ainput, SLOT(startCollect()));
    connect(this, SIGNAL(stopMicrophone()), _ainput, SLOT(stopCollect()));
    connect(_ainput, SIGNAL(audioinputerror(QString)), this, SLOT(audioError(QString)));
    connect(_aoutput, SIGNAL(audiooutputerror(QString)), this, SLOT(audioError(QString)));
    connect(_aoutput, SIGNAL(speaker(QString)), this, SLOT(speaks(QString)));

    ui->scrollArea->verticalScrollBar()->setStyleSheet("QScrollBar:vertical { width:8px; background:rgba(0,0,0,0%); margin:0px,0px,0px,0px; padding-top:9px; padding-bottom:9px; }QScrollBar::handle:vertical { width:8px; background:rgba(0,0,0,25%); border-radius:4px; min-height:20; }QScrollBar::handle:vertical:hover { width:8px; background:rgba(0,0,0,50%); border-radius:4px; min-height:20; } QScrollBar::add-line:vertical { height:9px;width:8px;subcontrol-position:bottom; } QScrollBar::sub-line:vertical { height:9px;width:8px;subcontrol-position:top; } QScrollBar::add-line:vertical:hover { height:9px;width:8px;  subcontrol-position:bottom; } QScrollBar::sub-line:vertical:hover { height:9px;width:8px; subcontrol-position:top; } QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:rgba(0,0,0,10%); border-radius:4px; }");
    ui->listWidget->setStyleSheet("QScrollBar:vertical { width:8px; background:rgba(0,0,0,0%); margin:0px,0px,0px,0px; padding-top:9px; padding-bottom:9px; } QScrollBar::handle:vertical { width:8px; background:rgba(0,0,0,25%); border-radius:4px; min-height:20; } QScrollBar::handle:vertical:hover { width:8px; background:rgba(0,0,0,50%); border-radius:4px; min-height:20; } QScrollBar::add-line:vertical { height:9px;width:8px;subcontrol-position:bottom; } QScrollBar::sub-line:vertical { height:9px;width:8px; subcontrol-position:top; } QScrollBar::add-line:vertical:hover { height:9px;width:8px;  subcontrol-position:bottom; } QScrollBar::sub-line:vertical:hover { height:9px;width:8px; subcontrol-position:top; } QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:rgba(0,0,0,10%); border-radius:4px; }");

    QFont te_font = this->font();
    te_font.setFamily("MicrosoftYaHei");
    te_font.setPointSize(12);
    ui->listWidget->setFont(te_font);
}

Widget::~Widget()
{
    if (_mytcpSocket->isRunning())
    {
        _mytcpSocket->stopImmediately();
        _mytcpSocket->wait();
    }
    delete _mytcpSocket;

    if (_recvThread->isRunning())
    {
        _recvThread->stopImmediately();
        _recvThread->wait();
    }
    delete _recvThread;
    if (_imgThread->isRunning())
    {
        _imgThread->quit();
        _imgThread->wait();
    }

    if (_sendImg->isRunning())
    {
        _sendImg->stopImmediately();
        _sendImg->wait();
    }
    delete _sendImg;
    delete _imgThread;

    if (_textThread->isRunning())
    {
        _textThread->quit();
        _textThread->wait();
    }

    if (_sendText->isRunning())
    {
        _sendText->stopImmediately();
        _sendText->wait();
    }
    delete _sendText;
    delete _textThread;
    if (_ainputThread->isRunning())
    {
        _ainputThread->quit();
        _ainputThread->wait();
    }
    delete _ainputThread;
    delete _ainput;
    if (_aoutput->isRunning())
    {
        _aoutput->stopImmediately();
        _aoutput->wait();
    }
    delete _aoutput;
    delete ui;
}

void Widget::on_createmeetBtn_clicked()
{
    if (false == _createmeet){
        ui->createmeetBtn->setDisabled(true);
        ui->openAudio->setDisabled(true);
        ui->openVideo->setDisabled(true);
        ui->exitmeetBtn->setDisabled(true);
        ui->outlog->setText("正在创建会议室...");
        emit sigPushTextMsgToTextQueue(CREATE_MEETING);
    }
}

void Widget::on_exitmeetBtn_clicked()
{
    if (_camera->status() == QCamera::ActiveStatus)
    {
        _camera->stop();
    }

    ui->createmeetBtn->setDisabled(false);
    ui->exitmeetBtn->setDisabled(true);
    _createmeet = false;
    _joinmeet = false;

    clearPartner();
    _mytcpSocket->disconnectionFromHost();
    _mytcpSocket->wait();

    ui->outlog->setText(tr("Exited the meeting"));
    ui->groupBox->setTitle(QString("Mian Display"));

    while (ui->listWidget->count() > 0)
    {
        ui->listWidget->clear();
    }

    emit send_quit();
}

void Widget::on_openVideo_clicked(){
    if (!_createmeet && !_joinmeet) return;
    if(_camera->status() == QCamera::ActiveStatus){ //如果相机正在运行
        _camera->stop();    //停止相机
        if(_camera->error() == QCamera::NoError){   //关闭未发生错误
            _imgThread->quit(); //停止图像线程
            _imgThread->wait();
            ui->openVideo->setText("打开视频");
            emit sigPushTextMsgToTextQueue(CLOSE_CAMERA);
        }
        closeImg(m_ip);
    }
    else{
        _camera->start();   //启动相机线程
        if (_camera->error() == QCamera::NoError){  //启动成功
            _imgThread->start();
            ui->openVideo->setText("关闭视频");
        }
    }
}

void Widget::closeImg(quint32 ip)
{
    if (!partner.contains(ip))
    {
        qDebug() << "close img error";
        return;
    }
    Partner* p = partner[ip];
    p->setpic(QImage(":/resource/background.png"));

    if (m_ip == ip){
        ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/resource/background.png").scaled(ui->mainshow_label->size())));
    }
}

void Widget::paintEvent(QPaintEvent* event){
    Q_UNUSED(event);
}

void Widget::on_openAudio_clicked(){
    if (!_createmeet && !_joinmeet) return;
}

void Widget::on_openMicrophone_clicked(){
    if (!_createmeet && !_joinmeet) return;
    if (ui->openMicrophone->text().toUtf8() == QString(OPENMICROPHONE).toUtf8()){
    emit startMicrophone();
    ui->openMicrophone->setText(QString(CLOSEMICROPHONE).toUtf8());
    }
    else{
    emit stopMicrophone();
    ui->openMicrophone->setText(QString(OPENMICROPHONE).toUtf8());
    }
}

void Widget::on_joinmeetBtn_clicked()
{
    QString roomNo = ui->meetno->text();
    emit sigPushTextMsgToTextQueue(JOIN_MEETING, roomNo);
}

void Widget::on_sendmsg_clicked(){   //点击发送
    QString msg = ui->plainTextEdit->toPlainText().trimmed();   //获取发送内容
    if (msg.size() == 0){
        qDebug() << "empty";
        return;
    }
    ui->plainTextEdit->clear();//清空框里内容

    QString msgshow=QHostAddress(_mytcpSocket->getLocalIp()).toString() + ": " + msg;
    if (ui->listWidget->count() == 0) {
        QDateTime time = QDateTime::fromTime_t(QDateTime::currentDateTimeUtc().toTime_t());
        time.setTimeSpec(Qt::UTC);
        QString m_Time = time.toString("yyyy-MM-dd ddd hh:mm:ss");

        QLabel* label = new QLabel;
        label->setText(m_Time);
        label->setAlignment(Qt::AlignCenter);
        QListWidgetItem* item = new QListWidgetItem;
        ui->listWidget->addItem(item);
        ui->listWidget->setItemWidget(item, label);
        ui->listWidget->addItem(msgshow);
    }
    else
        ui->listWidget->addItem(msgshow);

    emit sigPushTextMsgToTextQueue(TEXT_SEND, msg);  //发送信号
}

void Widget::on_sendimg_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("打开图片"), ".", tr("Image Files(*.png *.jpg *jpeg *.bmp)"));
    qDebug()<<"fileName:"<<fileName;
    QTextCodec* code = QTextCodec::codecForName("GB2312");
    QString imagePath = code->fromUnicode(fileName);
    qDebug()<<"imagePath:"<<imagePath;
    QImage img;
    img.load(imagePath);
    _imgThread->start();    //启动线程
    if(!img.isNull())
        emit sigPushToImgQueue(img);
}

Partner* Widget::addPartner(quint32 ip)
{
    if(partner.contains(ip))
        return nullptr;
    Partner* p = new Partner(ui->scrollAreaWidgetContents, ip);
    if (p == nullptr) {
        qDebug() << "new Partner error";
        return nullptr;
    }
    else {
        connect(p,&Partner::sendip,this,&Widget::recvIp);
        partner.insert(ip, p);
        layout->addWidget(p, 1);
        if (partner.size() > 1) {
            connect(this, SIGNAL(volumnChange(int)), _ainput, SLOT(setVolumn(int)), Qt::UniqueConnection);
            connect(this, SIGNAL(volumnChange(int)), _aoutput, SLOT(setVolumn(int)), Qt::UniqueConnection);
            ui->openAudio->setDisabled(false);
            _aoutput->startPlay();
        }
        return p;
    }
}

void Widget::removePartner(quint32 ip)
{
    if (partner.contains(ip)) {
        Partner* p = partner[ip];
        disconnect(p, &Partner::sendip, this, &Widget::recvIp);
        layout->removeWidget(p);
        delete p;
        partner.remove(ip);

        if (partner.size() <= 1)
        {
            disconnect(_ainput, SLOT(setVolumn(int)));
            disconnect(_aoutput, SLOT(setVolumn(int)));
            _ainput->stopCollect();
            _aoutput->stopPlay();
            ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
            ui->openAudio->setDisabled(true);
        }
    }
}

void Widget::clearPartner()
{
    ui->mainshow_label->setPixmap(QPixmap());
    if (partner.size() == 0) return;

    QMap<quint32, Partner*>::iterator iter = partner.begin();
    while (iter != partner.end())
    {
        quint32 ip = iter.key();
        iter++;
        Partner* p = partner.take(ip);
        layout->removeWidget(p);
        delete p;
        p = nullptr;
    }

    disconnect(_ainput, SLOT(setVolumn(int)));
    disconnect(_aoutput, SLOT(setVolumn(int)));
    _ainput->stopCollect();
    _aoutput->stopPlay();
    ui->openAudio->setText(QString(CLOSEAUDIO).toUtf8());
    ui->openAudio->setDisabled(true);

    if (_imgThread->isRunning())
    {
        _imgThread->quit();
        _imgThread->wait();
    }
    ui->openVideo->setText(QString(OPENVIDEO).toUtf8());
    ui->openVideo->setDisabled(true);
}

/*
 * @brief 收到DataRecv信号，获取queue_recv中的msg并处理
 * @msg 从queue_recv中获取的msg
*/
void Widget::dealDataFromRecvQueue(MESG* msg){
    if (msg->msg_type == CREATE_MEETING_RESPONSE){
        int roomno;
        memcpy(&roomno, msg->data, msg->len);   //获取房间号
        if (roomno != 0){
            QMessageBox::information(this, "会议创建成功", QString("房间号：%1").arg(roomno), QMessageBox::Yes, QMessageBox::Yes);//提示

            ui->groupBox->setTitle(QString("主显示屏(房间号: %1)").arg(roomno));
            ui->outlog->setText(QString("创建成功! 房间号: %1").arg(roomno));
            _createmeet = true;
            ui->exitmeetBtn->setDisabled(false);
            ui->openVideo->setDisabled(false);
            ui->openAudio->setDisabled(false);
            ui->joinmeetBtn->setDisabled(true);

            addPartner(m_ip);
            ui->groupBox->setTitle(QHostAddress(_mytcpSocket->getLocalIp()).toString());    //获取本地IP
            ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/resource/background.png").scaled(ui->mainshow_label->size())));//设置背景图
        }
        else{
            _createmeet = false;
            QMessageBox::information(this, "房间信息", QString("没有可用的房间"), QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("没有可用的房间"));
            ui->createmeetBtn->setDisabled(false);
        }
    }
    else if (msg->msg_type == JOIN_MEETING_RESPONSE){
        qint32 c;
        memcpy(&c, msg->data, msg->len);
        if (c == 0){
            QMessageBox::warning(this, "错误", tr("房间号不存在"), QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("房间号不存在"));
            ui->exitmeetBtn->setDisabled(true);
            ui->openVideo->setDisabled(true);
            ui->joinmeetBtn->setDisabled(false);
            _joinmeet = false;
        }
        else{
            QMessageBox::information(this, "会议信息", "加入成功！", QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("加入成功！"));
            //add user self
            addPartner(m_ip);
            ui->groupBox->setTitle(QHostAddress(_mytcpSocket->getLocalIp()).toString());
            ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/resource/background.png").scaled(ui->mainshow_label->size())));  //设置图片背景
            ui->joinmeetBtn->setDisabled(true);
            ui->exitmeetBtn->setDisabled(false);
            ui->createmeetBtn->setDisabled(true);
            ui->openVideo->setDisabled(false);
            _joinmeet = true;
        }
    }
    else if (msg->msg_type == IMG_RECV){
        QHostAddress a(msg->ip);
        qDebug() << a.toString();
        QImage img;
        img.loadFromData(msg->data, msg->len);
        //接受时缩小处理
//        QPixmap pixmap = QPixmap::fromImage(img);
//        int targetWidth = 120;
//        int targetHeight = 120;
//        Qt::AspectRatioMode aspectRatioMode = Qt::KeepAspectRatio;
//        QPixmap scaledPixmap = pixmap.scaled(targetWidth, targetHeight, aspectRatioMode);
//        img = scaledPixmap.toImage();

        if (partner.count(msg->ip) == 1){
            Partner* p = partner[msg->ip];
            p->setpic(img);
        }
        else{
            Partner* p = addPartner(msg->ip);
            p->setpic(img);
        }

        if (msg->ip == m_ip)
        {
            ui->mainshow_label->setPixmap(QPixmap::fromImage(img).scaled(ui->mainshow_label->size()));
        }
        repaint();
    }
    else if (msg->msg_type == TEXT_RECV)
    {
        if(partner.count(msg->ip) == 0) {
            Partner* p = addPartner(msg->ip);
        }
        QString str = QString::fromStdString(std::string((char*)msg->data, msg->len));
        str = QHostAddress(msg->ip).toString()+": " + str;
        if (ui->listWidget->count() == 0) {
            QDateTime time = QDateTime::fromTime_t(QDateTime::currentDateTimeUtc().toTime_t());
            time.setTimeSpec(Qt::UTC);
            QString m_Time = time.toString("yyyy-MM-dd ddd hh:mm:ss");

            QLabel* label = new QLabel;
            label->setText(m_Time);
            label->setAlignment(Qt::AlignCenter);
            QListWidgetItem* item = new QListWidgetItem;
            ui->listWidget->addItem(item);
            ui->listWidget->setItemWidget(item, label);
            ui->listWidget->addItem(str);
        }
        else
            ui->listWidget->addItem(str);

    }
    else if (msg->msg_type == PARTNER_JOIN){ //本房间有用户加入
        Partner* p = addPartner(msg->ip);
        if (p){
            p->setpic(QImage(":/resource/background.png"));
            ui->outlog->setText(QString("%1 加入会议").arg(QHostAddress(msg->ip).toString()));
        }
        ui->openVideo->setDisabled(false);
    }
    else if(msg->msg_type == PARTNER_EXIT){    //本房间有用户退出
        removePartner(msg->ip);
        ui->outlog->setText(QString("%1 退出会议").arg(QHostAddress(msg->ip).toString()));
    }
    else if(msg->msg_type == CLOSE_CAMERA){
        closeImg(msg->ip);
    }
    else if (msg->msg_type == REMOTEHOSTCLOSE_ERROR){
        clearPartner();
        _mytcpSocket->disconnectionFromHost();
        _mytcpSocket->wait();
        ui->outlog->setText(QString("远程服务器关闭连接"));
        ui->createmeetBtn->setDisabled(true);
        ui->exitmeetBtn->setDisabled(true);
        ui->joinmeetBtn->setDisabled(true);
        //clear chat record
        while (ui->listWidget->count() > 0){
            ui->listWidget->clear();
        }

        if (_createmeet || _joinmeet) QMessageBox::information(this, "信息", "会议结束", QMessageBox::Yes, QMessageBox::Yes);
    }
    else if (msg->msg_type == OTHERNET_ERROR){
        QMessageBox::warning(NULL, "错误", "网络异常", QMessageBox::Yes, QMessageBox::Yes);
        clearPartner();
        _mytcpSocket->disconnectionFromHost();
        _mytcpSocket->wait();
        ui->outlog->setText(QString("网络异常......"));
    }
    if (msg->data){
        free(msg->data);
        msg->data = NULL;
    }
    if (msg){
        free(msg);
        msg = NULL;
    }
}

void Widget::textSend()
{
    qDebug() << "send text over";

    ui->sendmsg->setDisabled(false);
}

void Widget::cameraImageCapture(QVideoFrame frame){
    if (frame.isValid() && frame.isReadable())
    {
        QImage videoImg = QImage(frame.bits(), frame.width(), frame.height(), QVideoFrame::imageFormatFromPixelFormat(frame.pixelFormat()));

        QTransform matrix;
        matrix.rotate(180.0);

        QImage img = videoImg.transformed(matrix, Qt::FastTransformation).scaled(ui->mainshow_label->size());

        if (partner.size() > 1)
        {
            emit sigPushToImgQueue(img);
        }

        ui->mainshow_label->setPixmap(QPixmap::fromImage(img).scaled(ui->mainshow_label->size()));

        Partner* p = partner[m_ip];
        if (p) p->setpic(img);
    }
    frame.unmap();
}

void Widget::cameraError(QCamera::Error)
{
    QMessageBox::warning(this, "Camera error", _camera->errorString(), QMessageBox::Yes, QMessageBox::Yes);
}

void Widget::audioError(QString err)
{
    QMessageBox::warning(this, "Audio error", err, QMessageBox::Yes);
}

void Widget::speaks(QString ip){
    ui->outlog->setText(QString(ip + " is speaking").toUtf8());
}

void Widget::recvIp(quint32 ip)
{
    if (partner.contains(m_ip))
    {
        Partner* p = partner[m_ip];
        p->setStyleSheet("border-width: 1px; border-style: solid; border-color:rgba(0, 0 , 255, 0.7)");
    }
    if (partner.contains(ip))
    {
        Partner* p = partner[ip];
        p->setStyleSheet("border-width: 1px; border-style: solid; border-color:rgba(255, 0 , 0, 0.7)");
    }
    ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/resource/background.png").scaled(ui->mainshow_label->size())));
    ui->groupBox_2->setTitle(QHostAddress(ip).toString());
    qDebug() << ip;
}

void Widget::recvOKfromLogin(QString ip, QString port){
    if(_mytcpSocket->connectToServer(ip, port, QIODevice::ReadWrite)){
        ui->outlog->setText("成功连接服务器: " + ip + ":" + port);
        ui->openAudio->setDisabled(true);
        ui->openVideo->setDisabled(true);
        ui->createmeetBtn->setDisabled(false);
        ui->exitmeetBtn->setDisabled(true);
        ui->joinmeetBtn->setDisabled(false);
        ui->sendmsg->setDisabled(false);
        ui->sendimg->setDisabled(false);
        m_ip = QHostAddress("0.0.0.0").toIPv4Address();
        emit send_L(true);
    }
    else
        emit send_L(false);
}


