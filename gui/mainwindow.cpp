#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <log.h>
#include <util.h>
#include <viewer.h>
#include <wizard.h>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#if defined(_WIN32) || defined(_WIN64)
    #include <direct.h>
    #define chdir _chdir
#else
#include <unistd.h>
#endif

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    enabled(true),
    worker(),
    wizard(NULL),
    platesViewer(NULL)
{
    workingDirectory = "";
    ui->setupUi(this);
    ui->message->hide();
    ui->progressBar->hide();
    setWindowTitle("Plater");

    about = new About;

    connect(&worker, SIGNAL(workerEnd()), this, SLOT(on_worker_end()));
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(on_about()));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timeOutSlot()));
    timer->start(250);
    updatePlateEnable();

    ui->nbThreads->setText(QString("%1").arg(QThread::idealThreadCount()));

    ui->randomIterations->setEnabled(false);

    // Loading config.json at startup
    on_actionLoad_configuration_triggered();
}

MainWindow::~MainWindow()
{
    if (wizard != NULL) {
        delete wizard;
    }
    if (platesViewer != NULL) {
        delete platesViewer;
    }
    delete about;
    delete ui;
}

// Get the singleton instance of MainWindow
MainWindow *MainWindow::instance()
{
    foreach (QWidget *w, qApp->topLevelWidgets()) {
        if (MainWindow* mw = qobject_cast<MainWindow *>(w)) {
            return mw;
        }
    }
    return nullptr;
}

void MainWindow::updatePlateEnable()
{
    if (ui->circularPlate->isChecked()) {
        ui->plateWidth->setEnabled(false);
        ui->plateHeight->setEnabled(false);
        ui->diameter->setEnabled(true);
    } else {
        ui->plateWidth->setEnabled(true);
        ui->plateHeight->setEnabled(true);
        ui->diameter->setEnabled(false);
    }
}

void MainWindow::enableAll(bool enable)
{
    enabled = enable;
    ui->parts->setEnabled(enable);
    ui->outputDirectory->setEnabled(enable);
    ui->outputDirectoryButton->setEnabled(enable);
    ui->plateHeight->setEnabled(enable);
    ui->plateWidth->setEnabled(enable);
    ui->stlRadio->setEnabled(enable);
    ui->ppmRadio->setEnabled(enable);
    ui->precision->setEnabled(enable);
    ui->spacing->setEnabled(enable);
    ui->bruteForceSpacing->setEnabled(enable);
    ui->bruteForceAngle->setEnabled(enable);
    ui->randomIterations->setEnabled(enable);
    ui->circularPlate->setEnabled(enable);
    ui->diameter->setEnabled(enable);
    ui->nbThreads->setEnabled(enable);

    if (enable) {
        updatePlateEnable();
        ui->runButton->setText("Run");
    } else {
        ui->runButton->setText("Cancel");
    }
}

void MainWindow::showError(string error)
{
    error = "<font color=\"red\">" + error + "</font";
    ui->message->setText(QString::fromStdString(error));
    ui->message->show();
}

void MainWindow::showSuccess(string success)
{
    success = "<font color=\"green\"> " + success + "</font";
    ui->message->setText(QString::fromStdString(success));
    ui->message->show();
}

void MainWindow::showMessage(string msg)
{
    ui->message->setText(QString::fromStdString(msg));
    ui->message->show();
}

void MainWindow::wizardNext()
{
    if (stls.size() == 0) {
        return;
    }
    QString stl = stls.last();
    stls.pop_back();

    if (stl != "" && QFile::exists(stl)) {
        QByteArray geometry;
        bool hadWizard = false;
        if (wizard != NULL) {
            hadWizard = true;
            geometry = wizard->saveGeometry();
            wizard->close();
            delete wizard;
        }

        wizard = new Wizard(stl);
        connect(wizard, SIGNAL(accepted()), this, SLOT(on_wizard_accept()));
        wizard->show();
        wizard->restoreGeometry(geometry);

        if (isCircular()) {
            wizard->setCircularPlateDiameter(getPlateDiameter());
        } else {
            wizard->setPlateDimension(getPlateWidth(), getPlateHeight());
        }
    }
}

// Add all parts from the list by invoking wizardNext() for each file.
void MainWindow::addParts(QStringList stls)
{
    this->stls = stls;
    wizardNext();
}

bool MainWindow::isCircular()
{
    return ui->circularPlate->isChecked();
}

float MainWindow::getPlateDiameter()
{
    return ui->diameter->text().toFloat();
}

float MainWindow::getPlateWidth()
{
    return ui->plateWidth->text().toFloat();
}

float MainWindow::getPlateHeight()
{
    return ui->plateHeight->text().toFloat();
}

void MainWindow::on_outputDirectoryButton_clicked()
{
    ui->outputDirectory->setText(QFileDialog::getExistingDirectory());
}

void MainWindow::on_runButton_clicked()
{
    if (enabled) {
        enableAll(false);
        increaseVerboseLevel();
        if (isCircular()) {
            worker.request.setPlateSize(getPlateDiameter(), getPlateDiameter());
            worker.request.plateMode = PLATE_MODE_CIRCLE;
        } else {
            worker.request.setPlateSize(getPlateWidth(), getPlateHeight());
            worker.request.plateMode = PLATE_MODE_RECTANGLE;
        }
        worker.request.pattern = ui->outputDirectory->text().toStdString() + "/plate_%03d";
        worker.request.spacing = ui->spacing->text().toFloat()*1000;
        worker.request.precision = ui->precision->text().toFloat()*1000;
        worker.request.randomIterations = ui->randomIterations->text().toInt();
        worker.request.delta = ui->bruteForceSpacing->text().toFloat()*1000;
        worker.request.deltaR = DEG2RAD(ui->bruteForceAngle->text().toFloat());
        worker.parts = ui->parts->toPlainText().toStdString();
        worker.request.plateDiameter = ui->diameter->text().toFloat()*1000;
        worker.request.nbThreads = ui->nbThreads->text().toInt();

        if (ui->singleSort->isChecked()) {
            worker.request.sortMode = REQUEST_SINGLE_SORT;
        } else {
            worker.request.sortMode = REQUEST_MULTIPLE_SORTS;
        }

        if (ui->ppmRadio->isChecked()) {
            worker.request.mode = REQUEST_PPM;
        } else {
            worker.request.mode = REQUEST_STL;
        }

        worker.request.platesInfo = ui->outputCsv->isChecked();

        showSuccess("Working...");
        worker.start();
    } else {
        worker.request.cancel = true;
    }
}

void MainWindow::on_partBrowse_clicked()
{
    stls = QFileDialog::getOpenFileNames(this, "Select a part", workingDirectory, "*.stl");
    wizardNext();
}

void MainWindow::on_worker_end()
{
    if (worker.request.hasError) {
        showError(worker.request.error);
    } else {
        if (worker.request.cancel) {
            showSuccess("Generation cancelled");
        } else {
            ostringstream oss;
            oss << "Generation over, generated " << worker.request.plates << " plates.";
            showSuccess(oss.str());

            if (ui->viewAfter->isChecked()) {
                if (platesViewer != NULL) {
                    platesViewer->close();
                    delete platesViewer;
                }
                platesViewer = new PlatesViewer();
                if (isCircular()) {
                    platesViewer->setCircularPlateDiameter(getPlateDiameter());
                } else {
                    platesViewer->setPlateDimension(getPlateWidth(), getPlateHeight());
                }
                platesViewer->setPlates(worker.request.generatedFiles);
                platesViewer->show();
            }
        }
    }
    enableAll(true);
}

void MainWindow::on_about()
{
    about->show();
}

void MainWindow::on_wizard_accept()
{
    QString parts = ui->parts->toPlainText();
    wizard->close();

    if (wizard->getQuantity() > 0) {
        QString part = wizard->getPartFilename();
        QString partOptions = wizard->getPartOptions();
        string directory = getDirectory(part.toStdString());

        if (parts.trimmed() == "") {
            workingDirectory = QString::fromStdString(directory);
            chdir(workingDirectory.toUtf8().data());
        }
        if (directory == workingDirectory.toStdString()) {
            part = QString::fromStdString(getBasename(part.toStdString()));
        }

        parts += part + " " + partOptions + "\n";
        ui->parts->setText(parts);
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    QMetaObject::invokeMethod(this, &MainWindow::wizardNext, Qt::QueuedConnection);
#else
    QMetaObject::invokeMethod(this, "wizardNext", Qt::QueuedConnection);
#endif
}

void MainWindow::on_saveButton_clicked()
{
    QString filename = QFileDialog::getSaveFileName(this, "Select where to save plater.conf", workingDirectory + "/plater.conf");
    QString configuration = ui->parts->toPlainText();

    QFile conf(filename);
    conf.open(QIODevice::WriteOnly);
    conf.write(configuration.toStdString().c_str());
    conf.close();
}

void MainWindow::on_openButton_clicked()
{
    QString filename = QFileDialog::getOpenFileName(this, "Select a plater.conf", workingDirectory);

    QFile conf(filename);
    conf.open(QIODevice::ReadOnly);
    ui->parts->setText(conf.readAll());
    conf.close();

    workingDirectory = QString::fromStdString(getDirectory(filename.toStdString()));
    chdir(workingDirectory.toStdString().c_str());
}

void MainWindow::on_clearButton_clicked()
{
    ui->parts->setText("");
}

void MainWindow::timeOutSlot()
{
    if (worker.working) {
        if (worker.request.solution != NULL) {
            ostringstream oss;
            oss << "Current solution: " << worker.request.solution->countPlates() << " plates";
            showMessage(oss.str());
        }
        float value = 100*(worker.request.placerCurrent/(float)worker.request.placersCount);
        ui->progressBar->show();
        ui->progressBar->setValue(value);
    } else {
        ui->progressBar->hide();
    }
}

void MainWindow::on_circularPlate_clicked()
{
    updatePlateEnable();
}

void MainWindow::on_actionQuit_triggered()
{
    close();
}

void MainWindow::on_actionAdd_parts_triggered()
{
    on_partBrowse_clicked();
}

void MainWindow::on_actionOpen_plater_conf_triggered()
{
    on_openButton_clicked();
}

void MainWindow::on_actionSave_triggered()
{
    on_saveButton_clicked();
}

void MainWindow::on_singleSort_clicked()
{
    ui->randomIterations->setEnabled(false);
}

void MainWindow::on_multipleSort_clicked()
{
    ui->randomIterations->setEnabled(true);
}

void MainWindow::on_actionSave_configuration_triggered()
{
    QJsonObject json;

    // Settings
    json["plateWidth"] = ui->plateWidth->text();
    json["plateHeight"] = ui->plateHeight->text();
    json["circular"] = ui->circularPlate->isChecked() ? "yes" : "no";
    json["diameter"] = ui->diameter->text();
    json["outputDirectory"] = ui->outputDirectory->text();

    // Advanced settings
    json["partsSpacing"] = ui->spacing->text();
    json["precision"] = ui->precision->text();
    json["bruteForceSpacing"] = ui->bruteForceSpacing->text();
    json["bruteForceAngle"] = ui->bruteForceAngle->text();
    json["threads"] = ui->nbThreads->text();
    json["sortMode"] = ui->singleSort->isChecked() ? "single" : "multiple";
    json["randomIterations"] = ui->randomIterations->text();
    json["outputType"] = ui->stlRadio->isChecked() ? "stl" : "csv";
    json["outputPlatesCsv"] = ui->outputCsv->isChecked() ? "yes" : "no";

    QJsonDocument doc(json);
    QFile file("config.json");
    file.open(QIODevice::WriteOnly);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

void MainWindow::on_actionLoad_configuration_triggered()
{
    if (QFile::exists("config.json")) {
        QFile file("config.json");
        file.open(QIODevice::ReadOnly);
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject())  {
            QJsonObject json = doc.object();

            // Settings
            if (json.contains("plateWidth"))
                ui->plateWidth->setText(json["plateWidth"].toString());
            if (json.contains("plateHeight"))
                ui->plateHeight->setText(json["plateHeight"].toString());
            if (json.contains("circular")) {
                ui->circularPlate->setChecked(json["circular"].toString() == "yes");
            }
            if (json.contains("diameter")) {
                ui->diameter->setText(json["diameter"].toString());
            }
            if (json.contains("outputDirectory")) {
                ui->outputDirectory->setText(json["outputDirectory"].toString());
            }

            // Advanced settings
            if (json.contains("partsSpacing"))
                ui->spacing->setText(json["partsSpacing"].toString());
            if (json.contains("precision"))
                ui->precision->setText(json["precision"].toString());
            if (json.contains("bruteForceSpacing"))
                ui->bruteForceSpacing->setText(json["bruteForceSpacing"].toString());
            if (json.contains("bruteForceAngle"))
                ui->bruteForceAngle->setText(json["bruteForceAngle"].toString());
            if (json.contains("threads"))
                ui->nbThreads->setText(json["threads"].toString());
            if (json.contains("sortMode")) {
                if (json["sortMode"].toString() == "single") {
                    ui->singleSort->setChecked(true);
                } else {
                    ui->multipleSort->setChecked(true);
                }
            }
            if (json.contains("randomIterations")) {
                ui->randomIterations->setText(json["randomIterations"].toString());
            }
            if (json.contains("outputType")) {
                if (json["outputType"] == "stl") {
                    ui->stlRadio->setChecked(true);
                } else {
                    ui->ppmRadio->setChecked(true);
                }
            }
            if (json.contains("outputPlatesCsv")) {
                ui->outputCsv->setChecked(json["outputPlatesCsv"].toString() == "yes");
            }
        }
    }

    // Updating UI accordingly
    updatePlateEnable();
    ui->randomIterations->setEnabled(ui->multipleSort->isChecked());
}
