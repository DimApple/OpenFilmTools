#pragma once

#include <QDir>
#include <QFile>
#include <cerrno>
#include <QString>
#include <filesystem>
#include <system_error>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace nativeFiles {
inline void publish(const QString& from, const QString& to)
{
#ifdef Q_OS_WIN
    const auto source = QDir::toNativeSeparators(from).toStdWString();
    const auto destination = QDir::toNativeSeparators(to).toStdWString();
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
        throw std::system_error(int(GetLastError()), std::system_category(), "Could not publish without replacing an existing path");
#else
    if (::syscall(SYS_renameat2, AT_FDCWD, QFile::encodeName(from).constData(),
                  AT_FDCWD, QFile::encodeName(to).constData(), RENAME_NOREPLACE) != 0)
        throw std::system_error(errno, std::generic_category(), "Could not publish without replacing an existing path");
#endif
}
}
