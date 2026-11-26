#pragma once
#include <QString>
#include <atomic>
#include <functional>

enum class ExportPreset {
    ProResProxy, ProResLT, ProResStandard, ProResHQ, ProRes4444, ProRes4444XQ,
    DnxLB, DnxSQ, DnxHQ, DnxHQX,
    H264Mkv, H264Mp4, Wav
};

QString exportSuffix(ExportPreset preset);
void transcodeMedia(const QString& source, const QString& destination, int maxHeight, ExportPreset preset,
                     const std::atomic_bool& cancelled,
                     const std::function<void(qint64, qint64, const QString&)>& progress);
