#ifndef MESSAGE_H
#define MESSAGE_H

#include <QSharedPointer>
#include "agentconnection.h"

QT_FORWARD_DECLARE_CLASS(Message);

typedef QSharedPointer<Message> PMessage;
class Message
{
public:
    Message();
    PMessage fromFrame(PFrame f);
    QList<PayloadObject*> POs();
    QList<PayloadObject*> FilterPOs(int ponum);

private:
    PFrame frame;
};

#endif // MESSAGE_H
