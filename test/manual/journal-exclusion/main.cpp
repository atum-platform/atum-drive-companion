// SPDX-License-Identifier: CC0-1.0
#include "common/syncjournaldb.h"
#include <QCoreApplication>
#include <QDir>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 3) {
        return 2;
    }
    OCC::SyncJournalDb journal(QDir(QString::fromLocal8Bit(argv[2])).filePath(".sync_journal.db"));
    const bool opened = journal.open();
    std::cout << "journal_open=" << opened << std::endl;
    if (opened && QString::fromLocal8Bit(argv[1]) == "hold") {
        std::cin.get();
    }
    journal.close();
    return opened ? 0 : 10;
}
