#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QThread>
#include <QTimer>
#include <QMainWindow>
#include "wizard.h"
#include "worker.h"
#include "platesviewer.h"
#include "about.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    // Get the singleton instance of MainWindow
    static MainWindow *instance();

    void updatePlateEnable();
    void enableAll(bool enable);
    void showError(std::string msg);
    void showSuccess(std::string msg);
    void showMessage(std::string msg);
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    // MainWindow::wizardNext() is called indirectly via QMetaObject::invokeMethod().
    // Versions of QT before 5.10 require the method to be specified as a string
    // and registered as invokable.
    Q_INVOKABLE
#endif
    void wizardNext();
    // Add all parts from the list by invoking wizardNext() for each file.
    void addParts(QStringList stls);
    bool isCircular();
    float getPlateDiameter();
    float getPlateWidth();
    float getPlateHeight();

public slots:
    void on_worker_end();
    void on_about();
    void on_wizard_accept();
    
private slots:
    void on_outputDirectoryButton_clicked();
    void on_runButton_clicked();
    void on_partBrowse_clicked();
    void on_saveButton_clicked();
    void on_openButton_clicked();
    void on_clearButton_clicked();
    void timeOutSlot();
    void on_circularPlate_clicked();
    void on_actionQuit_triggered();
    void on_actionAdd_parts_triggered();

    void on_actionOpen_plater_conf_triggered();

    void on_actionSave_triggered();

    void on_singleSort_clicked();

    void on_multipleSort_clicked();

    void on_actionSave_configuration_triggered();

    void on_actionLoad_configuration_triggered();

private:
    Ui::MainWindow *ui;
    QThread thread;
    bool enabled;
    Worker worker;
    Wizard *wizard;
    About *about;
    PlatesViewer *platesViewer;
    QStringList stls;
    QString workingDirectory;
    QTimer *timer;
};

#endif // MAINWINDOW_H
