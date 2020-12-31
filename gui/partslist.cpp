#include <QMimeData>
#include <QUrl>
#include "partslist.h"
#include "mainwindow.h"

// Invoked by QTextEdit's drop event handler
void PartsList::insertFromMimeData(const QMimeData *source)
{
    // If the source has any URLs, see if they refer to local
    // files. If so, convert them to local filesystem paths and add
    // them to the parts list.
    if (source->hasUrls()) {
        QStringList stls;
        for (auto url : source->urls()) {
            if (url.isLocalFile()) {
                stls.push_back(url.toLocalFile());
            }
        }
        if (stls.size() > 0) {
            MainWindow::instance()->addParts(stls);
        }
    } else {
        // default handling
        QTextEdit::insertFromMimeData(source);
    }
}
