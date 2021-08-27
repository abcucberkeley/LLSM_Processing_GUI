#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mainadvanced.h"
#include "dsradvanced.h"
#include "deconadvanced.h"
#include "jobadvanced.h"
#include "datapaths.h"
#include "loadprevioussettings.h"
#include <QTextDocument>
#include <QObjectList>


using namespace matlab::engine;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Set the tabs widget as the main Widget
    this->setCentralWidget(ui->tabWidget);

    // Matlab Threading and connecting signals/slots
    mThreadManager = new matlabThreadManager(jobLogPaths,outputLock,this);
    connect(this, &MainWindow::jobStart, mThreadManager, &matlabThreadManager::onJobStart);
    connect(mThreadManager, &matlabThreadManager::enableSubmitButton, this, &MainWindow::onEnableSubmitButton);
    mThreadManager->start(QThread::HighestPriority);

    // Connect crop signals
    connect(ui->cropAddPathsButton, &QPushButton::clicked, this, &MainWindow::on_addPathsButton_clicked);
    connect(ui->cropResultPathBrowseButton, &QPushButton::clicked, this, &MainWindow::selectFolderPath);
    connect(ui->cropJobLogDirBrowseButton, &QPushButton::clicked, this, &MainWindow::selectFolderPath);
    //connect(ui->cropSubmitButton,&QPushButton::clicked, this, &MainWindow::on_submitButton_clicked);

    // Output Window Threading
    mOutputWindow = new matlabOutputWindow(jobLogPaths,jobNames,this);
    //mOutputWindowThread = new matlabOutputWindowThread(jobLogPaths,this);
    //connect(mThreadManager, &matlabThreadManager::addOutputIDAndPath, mOutputWindowThread, &matlabOutputWindowThread::onAddOutputIDAndPath);


    // Disable all tabs except the main one on startup
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->DSR),false);
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->Stitch),false);
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->Decon),false);
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->Job),false);

    // Set most recent Dir to Home initially
    mostRecentDir = QDir::homePath();

    // Check if version is up to date with what was last saved.
    QSettings settings("ABC", "LLSM GUI");
    settings.beginGroup("MainWindow");
    QString savedVersion = settings.value("version").toString();
    settings.endGroup();

    QCoreApplication::setApplicationVersion(VERSION_STRING);
    if(savedVersion == QCoreApplication::applicationVersion()){
        // Restore previous settings if user says yes
        checkLoadPrevSettings();
        if(loadSettings) readSettings();
    }
    else{
        // If saved version is not the current version then reset values to avoid corruption
        outputLock.lock();
        std::cout << "New Version has been detected. Resetting saved settings to avoid corruption." << std::endl;
        outputLock.unlock();
    }

    // Set current tab to the main tab
    ui->tabWidget->setCurrentWidget(ui->Main);

    // Job Output
    if(!mOutputWindow->isVisible()){
        mOutputWindow->setModal(false);
        mOutputWindow->show();
    }

}

MainWindow::~MainWindow()
{
    delete ui;

    // If the thread is not done, kill it (This may have to change later because it can be dangerous)
    if(!mThreadManager->isFinished()) mThreadManager->terminate();

}

// Event triggered when main window is closed
void MainWindow::closeEvent(QCloseEvent *event)
{
    // Write current user settings
    writeSettings();
    event->accept();

    // Close output window if visible
    if(mOutputWindow) mOutputWindow->close();
}

// Write user settings
void MainWindow::writeSettings()
{
    QSettings settings("ABC", "LLSM GUI");

    settings.beginGroup("MainWindow");

    settings.setValue("version", QCoreApplication::applicationVersion());

    settings.beginWriteArray("dPaths");
    for(unsigned int i = 0; i < dPaths.size(); i++)
    {
        settings.setArrayIndex(i);
        settings.setValue("dPathsimasterPath", dPaths[i].masterPath);
        settings.setValue("dPathsiincludeMaster", dPaths[i].includeMaster);
        settings.setValue("dPathsipattern", dPaths[i].pattern);
        settings.setValue("dPathsimaxDepth", dPaths[i].maxDepth);
    }
    settings.endArray();


    for(unsigned int i = 0; i < dPaths.size(); i++)
    {
        settings.beginWriteArray(QString::number(i)+"bool");
        unsigned int j = 0;
        for(const auto &path : dPaths[i].subPaths){
            settings.setArrayIndex(j);
            settings.setValue("i"+QString::number(i)+"j"+QString::number(j)+"booli",path.second.first);
            j++;
        }
        settings.endArray();

        settings.beginWriteArray(QString::number(i)+"subPath");
        j = 0;
        for(const auto &path : dPaths[i].subPaths){
            settings.setArrayIndex(j);
            settings.setValue("i"+QString::number(i)+"j"+QString::number(j)+"subPathi",path.second.second);
            j++;
        }
        settings.endArray();


    }


    // Save Main Settings

    settings.setValue("deskewOverwrite",ui->deskewOverwriteDataCheckBox->isChecked());
    settings.setValue("rotateOverwrite",ui->rotateOverwriteDataCheckBox->isChecked());
    settings.setValue("deskewAndRotateOverwrite",ui->deskewAndRotateOverwriteDataCheckBox->isChecked());
    settings.setValue("stitchOverwrite",ui->stitchOverwriteDataCheckBox->isChecked());

    settings.setValue("Streaming",ui->streamingCheckBox->isChecked());

    // SAVE FOR CHANNEL PATTERNS
    settings.beginWriteArray("channelLabels");
    for(unsigned int i = 0; i < channelWidgets.size(); i++)
    {
        settings.setArrayIndex(i);
        settings.setValue("channelLabelsi", channelWidgets[i].first->text());
    }
    settings.endArray();

    settings.beginWriteArray("channelChecks");
    for(unsigned int i = 0; i < channelWidgets.size(); i++)
    {
        settings.setArrayIndex(i);
        settings.setValue("channelChecksi", channelWidgets[i].second->isChecked());
    }
    settings.endArray();

    // Save custom patterns
    settings.setValue("customPatternsCheckBox",ui->customPatternsCheckBox->isChecked());
    settings.setValue("customPatterns", ui->customPatternsLineEdit->text());

    settings.setValue("skewAngle",guiVals.skewAngle);
    settings.setValue("dz",ui->dzLineEdit->text());
    settings.setValue("xyPixelSize",guiVals.xyPixelSize);
    settings.setValue("Reverse",guiVals.Reverse);
    settings.setValue("objectiveScan",ui->objectiveScanCheckBox->isChecked());
    settings.setValue("sCMOSCameraFlip",guiVals.sCMOSCameraFlip);

    settings.setValue("deskewSave16Bit",ui->deskewSave16BitCheckBox->isChecked());
    settings.setValue("rotateSave16Bit",ui->rotateSave16BitCheckBox->isChecked());
    settings.setValue("deskewAndRotateSave16Bit",ui->deskewAndRotateSave16BitCheckBox->isChecked());
    settings.setValue("stitchSave16Bit",ui->stitchSave16BitCheckBox->isChecked());

    settings.setValue("deskewOnlyFirstTP",ui->deskewOnlyFirstTPCheckBox->isChecked());
    settings.setValue("rotateOnlyFirstTP",ui->rotateOnlyFirstTPCheckBox->isChecked());
    settings.setValue("deskewAndRotateOnlyFirstTP",ui->deskewAndRotateOnlyFirstTPCheckBox->isChecked());
    settings.setValue("stitchOnlyFirstTP",ui->stitchOnlyFirstTPCheckBox->isChecked());

    // Save Pipeline Settings

    settings.setValue("Deskew",ui->deskewCheckBox->isChecked());
    settings.setValue("Rotate",ui->rotateCheckBox->isChecked());
    settings.setValue("Stitch",ui->stitchCheckBox->isChecked());
    settings.setValue("deskewAndRotate",ui->deskewAndRotateCheckBox->isChecked());

    // decon only settings
    settings.setValue("deconOnly",ui->deconOnlyCheckBox->isChecked());
    settings.setValue("deconOnlyOnlyFirstTP",ui->deconOnlyOnlyFirstTPCheckBox->isChecked());
    settings.setValue("deconOnlyOverwriteData",ui->deconOnlyOverwriteDataCheckBox->isChecked());
    settings.setValue("deconOnlySave16Bit",ui->deconOnlySave16BitCheckBox->isChecked());

    settings.setValue("deskewDecon",ui->deskewDeconCheckBox->isChecked());
    settings.setValue("rotateDecon",ui->rotateDeconCheckBox->isChecked());
    settings.setValue("deskewAndRotateDecon",ui->deskewAndRotateDeconCheckBox->isChecked());
    settings.setValue("stitchDecon",ui->stitchDeconCheckBox->isChecked());

    // Save DSR Settings

    settings.setValue("parseSettingFile",ui->parseSettingsFileCheckBox->isChecked());
    settings.setValue("flipZstack",ui->flipZStackCheckBox->isChecked());

    settings.setValue("LLFFCorrection",ui->llffCorrectionCheckBox->isChecked());

    settings.beginWriteArray("LSImagePaths");
    for(unsigned int i = 0; i < lsImagePaths.size(); i++)
    {
        settings.setArrayIndex(i);
        settings.setValue("LSImagePathsi", lsImagePaths[i]);
    }
    settings.endArray();

    settings.beginWriteArray("BackgroundPaths");
    for(unsigned int i = 0; i < backgroundPaths.size(); i++)
    {
        settings.setArrayIndex(i);
        settings.setValue("BackgroundPathsi", backgroundPaths[i]);
    }
    settings.endArray();

    // Save DSR Advanced Settings

    settings.setValue("BKRemoval",guiVals.BKRemoval);
    settings.setValue("LowerLimit",guiVals.LowerLimit);
    settings.setValue("resampleType",guiVals.resampleType);
    settings.setValue("resample",QVariant::fromValue(guiVals.resample));

    // Save Stitch Settings

    settings.setValue("stitchPipeline", ui->stitchPipelineComboBox->currentText());
    settings.setValue("stitchResultsDir", ui->resultsDirLineEdit->text());
    settings.setValue("imageListFullPaths", ui->imageListFullPathsLineEdit->text());
    settings.setValue("axisOrder", ui->axisOrderLineEdit->text());
    settings.setValue("blendMethod", ui->blendMethodComboBox->currentText());
    settings.setValue("xcorrShift", ui->xCorrShiftCheckBox->isChecked());
    settings.setValue("xcorrMode", ui->xCorrModeComboBox->currentText());

    settings.setValue("boundBox", ui->boundBoxCheckBox->isChecked());
    settings.setValue("bbYMin", ui->boundBoxYMinSpinBox->value());
    settings.setValue("bbXMin", ui->boundBoxXMinSpinBox->value());
    settings.setValue("bbZMin", ui->boundBoxZMinSpinBox->value());
    settings.setValue("bbYMax", ui->boundBoxYMaxSpinBox->value());
    settings.setValue("bbXMax", ui->boundBoxXMaxSpinBox->value());
    settings.setValue("bbZMax", ui->boundBoxZMaxSpinBox->value());

    settings.setValue("primaryCh", ui->primaryCHLineEdit->text());

    // Save Decon Settings

    settings.setValue("matlabDecon", ui->matlabDeconRadioButton->isChecked());
    settings.setValue("cudaDecon", ui->cudaDeconRadioButton->isChecked());
    settings.setValue("cppDecon", ui->cppDeconRadioButton->isChecked());
    settings.setValue("DS", ui->deskewDeconCheckBox->isChecked());
    settings.setValue("DSR", ui->deskewAndRotateDeconCheckBox->isChecked());
    settings.setValue("Background", ui->backgroundIntensityLineEdit->text());
    settings.setValue("dzPSF", ui->dzPSFLineEdit->text());
    settings.setValue("edgeErosion", ui->edgeErosionLineEdit->text());
    settings.setValue("ErodeByFTP", ui->erodeByFTPCheckBox->isChecked());
    settings.setValue("deconRotate", ui->deconRotateCheckBox->isChecked());


    // Save Decon Advaced Settings

    settings.setValue("cppDeconPath",guiVals.cppDeconPath);
    settings.setValue("loadModules",guiVals.loadModules);
    settings.setValue("cudaDeconPath",guiVals.cudaDeconPath);
    settings.setValue("OTFGENPath",guiVals.OTFGENPath);
    settings.setValue("RLMethod",guiVals.RLMethod);
    settings.setValue("fixIter", guiVals.fixIter);
    settings.setValue("errThresh", guiVals.errThresh);
    settings.setValue("debug", guiVals.debug);
    settings.setValue("gpuJob", guiVals.gpuJob);

    settings.beginWriteArray("psfFullpaths");
    for(unsigned int i = 0; i < psfFullPaths.size(); i++)
    {
        settings.setArrayIndex(i);
        settings.setValue("psfFullpathsi", psfFullPaths[i]);
    }
    settings.endArray();

    settings.setValue("DeconIter", ui->deconIterationsLineEdit->text());
    settings.setValue("rotatedPSF", ui->rotatePSFCheckBox->isChecked());

    // Save Job Settings

    settings.setValue("parseCluster", ui->parseClusterCheckBox->isChecked());
    settings.setValue("cpusPerTask", ui->cpusPerTaskLineEdit->text());
    settings.setValue("cpuOnlyNodes", ui->cpuOnlyNodesCheckBox->isChecked());

    // Save Advanced Job Settings

    settings.setValue("largeFile",QVariant::fromValue(guiVals.largeFile));
    settings.setValue("jobLogDir",guiVals.jobLogDir);
    settings.setValue("uuid",guiVals.uuid);
    settings.setValue("maxTrialNum",QVariant::fromValue(guiVals.maxTrialNum));
    settings.setValue("unitWaitTime",QVariant::fromValue(guiVals.unitWaitTime));
    settings.setValue("minModifyTime",QVariant::fromValue(guiVals.minModifyTime));
    settings.setValue("maxModifyTime",QVariant::fromValue(guiVals.maxModifyTime));
    settings.setValue("maxWaitLoopNum",QVariant::fromValue(guiVals.maxWaitLoopNum));
    settings.setValue("MatlabLaunchStr",guiVals.MatlabLaunchStr);
    settings.setValue("SlurmParam",guiVals.SlurmParam);

    // ********* Save Crop Settings *********

    // Save Data Paths
    settings.beginWriteArray("cropDPaths");
    for(unsigned int i = 0; i < cropDPaths.size(); i++)
    {
        settings.setArrayIndex(i);
        settings.setValue("cropDPathsimasterPath", cropDPaths[i].masterPath);
        settings.setValue("cropDPathsiincludeMaster", cropDPaths[i].includeMaster);
        settings.setValue("cropDPathsipattern", cropDPaths[i].pattern);
        settings.setValue("cropDPathsimaxDepth", cropDPaths[i].maxDepth);
    }
    settings.endArray();


    for(unsigned int i = 0; i < cropDPaths.size(); i++)
    {
        settings.beginWriteArray("crop"+QString::number(i)+"bool");
        unsigned int j = 0;
        for(const auto &path : cropDPaths[i].subPaths){
            settings.setArrayIndex(j);
            settings.setValue("cropi"+QString::number(i)+"j"+QString::number(j)+"booli",path.second.first);
            j++;
        }
        settings.endArray();

        settings.beginWriteArray("crop"+QString::number(i)+"subPath");
        j = 0;
        for(const auto &path : cropDPaths[i].subPaths){
            settings.setArrayIndex(j);
            settings.setValue("cropi"+QString::number(i)+"j"+QString::number(j)+"subPathi",path.second.second);
            j++;
        }
        settings.endArray();


    }

    // SAVE FOR CHANNEL PATTERNS
    settings.beginWriteArray("cropChannelLabels");
    for(unsigned int i = 0; i < cropChannelWidgets.size(); i++)
    {
        settings.setArrayIndex(i);
        settings.setValue("cropChannelLabelsi", cropChannelWidgets[i].first->text());
    }
    settings.endArray();

    settings.beginWriteArray("cropChannelChecks");
    for(unsigned int i = 0; i < cropChannelWidgets.size(); i++)
    {
        settings.setArrayIndex(i);
        settings.setValue("cropChannelChecksi", cropChannelWidgets[i].second->isChecked());
    }
    settings.endArray();

    // Save custom patterns
    settings.setValue("cropCustomPatternsCheckBox",ui->cropCustomPatternsCheckBox->isChecked());
    settings.setValue("cropCustomPatterns", ui->cropCustomPatternsLineEdit->text());

    settings.setValue("cropCropType",ui->cropCropTypeComboBox->currentText());

    settings.setValue("cropBoundBoxYMin",ui->cropBoundBoxYMinSpinBox->value());
    settings.setValue("cropBoundBoxXMin",ui->cropBoundBoxXMinSpinBox->value());
    settings.setValue("cropBoundBoxZMin",ui->cropBoundBoxZMinSpinBox->value());
    settings.setValue("cropBoundBoxYMax",ui->cropBoundBoxYMaxSpinBox->value());
    settings.setValue("cropBoundBoxXMax",ui->cropBoundBoxXMaxSpinBox->value());
    settings.setValue("cropBoundBoxZMax",ui->cropBoundBoxZMaxSpinBox->value());

    settings.setValue("cropLastStartY",ui->cropLastStartYSpinBox->value());
    settings.setValue("cropLastStartX",ui->cropLastStartYSpinBox->value());
    settings.setValue("cropLastStartZ",ui->cropLastStartYSpinBox->value());

    settings.setValue("cropSave16Bit",ui->cropSave16BitCheckBox->isChecked());
    settings.setValue("cropPad",ui->cropPadCheckBox->isChecked());

    settings.setValue("cropResultPath", ui->cropResultPathLineEdit->text());
    settings.setValue("cropParseCluster", ui->cropParseClusterCheckBox->isChecked());
    settings.setValue("cropCpusPerTask", ui->cropCpusPerTaskLineEdit->text());
    settings.setValue("cropMasterCompute",ui->cropMasterComputeCheckBox->isChecked());
    settings.setValue("cropCpuOnlyNodes", ui->cropCpuOnlyNodesCheckBox->isChecked());
    settings.setValue("cropJobLogDir",ui->cropJobLogDirLineEdit->text());
    settings.setValue("cropUuid",ui->cropUuidLineEdit->text());

    settings.endGroup();
}

// Restore previous user settings
void MainWindow::readSettings()
{
    QSettings settings("ABC", "LLSM GUI");

    settings.beginGroup("MainWindow");

    // Read Data Paths
    int size = settings.beginReadArray("dPaths");
    for (int i = 0; i < size; i++){
        settings.setArrayIndex(i);
        dPaths.push_back(dataPath());
        dPaths[i].masterPath = settings.value("dPathsimasterPath").toString();
        dPaths[i].includeMaster = settings.value("dPathsiincludeMaster").toBool();
        dPaths[i].pattern = settings.value("dPathsipattern").toString();
        dPaths[i].maxDepth = settings.value("dPathsimaxDepth").toInt();
    }
    settings.endArray();

    for(size_t i = 0; i < dPaths.size(); i++){
        std::vector<QString> subPaths;
        std::vector<bool> include;

        size = settings.beginReadArray(QString::number(i)+"bool");
        for(int j = 0; j < size; j++){
            settings.setArrayIndex(j);
            include.push_back(settings.value("i"+QString::number(i)+"j"+QString::number(j)+"booli").toBool());
        }
        settings.endArray();

        size = settings.beginReadArray(QString::number(i)+"subPath");
        for(int j = 0; j < size; j++){
            settings.setArrayIndex(j);
            subPaths.push_back(settings.value("i"+QString::number(i)+"j"+QString::number(j)+"subPathi").toString());
        }
        settings.endArray();

        for(int j = 0; j < size; j++){
            dPaths[i].subPaths.emplace(subPaths[j],std::make_pair(include[j],subPaths[j]));
        }

    }




    // Read Main Settings

    ui->deskewOverwriteDataCheckBox->setChecked(settings.value("deskewOverwrite").toBool());
    ui->rotateOverwriteDataCheckBox->setChecked(settings.value("rotateOverwrite").toBool());
    ui->deskewAndRotateOverwriteDataCheckBox->setChecked(settings.value("deskewAndRotateOverwrite").toBool());
    ui->stitchOverwriteDataCheckBox->setChecked(settings.value("stitchOverwrite").toBool());

    ui->streamingCheckBox->setChecked(settings.value("Streaming").toBool());

    // READ FOR CHANNEL PATTERNS

    // temp vectors to hold labels and checkbox values
    std::vector<QString> tLabels;
    std::vector<bool> tChecks;

    // Read labels and checks into vectors
    size = settings.beginReadArray("channelLabels");
    for(int i = 0; i < size; i++)
    {
        settings.setArrayIndex(i);
        tLabels.push_back(settings.value("channelLabelsi").toString());
    }
    settings.endArray();

    size = settings.beginReadArray("channelChecks");
    for(int i = 0; i < size; i++)
    {
        settings.setArrayIndex(i);
        tChecks.push_back(settings.value("channelChecksi").toBool());
    }
    settings.endArray();

    // Read custom patterns
    ui->customPatternsCheckBox->setChecked(settings.value("customPatternsCheckBox").toBool());
    ui->customPatternsLineEdit->setText(settings.value("customPatterns").toString());

    // Create widgets and put them in channelWidgets
    for(size_t i = 0; i < tLabels.size(); i++){
        QLabel* label = new QLabel(ui->Main);
        label->setTextFormat(Qt::RichText);
        label->setText("<b>"+tLabels[i]+"<\b>");
        ui->horizontalLayout_4->addWidget(label);

        QCheckBox* checkBox = new QCheckBox(ui->Main);
        checkBox->setChecked(tChecks[i]);
        ui->horizontalLayout_4->addWidget(checkBox);

        channelWidgets.push_back(std::make_pair(label,checkBox));

    }

    guiVals.skewAngle = settings.value("skewAngle").toDouble();
    ui->dzLineEdit->setText(settings.value("dz").toString());
    guiVals.xyPixelSize = settings.value("xyPixelSize").toDouble();
    guiVals.Reverse = settings.value("Reverse").toBool();
    ui->objectiveScanCheckBox->setChecked(settings.value("objectiveScan").toBool());
    guiVals.sCMOSCameraFlip = settings.value("sCMOSCameraFlip").toBool();

    ui->deskewSave16BitCheckBox->setChecked(settings.value("deskewSave16Bit").toBool());
    ui->rotateSave16BitCheckBox->setChecked(settings.value("rotateSave16Bit").toBool());
    ui->deskewAndRotateSave16BitCheckBox->setChecked(settings.value("deskewAndRotateSave16Bit").toBool());
    ui->stitchSave16BitCheckBox->setChecked(settings.value("stitchSave16Bit").toBool());

    ui->deskewOnlyFirstTPCheckBox->setChecked(settings.value("deskewOnlyFirstTP").toBool());
    ui->rotateOnlyFirstTPCheckBox->setChecked(settings.value("rotateOnlyFirstTP").toBool());
    ui->deskewAndRotateOnlyFirstTPCheckBox->setChecked(settings.value("deskewAndRotateOnlyFirstTP").toBool());
    ui->stitchOnlyFirstTPCheckBox->setChecked(settings.value("stitchOnlyFirstTP").toBool());

    // Read Pipeline Settings

    ui->deskewCheckBox->setChecked(settings.value("Deskew").toBool());
    ui->rotateCheckBox->setChecked(settings.value("Rotate").toBool());
    ui->stitchCheckBox->setChecked(settings.value("Stitch").toBool());
    ui->deskewAndRotateCheckBox->setChecked(settings.value("deskewAndRotate").toBool());

    ui->deconOnlyCheckBox->setChecked(settings.value("deconOnly").toBool());
    ui->deconOnlyOnlyFirstTPCheckBox->setChecked(settings.value("deconOnlyOnlyFirstTP").toBool());
    ui->deconOnlyOverwriteDataCheckBox->setChecked(settings.value("deconOnlyOverwriteData").toBool());
    ui->deconOnlySave16BitCheckBox->setChecked(settings.value("deconOnlySave16Bit").toBool());

    ui->deskewDeconCheckBox->setChecked(settings.value("deskewDecon").toBool());
    ui->rotateDeconCheckBox->setChecked(settings.value("rotateDecon").toBool());
    ui->deskewAndRotateDeconCheckBox->setChecked(settings.value("deskewAndRotateDecon").toBool());
    ui->stitchDeconCheckBox->setChecked(settings.value("stitchDecon").toBool());

    // Read DSR Settings

    ui->parseSettingsFileCheckBox->setChecked(settings.value("parseSettingFile").toBool());
    ui->flipZStackCheckBox->setChecked(settings.value("flipZstack").toBool());

    ui->llffCorrectionCheckBox->setChecked(settings.value("LLFFCorrection").toBool());

    size = settings.beginReadArray("LSImagePaths");
    for(int i = 0; i < size; i++)
    {
        settings.setArrayIndex(i);
        lsImagePaths.push_back(settings.value("LSImagePathsi").toString());
    }
    settings.endArray();

    size = settings.beginReadArray("BackgroundPaths");
    for(int i = 0; i < size; i++)
    {
        settings.setArrayIndex(i);
        backgroundPaths.push_back(settings.value("BackgroundPathsi").toString());
    }
    settings.endArray();

    // Read DSR Advanced Settings

    guiVals.BKRemoval = settings.value("BKRemoval").toBool();
    guiVals.LowerLimit = settings.value("LowerLimit").toDouble();
    guiVals.resampleType = settings.value("resampleType").toString();
    guiVals.resample = settings.value("resample").toULongLong();

    // Read Stitch Settings
    ui->stitchPipelineComboBox->setCurrentText(settings.value("stitchPipeline").toString());
    ui->resultsDirLineEdit->setText(settings.value("stitchResultsDir").toString());
    ui->imageListFullPathsLineEdit->setText(settings.value("imageListFullPaths").toString());
    ui->axisOrderLineEdit->setText(settings.value("axisOrder").toString());
    ui->blendMethodComboBox->setCurrentText(settings.value("blendMethod").toString());
    ui->xCorrShiftCheckBox->setChecked(settings.value("xcorrShift").toBool());
    ui->xCorrModeComboBox->setCurrentText(settings.value("xcorrMode").toString());

    ui->boundBoxCheckBox->setChecked(settings.value("boundBox").toBool());
    ui->boundBoxYMinSpinBox->setValue(settings.value("bbYMin").toInt());
    ui->boundBoxXMinSpinBox->setValue(settings.value("bbXMin").toInt());
    ui->boundBoxZMinSpinBox->setValue(settings.value("bbZMin").toInt());
    ui->boundBoxYMaxSpinBox->setValue(settings.value("bbYMax").toInt());
    ui->boundBoxXMinSpinBox->setValue(settings.value("bbXMax").toInt());
    ui->boundBoxZMinSpinBox->setValue(settings.value("bbZMin").toInt());

    ui->primaryCHLineEdit->setText(settings.value("primaryCh").toString());


    // Read Decon Settings

    ui->matlabDeconRadioButton->setChecked(settings.value("matlabDecon").toBool());
    ui->cudaDeconRadioButton->setChecked(settings.value("cudaDecon").toBool());
    ui->cppDeconRadioButton->setChecked(settings.value("cppDecon").toBool());
    ui->deskewDeconCheckBox->setChecked(settings.value("DS").toBool());
    ui->deskewAndRotateDeconCheckBox->setChecked(settings.value("DSR").toBool());
    ui->backgroundIntensityLineEdit->setText(settings.value("Background").toString());
    ui->dzPSFLineEdit->setText(settings.value("dzPSF").toString());
    ui->edgeErosionLineEdit->setText(settings.value("edgeErosion").toString());
    ui->erodeByFTPCheckBox->setChecked(settings.value("ErodeByFTP").toBool());
    ui->deconRotateCheckBox->setChecked(settings.value("deconRotate").toBool());

    // Read Decon Advaced Settings

    guiVals.cppDeconPath = settings.value("cppDeconPath").toString();
    guiVals.loadModules = settings.value("loadModules").toString();
    guiVals.cudaDeconPath = settings.value("cudaDeconPath").toString();
    guiVals.OTFGENPath = settings.value("OTFGENPath").toString();
    guiVals.RLMethod = settings.value("RLMethod").toString();
    guiVals.fixIter = settings.value("fixIter").toBool();
    guiVals.errThresh = settings.value("errThresh").toDouble();
    guiVals.debug = settings.value("debug").toBool();
    guiVals.gpuJob = settings.value("gpuJob").toBool();


    size = settings.beginReadArray("psfFullpaths");
    for(int i = 0; i < size; i++)
    {
        settings.setArrayIndex(i);
        psfFullPaths.push_back(settings.value("psfFullpathsi").toString());
    }
    settings.endArray();

    ui->deconIterationsLineEdit->setText(settings.value("DeconIter").toString());
    ui->rotatePSFCheckBox->setChecked(settings.value("rotatedPSF").toBool());

    // Read Job Settings
    ui->parseClusterCheckBox->setChecked(settings.value("parseCluster").toBool());
    ui->cpusPerTaskLineEdit->setText(settings.value("cpusPerTask").toString());
    ui->cpuOnlyNodesCheckBox->setChecked(settings.value("cpuOnlyNodes").toBool());


    // Save Advanced Job Settings

    guiVals.largeFile = settings.value("largeFile").toBool();
    guiVals.jobLogDir = settings.value("jobLogDir").toString();
    guiVals.uuid = settings.value("uuid").toString();
    guiVals.maxTrialNum = settings.value("maxTrialNum").toULongLong();
    guiVals.unitWaitTime = settings.value("unitWaitTime").toULongLong();
    guiVals.minModifyTime = settings.value("minModifyTime").toULongLong();
    guiVals.maxModifyTime = settings.value("maxModifyTime").toULongLong();
    guiVals.maxWaitLoopNum = settings.value("maxWaitLoopNum").toULongLong();
    guiVals.MatlabLaunchStr = settings.value("MatlabLaunchStr").toString();
    guiVals.SlurmParam = settings.value("SlurmParam").toString();

    // ********* Read Crop Settings *********

    // Read Data Paths
    size = settings.beginReadArray("cropDPaths");
    for (int i = 0; i < size; i++){
        settings.setArrayIndex(i);
        cropDPaths.push_back(dataPath());
        cropDPaths[i].masterPath = settings.value("cropDPathsimasterPath").toString();
        cropDPaths[i].includeMaster = settings.value("cropDPathsiincludeMaster").toBool();
        cropDPaths[i].pattern = settings.value("cropDPathsipattern").toString();
        cropDPaths[i].maxDepth = settings.value("cropDPathsimaxDepth").toInt();
    }
    settings.endArray();

    for(size_t i = 0; i < cropDPaths.size(); i++){
        std::vector<QString> subPaths;
        std::vector<bool> include;

        size = settings.beginReadArray("crop"+QString::number(i)+"bool");
        for(int j = 0; j < size; j++){
            settings.setArrayIndex(j);
            include.push_back(settings.value("cropi"+QString::number(i)+"j"+QString::number(j)+"booli").toBool());
        }
        settings.endArray();

        size = settings.beginReadArray("crop"+QString::number(i)+"subPath");
        for(int j = 0; j < size; j++){
            settings.setArrayIndex(j);
            subPaths.push_back(settings.value("cropi"+QString::number(i)+"j"+QString::number(j)+"subPathi").toString());
        }
        settings.endArray();

        for(int j = 0; j < size; j++){
            cropDPaths[i].subPaths.emplace(subPaths[j],std::make_pair(include[j],subPaths[j]));
        }

    }

    // temp vectors to hold labels and checkbox values
    tLabels.clear();
    tChecks.clear();

    // READ FOR CHANNEL PATTERNS
    size = settings.beginReadArray("cropChannelLabels");
    for(int i = 0; i < size; i++)
    {
        settings.setArrayIndex(i);
        tLabels.push_back(settings.value("cropChannelLabelsi").toString());

    }
    settings.endArray();

    size = settings.beginReadArray("cropChannelChecks");
    for(int i = 0; i < size; i++)
    {
        settings.setArrayIndex(i);
        tChecks.push_back(settings.value("cropChannelChecksi").toBool());
    }
    settings.endArray();

    // Create widgets and put them in channelWidgets
    for(size_t i = 0; i < tLabels.size(); i++){
        QLabel* label = new QLabel(ui->Crop);
        label->setTextFormat(Qt::RichText);
        label->setText("<b>"+tLabels[i]+"<\b>");
        ui->cropChannelPatternsHorizontalLayout->addWidget(label);

        QCheckBox* checkBox = new QCheckBox(ui->Crop);
        checkBox->setChecked(tChecks[i]);
        ui->cropChannelPatternsHorizontalLayout->addWidget(checkBox);

        cropChannelWidgets.push_back(std::make_pair(label,checkBox));

    }

    // Read custom patterns
    ui->cropCustomPatternsCheckBox->setChecked(settings.value("cropCustomPatternsCheckBox").toBool());
    ui->cropCustomPatternsLineEdit->setText(settings.value("cropCustomPatterns").toString());

    ui->cropCropTypeComboBox->setCurrentText(settings.value("cropCropType").toString());

    ui->cropBoundBoxYMinSpinBox->setValue(settings.value("cropBoundBoxYMin").toULongLong());
    ui->cropBoundBoxXMinSpinBox->setValue(settings.value("cropBoundBoxXMin").toULongLong());
    ui->cropBoundBoxZMinSpinBox->setValue(settings.value("cropBoundBoxZMin").toULongLong());
    ui->cropBoundBoxYMaxSpinBox->setValue(settings.value("cropBoundBoxYMax").toULongLong());
    ui->cropBoundBoxXMaxSpinBox->setValue(settings.value("cropBoundBoxXMax").toULongLong());
    ui->cropBoundBoxZMaxSpinBox->setValue(settings.value("cropBoundBoxZMax").toULongLong());

    ui->cropLastStartYSpinBox->setValue(settings.value("cropLastStartY").toULongLong());
    ui->cropLastStartXSpinBox->setValue(settings.value("cropLastStartX").toULongLong());
    ui->cropLastStartZSpinBox->setValue(settings.value("cropLastStartZ").toULongLong());

    ui->cropSave16BitCheckBox->setChecked(settings.value("cropSave16Bit").toBool());
    ui->cropPadCheckBox->setChecked(settings.value("cropPad").toBool());

    ui->cropResultPathLineEdit->setText(settings.value("cropResultPath").toString());
    ui->cropParseClusterCheckBox->setChecked(settings.value("cropParseCluster").toBool());
    ui->cropCpusPerTaskLineEdit->setText(settings.value("cropCpusPerTask").toString());
    ui->cropMasterComputeCheckBox->setChecked(settings.value("cropMasterCompute").toBool());
    ui->cropCpuOnlyNodesCheckBox->setChecked(settings.value("cropCpuOnlyNodes").toBool());
    ui->cropJobLogDirLineEdit->setText(settings.value("cropJobLogDir").toString());
    ui->cropUuidLineEdit->setText(settings.value("cropUuid").toString());

    settings.endGroup();
}

// Reenable submit button for new jobs
void MainWindow::onEnableSubmitButton(){

    ui->cropSubmitButton->setEnabled(true);
    ui->submitButton->setEnabled(true);

    //QMessageBox msgBox;
    //msgBox.setText("Job Submitted. Ready for new job!");
    //msgBox.setIcon(QMessageBox::Information);
    //msgBox.exec();
}

// Open DSR Advanced Settings
void MainWindow::on_dsrAdvancedSettingsButton_clicked()
{
    DSRadvanced dsrAdvanced(guiVals);
    dsrAdvanced.setModal(true);
    dsrAdvanced.exec();
}

// Open Decon Advanced Settings
void MainWindow::on_deconAdvancedSettingsButton_clicked()
{
    deconAdvanced dAdvanced(guiVals);
    dAdvanced.setModal(true);
    dAdvanced.exec();
}

// Open Job Advanced Settings
void MainWindow::on_jobAdvancedSettingsButton_clicked()
{
    jobAdvanced jAdvanced(guiVals);
    jAdvanced.setModal(true);
    jAdvanced.exec();
}

// Submit settings
void MainWindow::on_submitButton_clicked()
{
    // Testing object list. Looping through all widgets
    /*
    QList<QWidget *> widgetList = ui->tabWidget->findChildren<QWidget *>();
    std::cout << "Amount of c " << std::endl;
    std::cout << "Amount of children found : " << widgetList.count() << std::endl;
    for(auto widget : widgetList){
        if(widget->objectName().toStdString().find("CheckBox") != QString::npos){
            std::cout << widget->objectName().toStdString() << std::endl;
        }
    }
    return;
    */

    // Testing data path check form
    if(ui->checkDataPathsCheckBox->isChecked()){
    std::vector<QString> patterns;
    if(!ui->customPatternsCheckBox->isChecked()){
        if(channelWidgets.size()){
            // Grab indexes of checked boxes
            std::vector<int> indexes;
            for(size_t i = 0; i < channelWidgets.size(); i++){
                if(channelWidgets[i].second->isChecked()) indexes.push_back(i);
            }
            patterns.resize(indexes.size());
            int cpi = 0;
            // Go through checked indexes and the label text (channel pattern) in the cell array
            for(int i : indexes){

                // Convert from rich text to plain text
                QTextDocument toPlain;
                toPlain.setHtml(channelWidgets[i].first->text());

                patterns[cpi] = toPlain.toPlainText();
                cpi++;
            }
        }
    }
    // Use custom patterns
    else{
        QString patternLine = ui->customPatternsLineEdit->text();
        QString pattern;
        for(int i = 0; i < patternLine.size(); i++){
            if(patternLine[i] == ','){
                patterns.push_back(pattern);
                pattern.clear();
            }
            else{
                pattern.push_back(patternLine[i]);
            }
        }
        if(pattern.size()) patterns.push_back(pattern);
    }
    dataPathCheck dPC(dPaths,patterns,ui->dzLineEdit->text());
    dPC.setModal(true);
    dPC.exec();
    return;
    }

    // Save settings in case of crash
    writeSettings();


    // TODO: Seperate functions for error checking

    // Error if data path does not exist when submit is pressed
    for(size_t i = 0; i < dPaths.size(); i++){
        if(dPaths[i].includeMaster){
            if(!QFileInfo::exists(dPaths[i].masterPath)){
                QMessageBox messageBox;
                messageBox.warning(0,"Error","Data path \"" + dPaths[i].masterPath + "\" does not exist!");
                messageBox.setFixedSize(500,200);
                return;
            }
        }
        for (const auto &subPath : dPaths[i].subPaths){
            if(subPath.second.first){
                if(!QFileInfo::exists(subPath.second.second)){
                    QMessageBox messageBox;
                    messageBox.warning(0,"Error","Data path \"" + subPath.second.second + "\" does not exist!");
                    messageBox.setFixedSize(500,200);
                    return;
                }
            }
        }
    }

    // Error if decon is set but no psf paths are set
    if((ui->deconOnlyCheckBox->isChecked() || ui->deskewDeconCheckBox->isChecked() || ui->rotateDeconCheckBox->isChecked() || ui->deskewAndRotateDeconCheckBox->isChecked() || ui->stitchDeconCheckBox->isChecked()) && !psfFullPaths.size()){
        QMessageBox messageBox;
        messageBox.warning(0,"Error","Decon is set but there are no psf paths set");
        messageBox.setFixedSize(500,200);
        return;
    }
    else{
        for(const QString &path : psfFullPaths){
            if(path.isEmpty()){
                QMessageBox messageBox;
                messageBox.warning(0,"Error","One of the PSF paths is empty!");
                messageBox.setFixedSize(500,200);
                return;
            }
            else if(!QFileInfo::exists(path)){
                QMessageBox messageBox;
                messageBox.warning(0,"Error","Psf path \"" + path + "\" does not exist!");
                messageBox.setFixedSize(500,200);
                return;
            }
        }
    }



    // Make it so the user can't submit another job while we are submitting this one
    ui->submitButton->setEnabled(false);

    matlabJobSettings submission = submitSettings(false,ui->dzLineEdit->text());
    //QString test = std::get<0>(submission.mPathJNameParseCluster);
    //qDebug() << test;
    emit jobStart(submission);
}

matlabJobSettings MainWindow::submitSettings(bool checkDataPaths, QString dz){
    // We need this to convert C++ vars to MATLAB vars
    matlab::data::ArrayFactory factory;

    // outA is the number of outputs (always zero) and data is the structure to hold the pipeline settings
    size_t outA = 0;
    std::vector<matlab::data::Array> data;

    // NOTE: We have to push a lot of things into our data array one at a time
    // Potentially in the future I can loop through the widgets and do this in fewer lines

    //
    //
    // TODO: GOING TO REDO THIS PORTION AND MAKE A FUNCTION SO I CAN SUPPORT MULTIPLE SCRIPTS EASIER
    //
    //

    // Set main path. This is where all the output files made by the GUI will be stored if a job log dir does not exist.
    //QString dateTime = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_");
    QString dateTime = QDateTime::currentDateTime().toString("yyyyMMdd_HHmm_");
    QString timeJobName = dateTime+QString(ui->jobNameLineEdit->text()).replace(" ","_");
    QString mainPath = dPaths[0].masterPath+"/job_logs/"+timeJobName;

    // Check for job log directory for main job
    QString jobLogCopy = guiVals.jobLogDir;
    //if(ui->parseClusterCheckBox->isChecked()){
    QDir dir(guiVals.jobLogDir);
    if (!dir.exists()){
        QDir mDir(mainPath);
        if(!mDir.exists()){
            mDir.mkpath(".");
        }
        guiVals.jobLogDir = mainPath;
        std::cout << "Chosen job log directory does not exist! Using " << guiVals.jobLogDir.toStdString()<< " as the job log directory instead." << std::endl;
    }
    else{
        mainPath = guiVals.jobLogDir+"/"+timeJobName;
        QDir mDir(mainPath);
        if(!mDir.exists()){
            mDir.mkpath(".");
        }
    }
    //}

    // Reset jobLogDir to what it was before at the end of this function


    if(ui->deconOnlyCheckBox->isChecked()){
        // Data Paths
        unsigned long long numPaths = 0;
        for(const auto &path : dPaths){
            if(path.includeMaster){
                QDirIterator it(path.masterPath,QDir::Files);
                if(it.hasNext()) numPaths++;
            }
            for(const auto &subPath : path.subPaths){
                if(subPath.second.first){
                    QDirIterator it(subPath.second.second,QDir::Files);
                    if(it.hasNext()) numPaths++;
                }
            }
        }
        matlab::data::CellArray dataPaths_exps = factory.createCellArray({1,numPaths});
        size_t currPath = 0;
        for(const auto &path : dPaths){
            if(path.includeMaster){
                QDirIterator it(path.masterPath,QDir::Files);
                if(it.hasNext()){
                    dataPaths_exps[currPath] = factory.createCharArray(path.masterPath.toStdString());
                    currPath++;
                }
                else std::cout << "WARNING: Data Path: " << path.masterPath.toStdString() << " not included because it contains no files. Continuing." << std::endl;
            }
            for(const auto &subPath : path.subPaths){
                if(subPath.second.first){
                    QDirIterator it(subPath.second.second,QDir::Files);
                    if(it.hasNext()){
                        dataPaths_exps[currPath] = factory.createCharArray(subPath.second.second.toStdString());
                        currPath++;
                    }
                    else std::cout << "WARNING: Data Path: " << subPath.second.second.toStdString() << " not included because it contains no files. Continuing." << std::endl;
                }
            }
        }
        data.push_back(dataPaths_exps);

        // Main Settings

        // TODO: Logic (in bool array 4th value is all decon, 5th value is rotate after decon)
        // TODO: FIX LOGIC FOR DECON ONLY
        data.push_back(factory.createCharArray("Overwrite"));
        if(ui->deconOnlyCheckBox->isChecked()) data.push_back(factory.createScalar<bool>(false));
        else if(ui->deskewOverwriteDataCheckBox->isChecked() && ui->rotateOverwriteDataCheckBox->isChecked() && ui->deskewAndRotateOverwriteDataCheckBox->isChecked() && ui->stitchOverwriteDataCheckBox->isChecked()) data.push_back(factory.createScalar<bool>(true));
        else data.push_back(factory.createArray<bool>({1,5},{ui->deskewOverwriteDataCheckBox->isChecked(),ui->rotateOverwriteDataCheckBox->isChecked(),ui->stitchOverwriteDataCheckBox->isChecked(),false,false}));

        // Channel Patterns
        data.push_back(factory.createCharArray("ChannelPatterns"));
        if(!ui->customPatternsCheckBox->isChecked()){
            if(channelWidgets.size()){
                // Grab indexes of checked boxes
                std::vector<int> indexes;
                for(size_t i = 0; i < channelWidgets.size(); i++){
                    if(channelWidgets[i].second->isChecked()) indexes.push_back(i);
                }
                matlab::data::CellArray channelPatterns = factory.createCellArray({1,indexes.size()});
                int cpi = 0;
                // Go through checked indexes and the label text (channel pattern) in the cell array
                for(int i : indexes){

                    // Convert from rich text to plain text
                    QTextDocument toPlain;
                    toPlain.setHtml(channelWidgets[i].first->text());

                    channelPatterns[cpi] = factory.createCharArray(toPlain.toPlainText().toStdString());
                    cpi++;
                }
                data.push_back(channelPatterns);
            }
        }
        // Use custom patterns
        else{
            QString patternLine = ui->customPatternsLineEdit->text();
            QString pattern;
            std::vector<QString> patterns;
            for(int i = 0; i < patternLine.size(); i++){
                if(patternLine[i] == ','){
                    patterns.push_back(pattern);
                    pattern.clear();
                }
                else{
                    pattern.push_back(patternLine[i]);
                }
            }
            if(pattern.size()) patterns.push_back(pattern);

            matlab::data::CellArray channelPatterns = factory.createCellArray({1,patterns.size()});

            for(size_t i = 0; i < patterns.size(); i++){
                channelPatterns[i] = factory.createCharArray(patterns[i].toStdString());
            }
            data.push_back(channelPatterns);
        }
        // Currently not used
        //data.push_back(factory.createCharArray("Channels"));
        //data.push_back(factory.createArray<uint64_t>({1,3},{488,560,642}));

        data.push_back(factory.createCharArray("SkewAngle"));
        data.push_back(factory.createScalar<double>(guiVals.skewAngle));

        data.push_back(factory.createCharArray("dz"));
        data.push_back(factory.createScalar<double>(dz.toDouble()));

        data.push_back(factory.createCharArray("xyPixelSize"));
        data.push_back(factory.createScalar<double>(guiVals.xyPixelSize));

        data.push_back(factory.createCharArray("Reverse"));
        data.push_back(factory.createScalar<bool>(guiVals.Reverse));

        data.push_back(factory.createCharArray("ObjectiveScan"));
        data.push_back(factory.createScalar<bool>(ui->objectiveScanCheckBox->isChecked()));

        data.push_back(factory.createCharArray("sCMOSCameraFlip"));
        data.push_back(factory.createScalar<bool>(guiVals.sCMOSCameraFlip));

        // This needs to change FIX
        // TODO: FIX LOGIC FOR DECON ONLY
        data.push_back(factory.createCharArray("Save16bit"));
        if (ui->deconOnlyCheckBox->isChecked()) data.push_back(factory.createScalar<bool>(false));
        else data.push_back(factory.createArray<bool>({1,4},{ui->deskewSave16BitCheckBox->isChecked() || ui->rotateSave16BitCheckBox->isChecked() || ui->deskewAndRotateSave16BitCheckBox->isChecked(),ui->stitchSave16BitCheckBox->isChecked(),false,false}));

        // This needs to change FIX
        data.push_back(factory.createCharArray("onlyFirstTP"));
        data.push_back(factory.createScalar<bool>(ui->deskewOnlyFirstTPCheckBox->isChecked() || ui->rotateOnlyFirstTPCheckBox->isChecked() || ui->deskewAndRotateOnlyFirstTPCheckBox->isChecked() || ui->stitchOnlyFirstTPCheckBox->isChecked()));


        // Pipeline Setting

        data.push_back(factory.createCharArray("Decon"));
        data.push_back(factory.createScalar<bool>(ui->deskewDeconCheckBox->isChecked() || ui->rotateDeconCheckBox->isChecked() || ui->deskewAndRotateDeconCheckBox->isChecked() || ui->stitchDeconCheckBox->isChecked() || ui->deconOnlyCheckBox->isChecked()));

        // Change later
        //data.push_back(factory.createCharArray("RotateAfterDecon"));
        //data.push_back(factory.createScalar<bool>(ui->rotateAfterDeconCheckBox->isChecked()));

        // Decon Settings

        data.push_back(factory.createCharArray("cudaDecon"));
        data.push_back(factory.createScalar<bool>(ui->cudaDeconRadioButton->isChecked()));

        data.push_back(factory.createCharArray("cppDecon"));
        data.push_back(factory.createScalar<bool>(ui->cppDeconRadioButton->isChecked()));

        data.push_back(factory.createCharArray("Background"));
        data.push_back(factory.createScalar<uint64_t>(ui->backgroundIntensityLineEdit->text().toULongLong()));

        data.push_back(factory.createCharArray("dzPSF"));
        data.push_back(factory.createScalar<double>(ui->dzPSFLineEdit->text().toDouble()));

        data.push_back(factory.createCharArray("EdgeErosion"));
        data.push_back(factory.createScalar<uint64_t>(ui->edgeErosionLineEdit->text().toULongLong()));

        data.push_back(factory.createCharArray("ErodeByFTP"));
        data.push_back(factory.createScalar<bool>(ui->erodeByFTPCheckBox->isChecked()));

        data.push_back(factory.createCharArray("deconRotate"));
        data.push_back(factory.createScalar<bool>(ui->deconRotateCheckBox->isChecked()));

        // Decon Advanced settings

        if(!guiVals.cppDeconPath.isEmpty()){
            data.push_back(factory.createCharArray("cppDeconPath"));
            data.push_back(factory.createCharArray(guiVals.cppDeconPath.toStdString()));
        }
        if(!guiVals.loadModules.isEmpty()){
            data.push_back(factory.createCharArray("loadModules"));
            data.push_back(factory.createCharArray(guiVals.loadModules.toStdString()));
        }
        if(!guiVals.cudaDeconPath.isEmpty()){
            data.push_back(factory.createCharArray("cudaDeconPath"));
            data.push_back(factory.createCharArray(guiVals.cudaDeconPath.toStdString()));
        }
        if(!guiVals.OTFGENPath.isEmpty()){
            data.push_back(factory.createCharArray("OTFGENPath"));
            data.push_back(factory.createCharArray(guiVals.OTFGENPath.toStdString()));
        }

        if(psfFullPaths.size()){
            data.push_back(factory.createCharArray("psfFullpaths"));
            matlab::data::CellArray psfMPaths = factory.createCellArray({1,psfFullPaths.size()});
            for(size_t i = 0; i < psfFullPaths.size(); i++){
                psfMPaths[i] = factory.createCharArray(psfFullPaths[i].toStdString());
            }
            data.push_back(psfMPaths);
        }
        data.push_back(factory.createCharArray("RLMethod"));
        data.push_back(factory.createCharArray(guiVals.RLMethod.toStdString()));

        //----TESTING ERR THRESH VALS------

        data.push_back(factory.createCharArray("fixIter"));
        data.push_back(factory.createScalar<bool>(guiVals.fixIter));

        data.push_back(factory.createCharArray("errThresh"));
        data.push_back(factory.createScalar<double>(guiVals.errThresh));

        data.push_back(factory.createCharArray("debug"));
        data.push_back(factory.createScalar<bool>(guiVals.debug));

        data.push_back(factory.createCharArray("GPUJob"));
        data.push_back(factory.createScalar<bool>(guiVals.gpuJob));

        data.push_back(factory.createCharArray("DeconIter"));
        data.push_back(factory.createScalar<uint64_t>(ui->deconIterationsLineEdit->text().toULongLong()));

        data.push_back(factory.createCharArray("rotatePSF"));
        data.push_back(factory.createScalar<bool>(ui->rotatePSFCheckBox->isChecked()));

        // Job Settings

        data.push_back(factory.createCharArray("parseCluster"));
        data.push_back(factory.createScalar<bool>(ui->parseClusterCheckBox->isChecked()));

        data.push_back(factory.createCharArray("cpusPerTask"));
        data.push_back(factory.createScalar<uint64_t>(ui->cpusPerTaskLineEdit->text().toULongLong()));

        data.push_back(factory.createCharArray("cpuOnlyNodes"));
        data.push_back(factory.createScalar<bool>(ui->cpuOnlyNodesCheckBox->isChecked()));

        // Advanced Job Settings

        data.push_back(factory.createCharArray("largeFile"));
        data.push_back(factory.createScalar<bool>(guiVals.largeFile));

        if(!guiVals.jobLogDir.isEmpty()){
            data.push_back(factory.createCharArray("jobLogDir"));
            data.push_back(factory.createCharArray(guiVals.jobLogDir.toStdString()));
        }

        if(!guiVals.uuid.isEmpty()){
            data.push_back(factory.createCharArray("uuid"));
            data.push_back(factory.createCharArray(guiVals.uuid.toStdString()));
        }

        data.push_back(factory.createCharArray("maxTrialNum"));
        data.push_back(factory.createScalar<uint64_t>(guiVals.maxTrialNum));

        data.push_back(factory.createCharArray("unitWaitTime"));
        data.push_back(factory.createScalar<uint64_t>(guiVals.unitWaitTime));

        data.push_back(factory.createCharArray("maxWaitLoopNum"));
        data.push_back(factory.createScalar<uint64_t>(guiVals.maxWaitLoopNum));

        if(!guiVals.MatlabLaunchStr.isEmpty()){
            data.push_back(factory.createCharArray("MatlabLaunchStr"));
            data.push_back(factory.createCharArray(guiVals.MatlabLaunchStr.toStdString()));
        }

        if(!guiVals.SlurmParam.isEmpty()){
            data.push_back(factory.createCharArray("SlurmParam"));
            data.push_back(factory.createCharArray(guiVals.SlurmParam.toStdString()));
        }
    }
    else{

        // Data Paths
        unsigned long long numPaths = 0;
        for(const auto &path : dPaths){
            if(path.includeMaster){
                QDirIterator it(path.masterPath,QDir::Files);
                if(it.hasNext()) numPaths++;
            }
            for(const auto &subPath : path.subPaths){
                if(subPath.second.first){
                    QDirIterator it(subPath.second.second,QDir::Files);
                    if(it.hasNext()) numPaths++;
                }
            }
        }
        matlab::data::CellArray dataPaths_exps = factory.createCellArray({1,numPaths});
        size_t currPath = 0;
        for(const auto &path : dPaths){
            if(path.includeMaster){
                QDirIterator it(path.masterPath,QDir::Files);
                if(it.hasNext()){
                    dataPaths_exps[currPath] = factory.createCharArray(path.masterPath.toStdString());
                    currPath++;
                }
                else std::cout << "WARNING: Data Path: " << path.masterPath.toStdString() << " not included because it contains no files. Continuing." << std::endl;
            }
            for(const auto &subPath : path.subPaths){
                if(subPath.second.first){
                    QDirIterator it(subPath.second.second,QDir::Files);
                    if(it.hasNext()){
                        dataPaths_exps[currPath] = factory.createCharArray(subPath.second.second.toStdString());
                        currPath++;
                    }
                    else std::cout << "WARNING: Data Path: " << subPath.second.second.toStdString() << " not included because it contains no files. Continuing." << std::endl;
                }
            }
        }
        data.push_back(dataPaths_exps);

        // Main Settings

        // TODO: Logic (in bool array 4th value is all decon, 5th value is rotate after decon)
        // TODO: FIX LOGIC FOR DECON ONLY
        data.push_back(factory.createCharArray("Overwrite"));
        if(ui->deconOnlyCheckBox->isChecked()) data.push_back(factory.createScalar<bool>(false));
        else if(ui->deskewOverwriteDataCheckBox->isChecked() && ui->rotateOverwriteDataCheckBox->isChecked() && ui->deskewAndRotateOverwriteDataCheckBox->isChecked() && ui->stitchOverwriteDataCheckBox->isChecked()) data.push_back(factory.createScalar<bool>(true));
        else data.push_back(factory.createArray<bool>({1,5},{ui->deskewOverwriteDataCheckBox->isChecked(),ui->rotateOverwriteDataCheckBox->isChecked(),ui->stitchOverwriteDataCheckBox->isChecked(),false,false}));

        data.push_back(factory.createCharArray("Streaming"));
        data.push_back(factory.createScalar<bool>(ui->streamingCheckBox->isChecked()));


        // Channel Patterns
        data.push_back(factory.createCharArray("ChannelPatterns"));
        if(!ui->customPatternsCheckBox->isChecked()){

            if(channelWidgets.size()){
                // Grab indexes of checked boxes
                std::vector<int> indexes;
                for(size_t i = 0; i < channelWidgets.size(); i++){
                    if(channelWidgets[i].second->isChecked()) indexes.push_back(i);
                }
                matlab::data::CellArray channelPatterns = factory.createCellArray({1,indexes.size()});
                int cpi = 0;
                // Go through checked indexes and the label text (channel pattern) in the cell array
                for(int i : indexes){

                    // Convert from rich text to plain text
                    QTextDocument toPlain;
                    toPlain.setHtml(channelWidgets[i].first->text());

                    channelPatterns[cpi] = factory.createCharArray(toPlain.toPlainText().toStdString());
                    cpi++;
                }
                data.push_back(channelPatterns);
            }

        }
        // Use custom patterns
        else{
            QString patternLine = ui->customPatternsLineEdit->text();
            QString pattern;
            std::vector<QString> patterns;
            for(int i = 0; i < patternLine.size(); i++){
                if(patternLine[i] == ','){
                    patterns.push_back(pattern);
                    pattern.clear();
                }
                else{
                    pattern.push_back(patternLine[i]);
                }
            }
            if(pattern.size()) patterns.push_back(pattern);

            matlab::data::CellArray channelPatterns = factory.createCellArray({1,patterns.size()});

            for(size_t i = 0; i < patterns.size(); i++){
                channelPatterns[i] = factory.createCharArray(patterns[i].toStdString());
            }
            data.push_back(channelPatterns);

        }
        // Currently not used
        //data.push_back(factory.createCharArray("Channels"));
        //data.push_back(factory.createArray<uint64_t>({1,3},{488,560,642}));

        data.push_back(factory.createCharArray("SkewAngle"));
        data.push_back(factory.createScalar<double>(guiVals.skewAngle));

        data.push_back(factory.createCharArray("dz"));
        data.push_back(factory.createScalar<double>(dz.toDouble()));

        data.push_back(factory.createCharArray("xyPixelSize"));
        data.push_back(factory.createScalar<double>(guiVals.xyPixelSize));

        data.push_back(factory.createCharArray("Reverse"));
        data.push_back(factory.createScalar<bool>(guiVals.Reverse));

        data.push_back(factory.createCharArray("ObjectiveScan"));
        data.push_back(factory.createScalar<bool>(ui->objectiveScanCheckBox->isChecked()));

        data.push_back(factory.createCharArray("sCMOSCameraFlip"));
        data.push_back(factory.createScalar<bool>(guiVals.sCMOSCameraFlip));

        // This needs to change FIX
        // TODO: FIX LOGIC FOR DECON ONLY
        data.push_back(factory.createCharArray("Save16bit"));
        if (ui->deconOnlyCheckBox->isChecked()) data.push_back(factory.createScalar<bool>(false));
        else data.push_back(factory.createArray<bool>({1,4},{ui->deskewSave16BitCheckBox->isChecked() || ui->rotateSave16BitCheckBox->isChecked() || ui->deskewAndRotateSave16BitCheckBox->isChecked(),ui->stitchSave16BitCheckBox->isChecked(),false,false}));

        // This needs to change FIX
        data.push_back(factory.createCharArray("onlyFirstTP"));
        data.push_back(factory.createScalar<bool>(ui->deskewOnlyFirstTPCheckBox->isChecked() || ui->rotateOnlyFirstTPCheckBox->isChecked() || ui->deskewAndRotateOnlyFirstTPCheckBox->isChecked() || ui->stitchOnlyFirstTPCheckBox->isChecked()));


        // Pipeline Settings

        //
        // TODO: Test this Logic more
        if(!(ui->deskewCheckBox->isChecked() || ui->rotateCheckBox->isChecked()) && ui->deskewAndRotateCheckBox->isChecked()){
            data.push_back(factory.createCharArray("DSRCombined"));
            data.push_back(factory.createScalar<bool>(true));

            data.push_back(factory.createCharArray("Deskew"));
            data.push_back(factory.createScalar<bool>(true));

            data.push_back(factory.createCharArray("Rotate"));
            data.push_back(factory.createScalar<bool>(true));

        }
        else{
            data.push_back(factory.createCharArray("DSRCombined"));
            data.push_back(factory.createScalar<bool>(false));

            data.push_back(factory.createCharArray("Deskew"));
            data.push_back(factory.createScalar<bool>(ui->deskewCheckBox->isChecked()));

            data.push_back(factory.createCharArray("Rotate"));
            data.push_back(factory.createScalar<bool>(ui->rotateCheckBox->isChecked()));
        }

        data.push_back(factory.createCharArray("Stitch"));
        data.push_back(factory.createScalar<bool>(ui->stitchCheckBox->isChecked()));

        data.push_back(factory.createCharArray("Decon"));
        data.push_back(factory.createScalar<bool>(ui->deskewDeconCheckBox->isChecked() || ui->rotateDeconCheckBox->isChecked() || ui->deskewAndRotateDeconCheckBox->isChecked() || ui->stitchDeconCheckBox->isChecked() || ui->deconOnlyCheckBox->isChecked()));

        // Change later
        //data.push_back(factory.createCharArray("RotateAfterDecon"));
        //data.push_back(factory.createScalar<bool>(ui->rotateAfterDeconCheckBox->isChecked()));


        // DSR Settings

        data.push_back(factory.createCharArray("parseSettingFile"));
        data.push_back(factory.createScalar<bool>(ui->parseSettingsFileCheckBox->isChecked()));

        data.push_back(factory.createCharArray("flipZstack"));
        data.push_back(factory.createScalar<bool>(ui->flipZStackCheckBox->isChecked()));

        data.push_back(factory.createCharArray("LLFFCorrection"));
        data.push_back(factory.createScalar<bool>(ui->llffCorrectionCheckBox->isChecked()));

        if(lsImagePaths.size()){
            data.push_back(factory.createCharArray("LSImagePaths"));
            matlab::data::CellArray lsImageMPaths = factory.createCellArray({1,lsImagePaths.size()});
            for(size_t i = 0; i < lsImagePaths.size(); i++){
                lsImageMPaths[i] = factory.createCharArray(lsImagePaths[i].toStdString());
            }
            data.push_back(lsImageMPaths);
        }

        if(backgroundPaths.size()){
            data.push_back(factory.createCharArray("BackgroundPaths"));
            matlab::data::CellArray backgroundMPaths = factory.createCellArray({1,backgroundPaths.size()});
            for(size_t i = 0; i < backgroundPaths.size(); i++){
                backgroundMPaths[i] = factory.createCharArray(backgroundPaths[i].toStdString());
            }
            data.push_back(backgroundMPaths);
        }

        // DSR Advanced Settings

        data.push_back(factory.createCharArray("BKRemoval"));
        data.push_back(factory.createScalar<bool>(guiVals.BKRemoval));

        data.push_back(factory.createCharArray("LowerLimit"));
        data.push_back(factory.createScalar<double>(guiVals.LowerLimit));

        // TODO: Update Resample
        data.push_back(factory.createCharArray("resampleType"));
        data.push_back(factory.createCharArray(guiVals.resampleType.toStdString()));

        if(guiVals.resample){
            data.push_back(factory.createCharArray("resample"));
            data.push_back(factory.createScalar<uint64_t>(guiVals.resample));
        }

        // Stitch Settings

        data.push_back(factory.createCharArray("stitchPipeline"));
        data.push_back(factory.createCharArray(ui->stitchPipelineComboBox->currentText().toStdString()));

        data.push_back(factory.createCharArray("stitchResultDir"));
        data.push_back(factory.createCharArray(ui->resultsDirLineEdit->text().toStdString()));

        data.push_back(factory.createCharArray("imageListFullpaths"));
        data.push_back(factory.createCharArray(ui->imageListFullPathsLineEdit->text().toStdString()));

        if(!ui->axisOrderLineEdit->text().isEmpty()){
            data.push_back(factory.createCharArray("axisOrder"));
            data.push_back(factory.createCharArray(ui->axisOrderLineEdit->text().toStdString()));
        }

        data.push_back(factory.createCharArray("BlendMethod"));
        data.push_back(factory.createCharArray(ui->blendMethodComboBox->currentText().toStdString()));

        data.push_back(factory.createCharArray("xcorrShift"));
        data.push_back(factory.createScalar<bool>(ui->xCorrShiftCheckBox->isChecked()));

        data.push_back(factory.createCharArray("xcorrMode"));
        data.push_back(factory.createCharArray(ui->xCorrModeComboBox->currentText().toStdString()));

        // Bound box crop
        if(ui->boundBoxCheckBox->isChecked()){
            data.push_back(factory.createArray<int>({1,6},{ui->boundBoxYMinSpinBox->value(),ui->boundBoxXMinSpinBox->value(),ui->boundBoxZMinSpinBox->value(),ui->boundBoxYMaxSpinBox->value(), ui->boundBoxXMaxSpinBox->value(), ui->boundBoxZMaxSpinBox->value()}));
        }

        //EX: "camA_ch0";
        if(!ui->primaryCHLineEdit->text().isEmpty()){
            data.push_back(factory.createCharArray("primaryCh"));
            data.push_back(factory.createCharArray(ui->primaryCHLineEdit->text().toStdString()));
        }

        // Decon Settings

        data.push_back(factory.createCharArray("cudaDecon"));
        data.push_back(factory.createScalar<bool>(ui->cudaDeconRadioButton->isChecked()));

        data.push_back(factory.createCharArray("cppDecon"));
        data.push_back(factory.createScalar<bool>(ui->cppDeconRadioButton->isChecked()));

        data.push_back(factory.createCharArray("DS"));
        data.push_back(factory.createScalar<bool>(ui->deskewDeconCheckBox->isChecked()));

        data.push_back(factory.createCharArray("DSR"));
        data.push_back(factory.createScalar<bool>(ui->deskewAndRotateDeconCheckBox->isChecked()));

        data.push_back(factory.createCharArray("Background"));
        data.push_back(factory.createScalar<uint64_t>(ui->backgroundIntensityLineEdit->text().toULongLong()));

        data.push_back(factory.createCharArray("dzPSF"));
        data.push_back(factory.createScalar<double>(ui->dzPSFLineEdit->text().toDouble()));

        data.push_back(factory.createCharArray("EdgeErosion"));
        data.push_back(factory.createScalar<uint64_t>(ui->edgeErosionLineEdit->text().toULongLong()));

        data.push_back(factory.createCharArray("ErodeByFTP"));
        data.push_back(factory.createScalar<bool>(ui->erodeByFTPCheckBox->isChecked()));

        data.push_back(factory.createCharArray("deconRotate"));
        data.push_back(factory.createScalar<bool>(ui->deconRotateCheckBox->isChecked()));

        // Decon Advanced settings

        if(!guiVals.cppDeconPath.isEmpty()){
            data.push_back(factory.createCharArray("cppDeconPath"));
            data.push_back(factory.createCharArray(guiVals.cppDeconPath.toStdString()));
        }
        if(!guiVals.loadModules.isEmpty()){
            data.push_back(factory.createCharArray("loadModules"));
            data.push_back(factory.createCharArray(guiVals.loadModules.toStdString()));
        }
        if(!guiVals.cudaDeconPath.isEmpty()){
            data.push_back(factory.createCharArray("cudaDeconPath"));
            data.push_back(factory.createCharArray(guiVals.cudaDeconPath.toStdString()));
        }
        if(!guiVals.OTFGENPath.isEmpty()){
            data.push_back(factory.createCharArray("OTFGENPath"));
            data.push_back(factory.createCharArray(guiVals.OTFGENPath.toStdString()));
        }

        if(psfFullPaths.size()){
            data.push_back(factory.createCharArray("psfFullpaths"));
            matlab::data::CellArray psfMPaths = factory.createCellArray({1,psfFullPaths.size()});
            for(size_t i = 0; i < psfFullPaths.size(); i++){
                psfMPaths[i] = factory.createCharArray(psfFullPaths[i].toStdString());
            }
            data.push_back(psfMPaths);
        }
        data.push_back(factory.createCharArray("RLMethod"));
        data.push_back(factory.createCharArray(guiVals.RLMethod.toStdString()));

        data.push_back(factory.createCharArray("fixIter"));
        data.push_back(factory.createScalar<bool>(guiVals.fixIter));

        data.push_back(factory.createCharArray("errThresh"));
        data.push_back(factory.createScalar<double>(guiVals.errThresh));

        data.push_back(factory.createCharArray("debug"));
        data.push_back(factory.createScalar<bool>(guiVals.debug));

        data.push_back(factory.createCharArray("GPUJob"));
        data.push_back(factory.createScalar<bool>(guiVals.gpuJob));

        data.push_back(factory.createCharArray("DeconIter"));
        data.push_back(factory.createScalar<uint64_t>(ui->deconIterationsLineEdit->text().toULongLong()));

        data.push_back(factory.createCharArray("rotatedPSF"));
        data.push_back(factory.createScalar<bool>(ui->rotatePSFCheckBox->isChecked()));

        // Job Settings

        data.push_back(factory.createCharArray("parseCluster"));
        data.push_back(factory.createScalar<bool>(ui->parseClusterCheckBox->isChecked()));

        data.push_back(factory.createCharArray("cpusPerTask"));
        data.push_back(factory.createScalar<uint64_t>(ui->cpusPerTaskLineEdit->text().toULongLong()));

        data.push_back(factory.createCharArray("cpuOnlyNodes"));
        data.push_back(factory.createScalar<bool>(ui->cpuOnlyNodesCheckBox->isChecked()));

        // Advanced Job Settings

        data.push_back(factory.createCharArray("largeFile"));
        data.push_back(factory.createScalar<bool>(guiVals.largeFile));

        if(!guiVals.jobLogDir.isEmpty()){
            data.push_back(factory.createCharArray("jobLogDir"));
            data.push_back(factory.createCharArray(guiVals.jobLogDir.toStdString()));
        }

        if(!guiVals.uuid.isEmpty()){
            data.push_back(factory.createCharArray("uuid"));
            data.push_back(factory.createCharArray(guiVals.uuid.toStdString()));
        }

        data.push_back(factory.createCharArray("maxTrialNum"));
        data.push_back(factory.createScalar<uint64_t>(guiVals.maxTrialNum));

        data.push_back(factory.createCharArray("unitWaitTime"));
        data.push_back(factory.createScalar<uint64_t>(guiVals.unitWaitTime));

        data.push_back(factory.createCharArray("minModifyTime"));
        data.push_back(factory.createScalar<uint64_t>(guiVals.minModifyTime));

        data.push_back(factory.createCharArray("maxModifyTime"));
        data.push_back(factory.createScalar<uint64_t>(guiVals.maxModifyTime));

        data.push_back(factory.createCharArray("maxWaitLoopNum"));
        data.push_back(factory.createScalar<uint64_t>(guiVals.maxWaitLoopNum));

        if(!guiVals.MatlabLaunchStr.isEmpty()){
            data.push_back(factory.createCharArray("MatlabLaunchStr"));
            data.push_back(factory.createCharArray(guiVals.MatlabLaunchStr.toStdString()));
        }

        if(!guiVals.SlurmParam.isEmpty()){
            data.push_back(factory.createCharArray("SlurmParam"));
            data.push_back(factory.createCharArray(guiVals.SlurmParam.toStdString()));
        }
    }
    QString funcType;

    if(ui->deconOnlyCheckBox->isChecked()){
        funcType="DeconOnly";
    }

    auto mPJNPC = std::make_tuple(mainPath, timeJobName,ui->parseClusterCheckBox->isChecked());
    // Send data to the MATLAB thread
    //emit jobStart(outA, data, funcType, mPJNPC);

    // Still deciding which name I want to show to the user
    size_t currJob = jobNames.size()+1;
    jobNames.emplace(currJob,timeJobName);

    QString currJobText = ui->jobNameLineEdit->text();
    if(currJobText.contains("Job ") && currJobText.back().isDigit()){
        for(int i = currJobText.size()-1; i >= 0; i--){
            if(currJobText.back().isDigit()){
                currJobText.chop(1);
            }
            else{
                currJobText.append(QString::number(currJob+1));
                ui->jobNameLineEdit->setText(currJobText);
                break;
            }
        }
    }

    // Reset jobLogDir
    guiVals.jobLogDir = jobLogCopy;

    return matlabJobSettings(outA,data,funcType,mPJNPC);
}

// Browse for Result Dir Folder
void MainWindow::on_resultsDirBrowseButton_clicked()
{
    QFileInfo folder_path = QFileDialog::getExistingDirectory(this,"Select or Create and Select the Results Folder",mostRecentDir);
    if(!folder_path.baseName().isEmpty()){
        ui->resultsDirLineEdit->setText(folder_path.baseName());
        mostRecentDir = folder_path.absoluteFilePath();
    }
}

// Set tool tip for Stitch Result Dir line
void MainWindow::on_resultsDirLineEdit_textChanged(const QString &arg1)
{
    ui->resultsDirLineEdit->setToolTip(arg1);
}


// Open Main Window Advanced Settings
void MainWindow::on_mainAdvancedSettingsButton_clicked()
{
    mainAdvanced mAdvanced(guiVals);
    mAdvanced.setModal(true);
    mAdvanced.exec();
}

// If Deskew is checked, enable first row and Rotate checkbox
// If Deskew is unchecked, disable first row and Rotate checkbox and remove all their current checks
void MainWindow::on_deskewCheckBox_stateChanged(int arg1)
{
    if(arg1){
        ui->deskewDeconCheckBox->setEnabled(true);
        ui->deskewOverwriteDataCheckBox->setEnabled(true);
        ui->deskewSave16BitCheckBox->setEnabled(true);
        ui->deskewOnlyFirstTPCheckBox->setEnabled(true);

        // Enable the Next Button
        ui->mainNextButton->setEnabled(true);
    }
    else{
        ui->deskewDeconCheckBox->setEnabled(false);
        ui->deskewOverwriteDataCheckBox->setEnabled(false);
        ui->deskewSave16BitCheckBox->setEnabled(false);
        ui->deskewOnlyFirstTPCheckBox->setEnabled(false);

        ui->deskewDeconCheckBox->setChecked(false);
        ui->deskewOverwriteDataCheckBox->setChecked(false);
        ui->deskewSave16BitCheckBox->setChecked(false);
        ui->deskewOnlyFirstTPCheckBox->setChecked(false);

        // If none of the other main boxes are checked then disable the next button
        if(!(ui->stitchCheckBox->isChecked() || ui->rotateCheckBox->isChecked() || ui->deskewAndRotateCheckBox->isChecked() || ui->deconOnlyCheckBox->isChecked())){
            ui->mainNextButton->setEnabled(false);
        }
    }
}

// If Rotate is checked, enable second row
// If Rotate is unchecked, disable second row and remove all their current checks
void MainWindow::on_rotateCheckBox_stateChanged(int arg1)
{
    if(arg1){
        ui->rotateDeconCheckBox->setEnabled(true);
        ui->rotateOverwriteDataCheckBox->setEnabled(true);
        ui->rotateSave16BitCheckBox->setEnabled(true);
        ui->rotateOnlyFirstTPCheckBox->setEnabled(true);

        // Enable the Next Button
        ui->mainNextButton->setEnabled(true);

    }
    else{
        ui->rotateDeconCheckBox->setEnabled(false);
        ui->rotateOverwriteDataCheckBox->setEnabled(false);
        ui->rotateSave16BitCheckBox->setEnabled(false);
        ui->rotateOnlyFirstTPCheckBox->setEnabled(false);

        ui->rotateDeconCheckBox->setChecked(false);
        ui->rotateOverwriteDataCheckBox->setChecked(false);
        ui->rotateSave16BitCheckBox->setChecked(false);
        ui->rotateOnlyFirstTPCheckBox->setChecked(false);

        // If none of the other main boxes are checked then disable the next button
        if(!(ui->stitchCheckBox->isChecked() || ui->deskewCheckBox->isChecked() || ui->deskewAndRotateCheckBox->isChecked() || ui->deconOnlyCheckBox->isChecked())){
            ui->mainNextButton->setEnabled(false);
        }
    }

}

// If Deskew and Rotate is checked, enable third row
// If Deskew and Rotate is unchecked, disable third row and remove all their current checks
void MainWindow::on_deskewAndRotateCheckBox_stateChanged(int arg1)
{
    if(arg1){
        ui->deskewAndRotateDeconCheckBox->setEnabled(true);
        ui->deskewAndRotateOverwriteDataCheckBox->setEnabled(true);
        ui->deskewAndRotateSave16BitCheckBox->setEnabled(true);
        ui->deskewAndRotateOnlyFirstTPCheckBox->setEnabled(true);

        // Enable the Next Button
        ui->mainNextButton->setEnabled(true);

    }
    else{
        ui->deskewAndRotateDeconCheckBox->setEnabled(false);
        ui->deskewAndRotateOverwriteDataCheckBox->setEnabled(false);
        ui->deskewAndRotateSave16BitCheckBox->setEnabled(false);
        ui->deskewAndRotateOnlyFirstTPCheckBox->setEnabled(false);

        ui->deskewAndRotateDeconCheckBox->setChecked(false);
        ui->deskewAndRotateOverwriteDataCheckBox->setChecked(false);
        ui->deskewAndRotateSave16BitCheckBox->setChecked(false);
        ui->deskewAndRotateOnlyFirstTPCheckBox->setChecked(false);

        // If none of the other main boxes are checked then disable the next button
        if(!(ui->stitchCheckBox->isChecked() || ui->deskewCheckBox->isChecked() || ui->rotateCheckBox->isChecked() || ui->deconOnlyCheckBox->isChecked())){
            ui->mainNextButton->setEnabled(false);
        }
    }
}

// If Stitch is checked, enable fourth row
// If Stitch is unchecked, disable fourth row and remove all their current checks
void MainWindow::on_stitchCheckBox_stateChanged(int arg1)
{
    if(arg1){
        ui->stitchDeconCheckBox->setEnabled(true);
        ui->stitchOverwriteDataCheckBox->setEnabled(true);
        ui->stitchSave16BitCheckBox->setEnabled(true);
        ui->stitchOnlyFirstTPCheckBox->setEnabled(true);

        // Enable the Next Button
        ui->mainNextButton->setEnabled(true);
    }
    else{
        ui->stitchDeconCheckBox->setEnabled(false);
        ui->stitchOverwriteDataCheckBox->setEnabled(false);
        ui->stitchSave16BitCheckBox->setEnabled(false);
        ui->stitchOnlyFirstTPCheckBox->setEnabled(false);

        ui->stitchDeconCheckBox->setChecked(false);
        ui->stitchOverwriteDataCheckBox->setChecked(false);
        ui->stitchSave16BitCheckBox->setChecked(false);
        ui->stitchOnlyFirstTPCheckBox->setChecked(false);

        // If none of the other main boxes are checked then disable the next button
        if(!(ui->deskewCheckBox->isChecked() || ui->rotateCheckBox->isChecked() || ui->deskewAndRotateCheckBox->isChecked() || ui->deconOnlyCheckBox->isChecked())){
            ui->mainNextButton->setEnabled(false);
        }
    }
}

// Functionality for the Next button on the main page
void MainWindow::on_mainNextButton_clicked()
{
    // Error if no data paths set
    if(!dPaths.size()){
        QMessageBox messageBox;
        messageBox.warning(0,"Error","No data paths are set. Please set at least one data path before continuing.");
        messageBox.setFixedSize(500,200);
        return;
    }

    // Error if no channel patterns set
    if(!channelWidgets.size() && (!ui->customPatternsCheckBox->isChecked() || ui->customPatternsLineEdit->text().isEmpty())){
        QMessageBox messageBox;
        messageBox.warning(0,"Error","No channel patterns set. Please set at least one pattern before continuing.");
        messageBox.setFixedSize(500,200);
        return;
    }
    for(size_t i = 0; i < channelWidgets.size(); i++){
        if(channelWidgets[i].second->isChecked()) break;
        if(i == channelWidgets.size()-1 && (!ui->customPatternsCheckBox->isChecked() || ui->customPatternsLineEdit->text().isEmpty())){
            QMessageBox messageBox;
            messageBox.warning(0,"Error","No channel patterns set. Please set at least one pattern before continuing.");
            messageBox.setFixedSize(500,200);
            return;
        }
    }

    // Set the checkmark and disable the tab
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->Main),QString::fromStdString("Main ✔"));
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->Main),false);

    // Logic of which tab is next
    if(ui->deskewCheckBox->isChecked() || ui->rotateCheckBox->isChecked() || ui->deskewAndRotateCheckBox->isChecked()){
        ui->tabWidget->setCurrentWidget(ui->DSR);
    }
    else if (ui->deconOnlyCheckBox->isChecked()){
        ui->tabWidget->setCurrentWidget(ui->Decon);
    }
    else{
        ui->tabWidget->setCurrentWidget(ui->Stitch);
    }

}

// Functionality for the Previous button on the dsr page
void MainWindow::on_dsrPreviousButton_clicked()
{
    // Enable the main tab and disable dsr tab
    ui->tabWidget->setCurrentWidget(ui->Main);
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->DSR),false);
}

// Functionality for the Next button on the dsr page
void MainWindow::on_dsrNextButton_clicked()
{
    // Set the checkmark and disable the tab
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->DSR),QString::fromStdString("DSR ✔"));
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->DSR),false);

    // Logic of which tab is next
    if(ui->stitchCheckBox->isChecked()){
        ui->tabWidget->setCurrentWidget(ui->Stitch);
    }
    else if(ui->deskewDeconCheckBox->isChecked() || ui->rotateDeconCheckBox->isChecked() || ui->deskewAndRotateDeconCheckBox->isChecked() || ui->deconOnlyCheckBox->isChecked()){
        ui->tabWidget->setCurrentWidget(ui->Decon);
    }
    else{
        ui->tabWidget->setCurrentWidget(ui->Job);
    }
}

// When going to a new tab, enable it and remove checkmark if it has one
void MainWindow::on_tabWidget_currentChanged(int index)
{
    ui->tabWidget->setTabEnabled(index, true);

    if(ui->tabWidget->tabText(index).contains(' ')){
        QString temp = ui->tabWidget->tabText(index);
        temp.remove(' ');
        temp.remove("✔");
        ui->tabWidget->setTabText(index,temp);
    }
}

// Disallow Deskew if Objective Scan is true
void MainWindow::on_objectiveScanCheckBox_stateChanged(int arg1)
{
    if(arg1){
        ui->deskewCheckBox->setChecked(false);
        ui->deskewCheckBox->setEnabled(false);
        ui->deskewLabel->setEnabled(false);
    }
    else{
        ui->deskewCheckBox->setEnabled(true);
        ui->deskewLabel->setEnabled(true);
    }
}

// Functionality for the Previous button on the stitch page
void MainWindow::on_stitchPreviousButton_clicked()
{
    // Logic for which tab to go to
    if(ui->deskewCheckBox->isChecked() || ui->rotateCheckBox->isChecked() || ui->deskewAndRotateCheckBox->isChecked()){
        ui->tabWidget->setCurrentWidget(ui->DSR);
    }
    else{
        ui->tabWidget->setCurrentWidget(ui->Main);
    }

    // Disable the stitch tab
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->Stitch),false);
}

// Functionality for the Next button on the stitch page
void MainWindow::on_stitchNextButton_clicked()
{
    // Set the checkmark and disable the tab
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->Stitch),QString::fromStdString("Stitch ✔"));
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->Stitch),false);

    // Logic of which tab is next
    if(ui->deskewDeconCheckBox->isChecked() || ui->rotateDeconCheckBox->isChecked() || ui->deskewAndRotateDeconCheckBox->isChecked() || ui->stitchDeconCheckBox->isChecked() || ui->deconOnlyCheckBox->isChecked()){
        ui->tabWidget->setCurrentWidget(ui->Decon);
    }
    else{
        ui->tabWidget->setCurrentWidget(ui->Job);
    }
}

// Functionality for the Previous button on the Decon page
void MainWindow::on_deconPreviousButton_clicked()
{
    // Logic for which tab to go to
    if(ui->stitchCheckBox->isChecked()){
        ui->tabWidget->setCurrentWidget(ui->Stitch);
    }
    else if((ui->deskewCheckBox->isChecked() || ui->rotateCheckBox->isChecked() || ui->deskewAndRotateCheckBox->isChecked())){
        ui->tabWidget->setCurrentWidget(ui->DSR);
    }
    else{
        ui->tabWidget->setCurrentWidget(ui->Main);
    }

    // Disable the Decon tab
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->Decon),false);
}

// Functionality for the Next button on the Decon page
void MainWindow::on_deconNextButton_clicked()
{
    // Set the checkmark and disable the tab
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->Decon),QString::fromStdString("Decon ✔"));
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->Decon),false);

    // Job is the only possible tab to go to next from decon
    ui->tabWidget->setCurrentWidget(ui->Job);
}

// Functionality for the Previous button on the job page
void MainWindow::on_jobPreviousButton_clicked()
{
    // Logic for which tab to go to
    if(ui->deskewDeconCheckBox->isChecked() || ui->rotateDeconCheckBox->isChecked() || ui->deskewAndRotateDeconCheckBox->isChecked() || ui->stitchDeconCheckBox->isChecked() || ui->deconOnlyCheckBox->isChecked()){
        ui->tabWidget->setCurrentWidget(ui->Decon);
    }
    else if (ui->stitchCheckBox->isChecked()){
        ui->tabWidget->setCurrentWidget(ui->Stitch);
    }
    else{
        ui->tabWidget->setCurrentWidget(ui->DSR);
    }

    // Disable the Job tab
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->Job),false);
}

// Opens a seperate window to add the data paths for the job
void MainWindow::on_addPathsButton_clicked()
{
    // Default behavior is for the main Data Paths
    std::vector<dataPath>* addPathsDataPaths = &dPaths;
    std::vector<std::pair<QLabel*,QCheckBox*>>* addPathsChannelWidgets = &channelWidgets;
    QWidget* addPathsCurrWidget = ui->Main;
    QHBoxLayout* addPathsCurrLayout = ui->horizontalLayout_4;

    // Data Paths for crop
    if(((QPushButton *)sender())->objectName().contains("crop")){
        addPathsDataPaths = &cropDPaths;
        addPathsChannelWidgets = &cropChannelWidgets;
        addPathsCurrWidget = ui->Crop;
        addPathsCurrLayout = ui->cropChannelPatternsHorizontalLayout;
    }


    dataPaths daPaths(*addPathsDataPaths, true, mostRecentDir);
    daPaths.setModal(true);
    daPaths.exec();

    // Find all possible channels
    if(addPathsChannelWidgets->size()){
        for(auto &i : *addPathsChannelWidgets){
            if(i.first) delete i.first;
            if(i.second) delete i.second;
        }
    }
    addPathsChannelWidgets->clear();
    if(!addPathsDataPaths->size()) return;
    std::vector<QString> channels;
    for(const auto &path : *addPathsDataPaths){
        if(path.includeMaster){
            // Looking for channel patterns in the given directory
            QDirIterator cPath(path.masterPath);
            QString c;
            QRegularExpression re("Cam\\w_ch\\d");
            QRegularExpressionMatch rmatch;
            while(cPath.hasNext()){
                c = cPath.next();
                rmatch = re.match(c);

                // Check if there is a match and that it is not already in the vector
                if (!rmatch.captured(0).isEmpty() && !(std::count(channels.begin(),channels.end(),rmatch.captured(0)))) channels.push_back(rmatch.captured(0));


            }
        }
        for(const auto &subPath : path.subPaths){
            if(subPath.second.first){
                // Looking for channel patterns in the given directory
                QDirIterator cPath(subPath.second.second);
                QString c;
                QRegularExpression re("Cam\\w_ch\\d");
                QRegularExpressionMatch rmatch;
                while(cPath.hasNext()){
                    c = cPath.next();
                    rmatch = re.match(c);

                    // Check if there is a match and that it is not already in the vector
                    if (!rmatch.captured(0).isEmpty() && !(std::count(channels.begin(),channels.end(),rmatch.captured(0)))) channels.push_back(rmatch.captured(0));


                }
            }
        }
    }
    for(const QString &ch : channels){

        QLabel* label = new QLabel(addPathsCurrWidget);
        label->setTextFormat(Qt::RichText);
        label->setText("<b>"+ch+"<\b>");
        addPathsCurrLayout->addWidget(label);

        QCheckBox* checkBox = new QCheckBox(addPathsCurrWidget);
        addPathsCurrLayout->addWidget(checkBox);
        addPathsChannelWidgets->push_back(std::make_pair(label,checkBox));
    }


}

void MainWindow::on_imageListFullPathsBrowseButton_clicked()
{
    QFileInfo file_path = QFileDialog::getOpenFileName(this,"Select the Image List Full Paths File",mostRecentDir);
    if(!file_path.absoluteFilePath().isEmpty()){
        ui->imageListFullPathsLineEdit->setText(file_path.absoluteFilePath());
        mostRecentDir = file_path.absolutePath();
    }
}

void MainWindow::on_imageListFullPathsLineEdit_textChanged(const QString &arg1)
{
    ui->imageListFullPathsLineEdit->setToolTip(arg1);
}

// Enable bound box options or disable them
void MainWindow::on_boundBoxCheckBox_stateChanged(int arg1)
{
    if(arg1){
        ui->boundBoxYMinLabel->setEnabled(true);
        ui->boundBoxYMinSpinBox->setEnabled(true);
        ui->boundBoxXMinLabel->setEnabled(true);
        ui->boundBoxXMinSpinBox->setEnabled(true);
        ui->boundBoxZMinLabel->setEnabled(true);
        ui->boundBoxZMinSpinBox->setEnabled(true);
        ui->boundBoxYMaxLabel->setEnabled(true);
        ui->boundBoxYMaxSpinBox->setEnabled(true);
        ui->boundBoxXMaxLabel->setEnabled(true);
        ui->boundBoxXMaxSpinBox->setEnabled(true);
        ui->boundBoxZMaxLabel->setEnabled(true);
        ui->boundBoxZMaxSpinBox->setEnabled(true);
    }
    else{
        ui->boundBoxYMinLabel->setEnabled(false);
        ui->boundBoxYMinSpinBox->setEnabled(false);
        ui->boundBoxXMinLabel->setEnabled(false);
        ui->boundBoxXMinSpinBox->setEnabled(false);
        ui->boundBoxZMinLabel->setEnabled(false);
        ui->boundBoxZMinSpinBox->setEnabled(false);
        ui->boundBoxYMaxLabel->setEnabled(false);
        ui->boundBoxYMaxSpinBox->setEnabled(false);
        ui->boundBoxXMaxLabel->setEnabled(false);
        ui->boundBoxXMaxSpinBox->setEnabled(false);
        ui->boundBoxZMaxLabel->setEnabled(false);
        ui->boundBoxZMaxSpinBox->setEnabled(false);

        ui->boundBoxYMinSpinBox->setValue(0);
        ui->boundBoxXMinSpinBox->setValue(0);
        ui->boundBoxZMinSpinBox->setValue(0);
        ui->boundBoxYMaxSpinBox->setValue(0);
        ui->boundBoxXMaxSpinBox->setValue(0);
        ui->boundBoxZMaxSpinBox->setValue(0);
    }
}

// Enable or Disable settings associated with LLFF
void MainWindow::on_llffCorrectionCheckBox_stateChanged(int arg1)
{
    if(arg1){
        ui->lsImagePathsLabel->setEnabled(true);
        ui->lsImageAddPathsButton->setEnabled(true);
        ui->backgroundPathsLabel->setEnabled(true);
        ui->backgroundAddPathsButton->setEnabled(true);
    }
    else{
        ui->lsImagePathsLabel->setEnabled(false);
        ui->lsImageAddPathsButton->setEnabled(false);
        ui->backgroundPathsLabel->setEnabled(false);
        ui->backgroundAddPathsButton->setEnabled(false);

        lsImagePaths.clear();
        backgroundPaths.clear();
    }
}

// Open window for adding lsImage Paths
void MainWindow::on_lsImageAddPathsButton_clicked()
{

    dataPaths daPaths(lsImagePaths, false, mostRecentDir);
    daPaths.setModal(true);
    daPaths.exec();

}

// Open window for adding background Paths
void MainWindow::on_backgroundAddPathsButton_clicked()
{

    dataPaths daPaths(backgroundPaths, false, mostRecentDir);
    daPaths.setModal(true);
    daPaths.exec();

}

// Open window for adding PSF Paths
void MainWindow::on_psfFullAddPathsButton_2_clicked()
{
    std::vector<QString> channelNames;
    if(!ui->customPatternsCheckBox->isChecked()){
        for(auto i : channelWidgets){
            if(i.second->isChecked()){
                channelNames.push_back(i.first->text());
            }
        }
    }
    else{
        QString patternLine = ui->customPatternsLineEdit->text();
        QString pattern;
        for(int i = 0; i < patternLine.size(); i++){
            if(patternLine[i] == ','){
                channelNames.push_back(pattern);
                pattern.clear();
            }
            else{
                pattern.push_back(patternLine[i]);
            }
        }
        if(pattern.size()){
            channelNames.push_back(pattern);
        }
    }
    dataPaths daPaths(psfFullPaths, false, mostRecentDir, channelNames);
    daPaths.setModal(true);
    daPaths.exec();
}

// Check if the user wants to load previous settings
void MainWindow::checkLoadPrevSettings()
{
    loadSettings = false;
    loadPreviousSettings lPSettings(loadSettings);
    lPSettings.setModal(true);
    lPSettings.exec();
}

// Enable next button if deconOnly is checked
void MainWindow::on_deconOnlyCheckBox_stateChanged(int arg1)
{
    if(arg1){
        ui->mainNextButton->setEnabled(true);
    }
    else{
        if(!(ui->stitchCheckBox->isChecked() || ui->deskewCheckBox->isChecked() || ui->deskewAndRotateCheckBox->isChecked() || ui->rotateCheckBox->isChecked())){
            ui->mainNextButton->setEnabled(false);
        }
    }
}

// Enable or Disable custom pattern writing
void MainWindow::on_customPatternsCheckBox_stateChanged(int arg1)
{
    if(arg1){
        ui->customPatternsLineEdit->setEnabled(true);
    }
    else{
        ui->customPatternsLineEdit->setEnabled(false);
    }
}


void MainWindow::on_cropSubmitButton_clicked()
{
    // Write settings in case of crash
    writeSettings();

    // Disable submit button
    ui->cropSubmitButton->setEnabled(false);

    // We need this to convert C++ vars to MATLAB vars
    matlab::data::ArrayFactory factory;

    // outA is the number of outputs (always zero) and data is the structure to hold the pipeline settings
    size_t outA = 0;
    std::vector<matlab::data::Array> data;

    // NOTE: We have to push a lot of things into our data array one at a time
    // Potentially in the future I can loop through the widgets and do this in fewer lines

    // Set main path. This is where all the output files made by the GUI will be stored.
    QString mainPath = cropDPaths[0].masterPath;



    // Data Path
    data.push_back(factory.createCharArray(mainPath.toStdString()));

    // Result Path
    data.push_back(factory.createCharArray(ui->cropResultPathLineEdit->text().toStdString()));

    // bbox
    data.push_back(factory.createArray<double>({1,6},{static_cast<double>(ui->cropBoundBoxYMinSpinBox->value()),static_cast<double>(ui->cropBoundBoxXMinSpinBox->value()),static_cast<double>(ui->cropBoundBoxZMinSpinBox->value()),static_cast<double>(ui->cropBoundBoxYMaxSpinBox->value()), static_cast<double>(ui->cropBoundBoxXMaxSpinBox->value()), static_cast<double>(ui->cropBoundBoxZMaxSpinBox->value())}));

    // Channel Patterns
    data.push_back(factory.createCharArray("ChannelPatterns"));
    if(!ui->cropCustomPatternsCheckBox->isChecked()){

        if(cropChannelWidgets.size()){
            // Grab indexes of checked boxes
            std::vector<int> indexes;
            for(size_t i = 0; i < cropChannelWidgets.size(); i++){
                if(cropChannelWidgets[i].second->isChecked()) indexes.push_back(i);
            }
            matlab::data::CellArray channelPatterns = factory.createCellArray({1,indexes.size()});
            int cpi = 0;
            // Go through checked indexes and the label text (channel pattern) in the cell array
            for(int i : indexes){

                // Convert from rich text to plain text
                QTextDocument toPlain;
                toPlain.setHtml(cropChannelWidgets[i].first->text());

                channelPatterns[cpi] = factory.createCharArray(toPlain.toPlainText().toStdString());
                cpi++;
            }
            data.push_back(channelPatterns);
        }

    }
    // Use custom patterns
    else{
        QString patternLine = ui->cropCustomPatternsLineEdit->text();
        QString pattern;
        std::vector<QString> patterns;
        for(int i = 0; i < patternLine.size(); i++){
            if(patternLine[i] == ','){
                patterns.push_back(pattern);
                pattern.clear();
            }
            else{
                pattern.push_back(patternLine[i]);
            }
        }
        if(pattern.size()) patterns.push_back(pattern);

        matlab::data::CellArray channelPatterns = factory.createCellArray({1,patterns.size()});

        for(size_t i = 0; i < patterns.size(); i++){
            channelPatterns[i] = factory.createCharArray(patterns[i].toStdString());
        }
        data.push_back(channelPatterns);

    }

    data.push_back(factory.createCharArray("Save16bit"));
    data.push_back(factory.createScalar<bool>(ui->cropSave16BitCheckBox->isChecked()));

    data.push_back(factory.createCharArray("pad"));
    data.push_back(factory.createScalar<bool>(ui->cropPadCheckBox->isChecked()));

    data.push_back(factory.createCharArray("cropType"));
    data.push_back(factory.createCharArray(ui->cropCropTypeComboBox->currentText().toStdString()));

    data.push_back(factory.createCharArray("lastStart"));
    data.push_back(factory.createArray<double>({1,3},{static_cast<double>(ui->cropLastStartYSpinBox->value()),static_cast<double>(ui->cropLastStartXSpinBox->value()),static_cast<double>(ui->cropLastStartZSpinBox->value())}));

    // Job Settings
    data.push_back(factory.createCharArray("parseCluster"));
    data.push_back(factory.createScalar<bool>(ui->cropParseClusterCheckBox->isChecked()));

    data.push_back(factory.createCharArray("masterCompute"));
    data.push_back(factory.createScalar<bool>(ui->cropMasterComputeCheckBox->isChecked()));

    data.push_back(factory.createCharArray("cpusPerTask"));
    data.push_back(factory.createScalar<uint64_t>(ui->cropCpusPerTaskLineEdit->text().toULongLong()));

    data.push_back(factory.createCharArray("cpuOnlyNodes"));
    data.push_back(factory.createScalar<bool>(ui->cropCpuOnlyNodesCheckBox->isChecked()));

    // Advanced Job Settings

    if(!ui->cropJobLogDirLineEdit->text().isEmpty()){
        data.push_back(factory.createCharArray("jobLogDir"));
        data.push_back(factory.createCharArray(ui->cropJobLogDirLineEdit->text().toStdString()));
    }

    if(!ui->cropUuidLineEdit->text().isEmpty()){
        data.push_back(factory.createCharArray("uuid"));
        data.push_back(factory.createCharArray(ui->cropUuidLineEdit->text().toStdString()));
    }

    QString funcType = "crop";

    // Send data to the MATLAB thread
    auto cMPJNPC = std::make_tuple(mainPath, ui->jobNameLineEdit->text(),ui->parseClusterCheckBox->isChecked());
    emit jobStart(matlabJobSettings(outA, data, funcType,cMPJNPC));
}

void MainWindow::selectFolderPath(){
    QLineEdit* result = nullptr;

    if(((QPushButton *)sender())->objectName().contains("cropResult")){
        result = ui->cropResultPathLineEdit;
    }

    else {
        result = ui->cropJobLogDirLineEdit;
    }

    QFileInfo folder_path = QFileDialog::getExistingDirectory(this,"Select or Create and Select the Folder",mostRecentDir);
    if(!folder_path.absoluteFilePath().isEmpty()){
        result->setText(folder_path.absoluteFilePath());
        mostRecentDir = folder_path.absoluteFilePath();
    }
}

