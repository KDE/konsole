// Own
#include "MemorySettings.h"

// Qt
#include <QCheckBox>
#include <QLabel>

using namespace Konsole;

MemorySettings::MemorySettings(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

MemorySettings::~MemorySettings() = default;

#include "moc_MemorySettings.cpp"
