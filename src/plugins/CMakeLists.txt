# SPDX-FileCopyrightText: 2022 Tomaz Canabrava <tcanabrava@kde.org>
#
# SPDX-License-Identifier: BSD-2-Clause

if(ENABLE_PLUGIN_SSHMANAGER)
    add_subdirectory(SSHManager)
endif()
if(ENABLE_PLUGIN_QUICKCOMMANDS)
    add_subdirectory(QuickCommands)
endif()
