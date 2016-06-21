#include "libbw.h"
#include "bosswave.h"
#include <QQmlExtensionPlugin>
#include <qqml.h>

void initLibBW()
{
    qmlRegisterSingletonType<BW>("BOSSWAVE", 1, 0, "BW", &BW::qmlSingleton);
}
