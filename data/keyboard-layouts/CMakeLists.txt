# SPDX-FileCopyrightText: 2007 Robert Knight <robertknight@gmail.com>
#
# SPDX-License-Identifier: BSD-2-Clause

install( FILES
         default.keytab
         linux.keytab
         macos.keytab
         solaris.keytab
         DESTINATION
         ${KDE_INSTALL_DATADIR}/konsole )
