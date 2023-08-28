#ifndef MEMORYSETTINGS_H
#define MEMORYSETTINGS_H

#include "ui_MemorySettings.h"

namespace Konsole
{
class MemorySettings : public QWidget, private Ui::MemorySettings
{
    Q_OBJECT

public:
    explicit MemorySettings(QWidget *parent = nullptr);
    ~MemorySettings() override;
};
}

#endif
