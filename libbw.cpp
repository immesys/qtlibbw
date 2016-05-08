#include "libbw.h"
#include "bosswave.h"
#include <QQmlExtensionPlugin>
#include <qqml.h>

ImportBosswave::ImportBosswave()
{
    qmlRegisterSingletonType<BW>("BOSSWAVE", 1, 0, "BW", &BW::qmlSingleton);
}


static ImportBosswave bydefault;
