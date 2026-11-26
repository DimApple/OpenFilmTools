#include "project.hpp"
#include "proxy_widget.hpp"

#include <QApplication>
#include <QFile>
#include <QListWidget>
#include <QTemporaryDir>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir tmp;
    if (!tmp.isValid()) return 1;
    const auto one = tmp.filePath("One.openroll");
    const auto two = tmp.filePath("Two.openroll");
    Clip online, offline;
    online.path = tmp.filePath("Available.mov");
    offline.path = tmp.filePath("Missing.mov");
    QFile fixture(online.path);
    if (!fixture.open(QIODevice::WriteOnly)) return 1;
    fixture.close();
    Project first(one, true);
    first.saveScan(tmp.path(), {online, offline});
    Project second(two, true);

    auto active = one;
    ProxyWidget widget([&] { return active; });
    widget.show();
    auto* list = widget.findChild<QListWidget*>();
    if (!list || list->count() != 2 ||
        list->item(0)->checkState() != Qt::Unchecked ||
        list->item(1)->flags().testFlag(Qt::ItemIsEnabled)) return 1;
    list->item(0)->setCheckState(Qt::Checked);
    widget.hide();
    widget.show();
    if (list->item(0)->checkState() != Qt::Checked) return 1;
    widget.hide();
    active = two;
    widget.show();
    return list->count() == 0 ? 0 : 1;
}
