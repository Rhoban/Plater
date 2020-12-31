#ifndef PARTSLIST_H
#define PARTSLIST_H

#include <QTextEdit>

class PartsList : public QTextEdit
{
    Q_OBJECT
public:
    PartsList(QWidget *parent = nullptr) : QTextEdit(parent) {}
protected:
    // Invoked by QTextEdit's drop event handler
    virtual void insertFromMimeData(const QMimeData *source);
};

#endif // PARTSLIST_H
