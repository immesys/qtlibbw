#include "message.h"
#include "agentconnection.h"

Message::Message()
{

}

PMessage Message::fromFrame(PFrame f)
{
    Message *rv = new Message();
    rv->frame = f;
    return QSharedPointer<Message>(rv);
}

QList<PayloadObject*> Message::POs()
{
    return frame->pos;
}

QList<PayloadObject*> Message::FilterPOs(int ponum)
{
    auto rv = QList<PayloadObject*>();
    foreach(auto po, frame->pos)
    {
        if (po->ponum() == ponum)
        {
            rv.append(po);
        }
    }
    return rv;
}
