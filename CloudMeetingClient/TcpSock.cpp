#include "TcpSock.h"
#include <qhostaddress.h>
#include <qmetaobject.h>
#include <QtEndian>
#include <zlib.h>

extern QUEUE_DATA<MESG> queue_send;
extern QUEUE_DATA<MESG> queue_recv;
extern QUEUE_DATA<MESG> audio_recv;

TcpSock::TcpSock(QObject *parent): QThread(parent)
{
	qRegisterMetaType<QAbstractSocket::SocketError>();
	socktcp = nullptr;
	sockThread = new QThread();
	moveToThread(sockThread);
	connect(sockThread, &QThread::finished, this, &TcpSock::closeSocket);
	sendbuf = (uchar*)malloc(4 * MB);
	recvbuf= (uchar*)malloc(4 * MB);
	hasReceive = 0;
}

TcpSock::~TcpSock()
{
	delete recvbuf;
	delete sendbuf;
	delete socktcp;
	delete sockThread;
}

bool TcpSock::connectToServer(QString ip, QString port, QIODevice::OpenModeFlag flag)
{
	sockThread->start();
	bool retVal;
	QMetaObject::invokeMethod(this, "connectServer", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, retVal),
		Q_ARG(QString, ip), Q_ARG(QString, port), Q_ARG(QIODevice::OpenModeFlag, flag));
	if (retVal) {
		start();
		return true;
	}
	else
		return false;
}

QString TcpSock::errorString()
{
	return socktcp->errorString();
}

//断开与主机的连接，并清空接收队列和发送队列
void TcpSock::disconnectionFromHost(){
    // 如果当前线程正在运行，则先加锁，然后设置 isRun 为 false
	if (isRunning()) {
		QMutexLocker locker(&m_lock);
		isRun = false;
	}

    // 如果 sockThread 线程正在运行，则先退出线程，再等待线程结束
	if (sockThread->isRunning()) {
		sockThread->quit();
		sockThread->wait();
	}

    // 清空接收队列、发送队列和音频接收缓冲区
	queue_recv.clear();
	queue_send.clear();
	audio_recv.clear();
}

//获取本地ip
quint32 TcpSock::getLocalIp()
{
    if (socktcp->isOpen())
		return socktcp->localAddress().toIPv4Address();
	else
		return -1;
}

//循环发送queue_send里的信息
void TcpSock::run(){
    isRun = true;
    while(1){
		{
			QMutexLocker locker(&m_lock);
			if (isRun == false)return;
		}
        MESG* send = queue_send.pop_msg();
//        qDebug()<<"send->type:"<<send->msg_type;
        if(send == nullptr)continue;
        QMetaObject::invokeMethod(this, "sendData", Q_ARG(MESG*, send));    //通过方法名间接调用 Qt 对象的方法的函数, 相当于this->sendData(send)
	}
}

//根据send, 构造消息, 向服务端发送信息
void TcpSock::sendData(MESG* send)
{
    if (socktcp->state() == QAbstractSocket::UnconnectedState) {    //未连接状态
		emit sendTextOver();
		if (send->data)free(send->data);
		if (send)free(send);
		return;
	}
    //构造发送消息
    //1 $ 1Byte
	quint64 bytestowrite = 0;
	sendbuf[bytestowrite++] = '$';

    //2 type 2Byte
    qToBigEndian<quint16>(send->msg_type, sendbuf + bytestowrite);  //转换为网络字节序
	bytestowrite += 2;

    //3 ip 4Byte
	quint32 ip = socktcp->localAddress().toIPv4Address();
	qToBigEndian<quint32>(ip, sendbuf + bytestowrite);
	bytestowrite += 4;

    //4 len 4Byte
	if (send->msg_type == CREATE_MEETING || send->msg_type == AUDIO_SEND ||
		send->msg_type == CLOSE_CAMERA || send->msg_type == IMG_SEND ||
		send->msg_type == TEXT_SEND|| send->msg_type==LOGIN|| send->msg_type==REGISTER) {
        qToBigEndian<quint32>(send->len, sendbuf + bytestowrite);
		bytestowrite += 4;
	}
	else if (send->msg_type == JOIN_MEETING) {
		qToBigEndian<quint32>(send->len, sendbuf + bytestowrite);
		bytestowrite += 4;

		uint32_t room;
		memcpy(&room, send->data, send->len);
		qToBigEndian<quint32>(room, send->data);
	}

    //data send->lenByte
	memcpy(sendbuf + bytestowrite, send->data, send->len);
	bytestowrite += send->len;

    //# 1Byte
	sendbuf[bytestowrite++] = '#';

//    qDebug()<<"sendbuf:";
//    for(int i = 0;i<MSG_HEADER+send->len+1;i++){
//        qDebug()<<QString::number(sendbuf[i], 16)<<" size:"<<sizeof(sendbuf[i]);
//    }

    //开始发送
	qint64 hastowrite = bytestowrite;
	qint64 ret = 0, haswrite = 0;
	while ((ret = socktcp->write((char*)sendbuf + haswrite, hastowrite - haswrite)) < hastowrite) {
		if (ret == -1 && socktcp->error() == QAbstractSocket::TemporaryError)
			ret = 0;
		else if (ret == -1) {
			qDebug() << "network error";
			break;
		}
		haswrite += ret;
		hastowrite -= ret;
	}
    //等待发送完毕
	socktcp->waitForBytesWritten();
    //发送完毕回收资源
	if (send->msg_type == TEXT_SEND)
		emit sendTextOver();
	if (send->data) {
		free(send->data);
	}
	if (send)
		free(send);
}

void TcpSock::closeSocket()
{
	if (socktcp && socktcp->isOpen()) {
		socktcp->close();
	}
}

void TcpSock::stopImmediately()
{
	{
		QMutexLocker lock(&m_lock);
		if (isRun == true)isRun = false;
	}
	sockThread->quit();
	sockThread->wait();
}

void TcpSock::errorDetect(QAbstractSocket::SocketError error)
{
	qDebug() << "Sock error" << QThread::currentThreadId();
	MESG* msg = (MESG*)malloc(sizeof(MESG));
	if (msg == nullptr) {
		qDebug() << "errdect malloc error";
	}
	else {
		memset(msg, 0, sizeof(MESG));
		if (error == QAbstractSocket::RemoteHostClosedError) {
			msg->msg_type = REMOTEHOSTCLOSE_ERROR;
		}
		else {
			msg->msg_type = OTHERNET_ERROR;
		}
		queue_recv.push_msg(msg);
	}
}

void TcpSock::recvFromSocket() {    //1 接受服务端的信息并解析处理

    qint64 availbytes = socktcp->bytesAvailable();  // 获取当前可读取的字节数

    if (availbytes <= 0)    // 如果没有可读取的数据，则直接返回
		return;

    qint64 ret = socktcp->read((char*)recvbuf + hasReceive, availbytes);     // 从socket中读取数据到接收缓冲区 availbytes->recvbuf
    if (ret <= 0) {
		qDebug() << "error or no more data";
		return;
    }
    hasReceive+=ret;    // 更新已接收数据的总字节数

    if (hasReceive < MSG_HEADER)    // 如果已接收数据的字节数小于报文头部长度 说明数据有问题, 返回
		return;
    else{
        quint32 datasize;
        qFromBigEndian<quint32>(recvbuf + 7, 4, &datasize);     // 从接收缓冲区中读取数据大小（数据大小在报文头部的特定位置）
        if ((quint64)datasize + 1 + MSG_HEADER <= hasReceive){  // 如果接收到的数据已经包含一个完整的数据包
            if (recvbuf[0] == '$' && recvbuf[MSG_HEADER + datasize] == '#'){     // 检查数据包的起始标记和结束标记是否正确
                MSG_TYPE msgtype;   // 读取消息类型
				uint16_t type;
                qFromBigEndian<uint16_t>(recvbuf + 1, 2, &type);    //该句使datasize出现问题
                msgtype = (MSG_TYPE)(type);

                qFromBigEndian<quint32>(recvbuf + 7, 4, &datasize); // 修复datasize

                if (msgtype == CREATE_MEETING_RESPONSE || msgtype == JOIN_MEETING_RESPONSE){     // 如果消息类型是 创建会议响应 或 加入会议响应
                    if (msgtype == CREATE_MEETING_RESPONSE){
                        MESG* msg = (MESG*)malloc(sizeof(MESG));    //构造msg
                        if(msg == nullptr){
                            qDebug() << __LINE__ << "CREATE_MEETING_RESPONSE malloc MESG failed";
                        }
                        else{
                            memset(msg, 0, sizeof(MESG));
                            //读取type
                            msg->msg_type = msgtype;
//                            qDebug()<<"type"<<msg->msg_type;
                            //读取datasize
                            qFromBigEndian<quint32>(recvbuf + 7, 4, &datasize);
                            msg->len = datasize;
//                            qDebug()<<"datasize"<<datasize;

                            msg->data = (uchar*)malloc((quint64)datasize);
                            if(msg->data == nullptr){     // 分配内存失败，输出错误信息
                                free(msg->data);
                                qDebug() << __LINE__ << "CREATE_MEETING_RESPONSE malloc MESG.data failed";
                            }
                            else{
                                //读取data
                                qint32 roomNo;
                                memset(msg->data, 0, (quint64)datasize);
                                memcpy(&roomNo, recvbuf + MSG_HEADER, datasize);
                                memcpy(msg->data, &roomNo, datasize);   // 将房间号拷贝到消息体中
//                                qDebug()<<"data:"<<*(msg->data);
                                queue_recv.push_msg(msg);   //将消息加入接收队列
                            }
                        }
					}
					else if (msgtype == JOIN_MEETING_RESPONSE) {
						qint32 c;
                        memcpy(&c, recvbuf + MSG_HEADER, datasize); // 从接收缓冲区中读取某个值
						MESG* msg = (MESG*)malloc(sizeof(MESG));
						if (msg == nullptr) {
							qDebug() << __LINE__ << "JOIN_MEETING_RESPONSE malloc MESG failed";
						}
						else {
							memset(msg, 0, sizeof(MESG));
							msg->msg_type = msgtype;
							msg->data = (uchar*)malloc(datasize);
							if (msg->data == nullptr) {
								free(msg);
								qDebug() << __LINE__ << "JOIN_MEETING_RESPONSE malloc MESG.data failed";

							}
							else {
								memset(msg->data, 0, datasize);
                                memcpy(msg->data, &c, datasize);    // 将某个值拷贝到消息体中
								msg->len = datasize;
								queue_recv.push_msg(msg);
							}
						}
					}
				}
				else if (msgtype == IMG_RECV ||
					msgtype == PARTNER_JOIN ||
					msgtype == PARTNER_EXIT ||
					msgtype == AUDIO_RECV ||
					msgtype == CLOSE_CAMERA ||
                    msgtype == TEXT_RECV) {
                    quint32 ip; // 从接收缓冲区中读取IP地址
                    qFromBigEndian<quint32>(recvbuf + 3, 4, &ip);
                    if (msgtype == IMG_RECV){    // 如果消息类型是图片接收
						uchar* dst = (uchar*)malloc(4 * MB);
						if (dst == nullptr) {
							free(dst);
							qDebug() << __LINE__ << "malloc failed";
						}
						memset(dst, 0, 4 * MB);
						ulong dstlen = 4 * MB;
						QByteArray cc((char*)recvbuf + MSG_HEADER, datasize);
						QByteArray rc = QByteArray::fromBase64(cc);
                        int flag = uncompress(dst, &dstlen, (uchar*)rc.data(), rc.size());  // 解压缩数据
                        if(flag == 0){
							MESG* msg = (MESG*)malloc(sizeof(MESG));
                            if(msg == nullptr){
								qDebug() << __LINE__ << "malloc failed";
							}
                            else{
								memset(msg, 0, sizeof(MESG));
								msg->msg_type = msgtype;
								msg->data = dst;

								if (msg->data == nullptr) {
									free(msg->data);
									qDebug() << __LINE__ << "malloc failed";
								}
								else {
									msg->len = dstlen;
									msg->ip = ip;
									queue_recv.push_msg(msg);
								}
							}
						}
						else {
							qDebug() << __FILE__ << __LINE__ << "uncompress error:" << flag;
						}
					}
                    else if (msgtype == PARTNER_JOIN || msgtype == PARTNER_EXIT || msgtype == CLOSE_CAMERA){     // 如果消息类型是合作伙伴加入、合作伙伴退出或关闭摄像头
						MESG* msg = (MESG*)malloc(sizeof(MESG));
                        if(msg == nullptr){
							qDebug() << __LINE__ << "malloc failed";

						}
                        else{
							memset(msg, 0, sizeof(MESG));
							msg->msg_type = msgtype;
							msg->ip = ip;
							queue_recv.push_msg(msg);
						}
					}
                    else if(msgtype == AUDIO_RECV){      // 如果消息类型是音频接收
						uchar* dst = (uchar*)malloc(4 * MB);
						if (dst == nullptr) {
							free(dst);
							qDebug() << __LINE__ << "malloc failed";
						}
						memset(dst, 0, 4 * MB);
						ulong dstlen = 4 * MB;
                        QByteArray cc((char*)recvbuf + MSG_HEADER, datasize);

//                        uchar* recv = (uchar*)cc.data();
//                        qDebug()<<"recvAudioData:";
//                        for(int i = 0;i<10;i++){
//                            qDebug()<<QString::number(recv[i], 16)<<" size:"<<sizeof(recv[i]);
//                        }

						int flag = uncompress(dst, &dstlen, (uchar*)QByteArray::fromBase64(cc).data(), datasize);
                        if(flag == 0){
							MESG* msg = (MESG*)malloc(sizeof(MESG));
                            if(msg == nullptr){
								qDebug() << __LINE__ << "malloc failed";
							}
                            else{
								memset(msg, 0, sizeof(MESG));
								msg->msg_type = msgtype;
								msg->ip = ip;
                                msg->data = dst;
								msg->len = dstlen;
                                audio_recv.push_msg(msg);
							}
						}
						else {
							qDebug() << __FILE__ << __LINE__ << "uncompress error:" << flag;
						}
					}
                    else if (msgtype == TEXT_RECV){
//						uchar* dst = (uchar*)malloc(4 * MB);
                        uchar* dst = (uchar*)malloc(datasize);
						if (dst == nullptr) {
							free(dst);
							qDebug() << __LINE__ << "malloc failed";
						}
//						memset(dst, 0, 4 * MB);
//						ulong dstlen = 4 * MB;
                        memset(dst, 0, datasize);
                        ulong dstlen = datasize;
                        memcpy(dst, recvbuf+MSG_HEADER, datasize);
                        MESG* msg = (MESG*)malloc(sizeof(MESG));
                        if (msg == nullptr) {
                            qDebug() << __LINE__ << "malloc failed";
                        }
                        else {
                            memset(msg, 0, sizeof(MESG));
                            msg->msg_type = msgtype;
                            msg->ip = ip;
                            msg->data = dst;
                            qDebug()<<"recv data:"<<msg->data;
                            msg->len = dstlen;
                            queue_recv.push_msg(msg);
                        }
					}
				}
                else if(msgtype == LOGIN_RESPONSE || msgtype == REGISTER_RESPONSE){
					MESG* msg = (MESG*)malloc(sizeof(MESG));
					if (msg == nullptr) {
						qDebug() << __LINE__ << "malloc failed";

					}
					else {
						memset(msg, 0, sizeof(MESG));
						if (datasize > 0) {
							msg->data=(uchar*)malloc(datasize);
							if (msg->data == nullptr) {
								free(msg);
								qDebug() << __LINE__ << "LOGIN_RESPONSE malloc MESG.data failed";
							}
							else {
								memset(msg->data, 0, datasize);
								memcpy(msg->data, recvbuf + MSG_HEADER, datasize);
                                msg->msg_type = msgtype;
								msg->len = datasize;
								emit send_L(msg);
							}
						}
					}
				}
				else {
					qDebug() << "msgtype error";
				}
			}
			else
				qDebug() << "package error";
			memmove_s(recvbuf, 4 * MB, recvbuf + MSG_HEADER + datasize + 1, hasReceive - ((quint64)datasize + 1 + MSG_HEADER));
			hasReceive -= ((quint64)datasize + 1 + MSG_HEADER);
		}
		else
			return;
	}
}

bool TcpSock::connectServer(QString ip, QString port, QIODevice::OpenModeFlag flag) {
	if (socktcp == nullptr)socktcp = new QTcpSocket();
	socktcp->connectToHost(ip, port.toUShort(), flag);
	connect(socktcp, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(errorDetect(QAbstractSocket::SocketError)),Qt::UniqueConnection);
	connect(socktcp, &QTcpSocket::readyRead, this, &TcpSock::recvFromSocket, Qt::UniqueConnection);
	if (socktcp->waitForConnected(5000)) {
		return true;
	}
	socktcp->close();
	return false;
}
