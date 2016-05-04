#include "bosswave.h"

BW::BW(QObject *parent):
    QObject(parent)
{

}

BW::~BW()
{
}

QObject *BW::qmlSingleton(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    auto rv = BW::instance();
    rv->engine = engine;
    rv->jsengine = scriptEngine;
    return rv;
}
BW *BW::instance()
{
    static BW *bw = new BW();
    return bw;
}
