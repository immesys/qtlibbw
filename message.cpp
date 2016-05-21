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
