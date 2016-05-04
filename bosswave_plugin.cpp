#include "bosswave_plugin.h"
#include "bosswave.h"

#include <qqml.h>

void BOSSWAVEPlugin::registerTypes(const char *uri)
{
    // @uri io.bw2
    qmlRegisterSingletonType<BW>(uri, 1, 0, "BW", &BW::qmlSingleton);
}

