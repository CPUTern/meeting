#include "SendText.h"
#include <qdebug.h>
#include <zlib.h>

extern QUEUE_DATA<MESG> queue_send;

SendText::SendText(QObject *parent)
	: QThread(parent)
{}

SendText::~SendText()
{}

void SendText::run(){    //循环将text消息从text队列中发送到send队列，直到text消息队列为空则等待
	isRun = true;
    while(1) {
		queue_lock.lock();
        while (textqueue.size() == 0) {     //textqueue为空
			bool f = queue_cond.wait(&queue_lock, WAITSECONDS * 1000);
			if (f == false) {
				QMutexLocker locker(&m_lock);
				if (isRun == false) {
					queue_lock.unlock();
					return;
				}
			}
		}

        textMsg text = textqueue.front();// 获取队首
		textqueue.pop_front();
		queue_lock.unlock();
        queue_cond.wakeOne();

        //构造发送消息
        MESG* send = (MESG*)malloc(sizeof(MESG));
        if (send == nullptr){
			qDebug() << __FILE__ << __LINE__ << "malloc fail";
			continue;
		}
        else{
			memset(send, 0, sizeof(MESG));
            if (text.type == CREATE_MEETING || text.type == CLOSE_CAMERA){
				send->len = 0;
				send->data = nullptr;
				send->msg_type = text.type;
				queue_send.push_msg(send);
			}
            else if (text.type == JOIN_MEETING){
				send->msg_type = JOIN_MEETING;
				send->len = 4; 
				send->data = (uchar*)malloc(send->len + 10);
                if (send->data == nullptr){
					qDebug() << __FILE__ << __LINE__ << "malloc fail";
					free(send);
					continue;
				}
                else{
					memset(send->data, 0, send->len + 10);
					quint32 roomno = text.str.toUInt();
					memcpy(send->data, &roomno, sizeof(roomno));
					queue_send.push_msg(send);
				}
			}
            else if (text.type == TEXT_SEND){
				send->msg_type = TEXT_SEND;

                QByteArray byteArray = text.str.toUtf8();   //转为ByteArray
                const char* sourceData = reinterpret_cast<const char*>(byteArray.data());   //转为const char*
                uLong sourceLen = strlen(sourceData);

//                qDebug()<<"sourceData:";
//                for(int i = 0;i<sourceLen;i++){
//                    qDebug()<<QString::number(sourceData[i], 16)<<" size:"<<sizeof(send->data[i]);
//                }

                send->len = (long)sourceLen;
                if((send->data =(uchar*)malloc(sourceLen))== nullptr){
                    qDebug()<<"No Enough Memory";
                    free(send->data);
                    continue;
                }

                memcpy(send->data, sourceData, sourceLen);
                queue_send.push_msg(send);
			}
		}
	}
}

void SendText::stopImmediately()
{
	QMutexLocker locker(&m_lock);
	isRun = false;
}

void SendText::pushTextMsgToTextQueue(MSG_TYPE msgType, QString str) {   //将发送数据，添加到text发送消息队列
	queue_lock.lock();
	while (textqueue.size() > QUEUE_SIZE) {
		queue_cond.wait(&queue_lock);
	}
    textqueue.push_back(textMsg(str, msgType));
	queue_lock.unlock();
	queue_cond.wakeOne();
}
