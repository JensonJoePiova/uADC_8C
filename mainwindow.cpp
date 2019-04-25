
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"
#include "writeregistermodel.h"

#include <QModbusTcpClient>
#include <QModbusRtuSerialMaster>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QUrl>
#include <QTimer>
#include <QDebug>
#include "qcustomplot.h"
#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QKeyEvent>


enum ModbusConnection {
    Serial,
    Tcp
};


//  double channel_00=0;
//  double channel_01=0;
double channel_1=0;
double channel_2=0;
double channel_3=0;
double channel_4=0;
double channel_5=0;
double channel_6=0;
double channel_7=0;
double channel_8=0;
double deviceID = 0;
double DACfrequency =0;
double DAChalf = 0;
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , lastRequest(nullptr)
    , modbusDevice(nullptr)
{
    ui->setupUi(this);
    m_settingsDialog = new SettingsDialog(this);
    initActions();
    writeModel = new WriteRegisterModel(this);
    writeModel->setStartAddress(ui->writeAddress->value());
    writeModel->setNumberOfValues(ui->writeSize->currentText());

    ui->writeValueTable->setModel(writeModel);
    ui->writeValueTable->hideColumn(2);
    connect(writeModel, &WriteRegisterModel::updateViewport, ui->writeValueTable->viewport(),
            static_cast<void (QWidget::*)()>(&QWidget::update));

    ui->writeTable->addItem(tr("Holding Registers"), QModbusDataUnit::HoldingRegisters);
    ui->writeTable->addItem(tr("Coils"), QModbusDataUnit::Coils);
    ui->writeTable->addItem(tr("Discrete Inputs"), QModbusDataUnit::DiscreteInputs);
    ui->writeTable->addItem(tr("Input Registers"), QModbusDataUnit::InputRegisters);


    ui->connectType->setCurrentIndex(0);
    on_connectType_currentIndexChanged(0);

    auto model = new QStandardItemModel(26, 1, this);
    for (int i = 0; i < 26; ++i)
        model->setItem(i, new QStandardItem(QStringLiteral("%1").arg(i + 1)));
    ui->writeSize->setModel(model);
    ui->writeSize->setCurrentText("26");
    connect(ui->writeSize,&QComboBox::currentTextChanged, writeModel,
            &WriteRegisterModel::setNumberOfValues);

    auto valueChanged = static_cast<void (QSpinBox::*)(int)> (&QSpinBox::valueChanged);
    connect(ui->writeAddress, valueChanged, writeModel, &WriteRegisterModel::setStartAddress);
    connect(ui->writeAddress, valueChanged, this, [this, model](int i) {
        int lastPossibleIndex = 0;
        const int currentIndex = ui->writeSize->currentIndex();
        for (int ii = 0; ii < 10; ++ii) {
            if (ii < (10 - i)) {
                lastPossibleIndex = ii;
                model->item(ii)->setEnabled(true);
            } else {
                model->item(ii)->setEnabled(false);
            }
        }
        if (currentIndex > lastPossibleIndex)
            ui->writeSize->setCurrentIndex(lastPossibleIndex);
    });


    QTimer *timer=new QTimer;
    //1ms调用一次，想干啥都行
    //假设按钮是pushButton
    connect(timer, &QTimer::timeout, this, [&](){ ui->readButton->clicked(); });
    timer->start(0);

    // configure plot to have two right axes:
    ui->Gwidget->yAxis->setTickLabels(false);
    //        connect(ui->Gwidget->yAxis2, SIGNAL(rangeChanged(QCPRange)), ui->Gwidget->yAxis, SLOT(setRange(QCPRange))); // left axis only mirrors inner right axis
    ui->Gwidget->yAxis->setVisible(true);
    ui->Gwidget->yAxis2->setVisible(true);
    ui->Gwidget->yAxis2->setTicks(true);
    ui->Gwidget->axisRect()->axis(QCPAxis::atRight, 0)->setPadding(88); // add some padding to have space for tags
    // create graphs:
    mGraph1 = ui->Gwidget->addGraph(ui->Gwidget->xAxis, ui->Gwidget->axisRect()->axis(QCPAxis::atRight, 0));
    mGraph2 = ui->Gwidget->addGraph(ui->Gwidget->xAxis, ui->Gwidget->axisRect()->axis(QCPAxis::atRight, 0));
    mGraph3 = ui->Gwidget->addGraph(ui->Gwidget->xAxis, ui->Gwidget->axisRect()->axis(QCPAxis::atRight, 0));
    mGraph4 = ui->Gwidget->addGraph(ui->Gwidget->xAxis, ui->Gwidget->axisRect()->axis(QCPAxis::atRight, 0));
    mGraph5 = ui->Gwidget->addGraph(ui->Gwidget->xAxis, ui->Gwidget->axisRect()->axis(QCPAxis::atRight, 0));
    mGraph6 = ui->Gwidget->addGraph(ui->Gwidget->xAxis, ui->Gwidget->axisRect()->axis(QCPAxis::atRight, 0));
    mGraph7 = ui->Gwidget->addGraph(ui->Gwidget->xAxis, ui->Gwidget->axisRect()->axis(QCPAxis::atRight, 0));
    mGraph8 = ui->Gwidget->addGraph(ui->Gwidget->xAxis, ui->Gwidget->axisRect()->axis(QCPAxis::atRight, 0));

    mGraph1->setPen(QPen(QColor(250, 120, 0)));
    mGraph2->setPen(QPen(QColor(255, 0, 0)));
    mGraph3->setPen(QPen(QColor(0, 255, 0)));
    mGraph4->setPen(QPen(QColor(0, 0, 255)));
    mGraph5->setPen(QPen(QColor(255, 255, 255)));
    mGraph6->setPen(QPen(QColor(255, 255, 0)));
    mGraph7->setPen(QPen(QColor(255, 0, 255)));
    mGraph8->setPen(QPen(QColor(0, 255, 255)));


    connect(&mDataTimer, SIGNAL(timeout()), this, SLOT(timerSlot()));

  //  connect(&mDataTimer, SIGNAL(timeout()), this, SLOT(writeTofile()));


    //!这里应该再加上一个信号与槽函数,用来控制储存的自由性,而不是一运行就储存而不能停下来.
    //!
    //!
    //!
    mDataTimer.start(34);
    //! 这里的定时器有两个作用,第一个作用就是用它来每次进行刷新和重绘图像,
    //! 另外一个作用就是用它在刷新的 同时,每次刷新都存储一个结构体,这样就能刷新一次存储一次数据  确保每次图像都是和数据一一对应的关系.
    //! 因此在实现该功能的时候,直接写在一个函数里面,或者说单独写一个函数,以避免污染绘图函数.

 //  connect(ui->Gwidget->xAxis, SIGNAL(rangeChanged(QCPRange)), ui->Gwidget->yAxis, SLOT(setRange(QCPRange)));
  connect(ui->Gwidget->yAxis, SIGNAL(rangeChanged(QCPRange)), ui->Gwidget->yAxis2, SLOT(setRange(QCPRange)));

    ui->Gwidget->setInteractions(QCP::iRangeDrag|QCP::iRangeZoom| QCP::iSelectAxes |
                                 QCP::iSelectLegend | QCP::iSelectPlottables |
                                 QCP::iMultiSelect|QCP::iSelectItems );



    ui->Gwidget->plotLayout()->insertRow(0);
    QCPTextElement *title = new QCPTextElement(ui->Gwidget, "All ", QFont("sans", 17, QFont::Bold));
    ui->Gwidget->plotLayout()->addElement(0, 0, title);

    ui->Gwidget->legend->setVisible(true);
    QFont legendFont = font();
    legendFont.setPointSize(8);
    ui->Gwidget->legend->setFont(legendFont);
    ui->Gwidget->legend->setSelectedFont(legendFont);
    ui->Gwidget->legend->setSelectableParts(QCPLegend::spItems);




    connect(ui->Gwidget, SIGNAL(selectionChangedByUser()), this, SLOT(selectionChanged()));
    // connect slots that takes care that when an axis is selected, only that direction can be dragged and zoomed:
    connect(ui->Gwidget, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(mousePress()));
    connect(ui->Gwidget, SIGNAL(mouseWheel(QWheelEvent*)), this, SLOT(mouseWheel()));



    /*
    // make bottom and left axes transfer their ranges to top and right axes:
    connect(ui->Gwidget->xAxis, SIGNAL(rangeChanged(QCPRange)), ui->Gwidget->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->Gwidget->yAxis, SIGNAL(rangeChanged(QCPRange)), ui->Gwidget->yAxis2, SLOT(setRange(QCPRange)));
    */


    // connect some interaction slots:
    connect(ui->Gwidget, SIGNAL(axisDoubleClick(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)), this, SLOT(axisLabelDoubleClick(QCPAxis*,QCPAxis::SelectablePart)));
    connect(ui->Gwidget, SIGNAL(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)), this, SLOT(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*)));
    connect(title, SIGNAL(doubleClicked(QMouseEvent*)), this, SLOT(titleDoubleClick(QMouseEvent*)));

    // connect slot that shows a message in the status bar when a graph is clicked:
    connect(ui->Gwidget, SIGNAL(plottableClick(QCPAbstractPlottable*,int,QMouseEvent*)), this, SLOT(graphClicked(QCPAbstractPlottable*,int)));

    // setup policy and connect slot for context menu popup:
    ui->Gwidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->Gwidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenuRequest(QPoint)));


}

MainWindow::~MainWindow()
{
    if (modbusDevice)
        modbusDevice->disconnectDevice();
    delete modbusDevice;

    delete ui;
}

void MainWindow::initActions()
{
    ui->actionConnect->setEnabled(true);
    ui->actionDisconnect->setEnabled(false);
    ui->actionExit->setEnabled(true);
    ui->actionOptions->setEnabled(true);

    connect(ui->actionConnect, &QAction::triggered,
            this, &MainWindow::on_connectButton_clicked);
    connect(ui->actionDisconnect, &QAction::triggered,
            this, &MainWindow::on_connectButton_clicked);

    connect(ui->actionExit, &QAction::triggered, this, &QMainWindow::close);
    connect(ui->actionOptions, &QAction::triggered, m_settingsDialog, &QDialog::show);
}

void MainWindow::on_connectType_currentIndexChanged(int index)
{
    if (modbusDevice) {
        modbusDevice->disconnectDevice();
        delete modbusDevice;
        modbusDevice = nullptr;
    }

    auto type = static_cast<ModbusConnection> (index);
    if (type == Serial) {
        modbusDevice = new QModbusRtuSerialMaster(this);
    } else if (type == Tcp) {
        modbusDevice = new QModbusTcpClient(this);
        if (ui->portEdit->text().isEmpty())
            ui->portEdit->setText(QLatin1Literal("127.0.0.1:502"));
    }

    connect(modbusDevice, &QModbusClient::errorOccurred, [this](QModbusDevice::Error) {
        statusBar()->showMessage(modbusDevice->errorString(), 5000);
    });

    if (!modbusDevice) {
        ui->connectButton->setDisabled(true);
        if (type == Serial)
            statusBar()->showMessage(tr("Could not create Modbus master."), 5000);
        else
            statusBar()->showMessage(tr("Could not create Modbus client."), 5000);
    } else {
        connect(modbusDevice, &QModbusClient::stateChanged,
                this, &MainWindow::onStateChanged);
    }
}

void MainWindow::on_connectButton_clicked()
{
    if (!modbusDevice)
        return;

    statusBar()->clearMessage();
    if (modbusDevice->state() != QModbusDevice::ConnectedState) {
        if (static_cast<ModbusConnection> (ui->connectType->currentIndex()) == Serial) {
            modbusDevice->setConnectionParameter(QModbusDevice::SerialPortNameParameter,
                                                 ui->portEdit->text());
            modbusDevice->setConnectionParameter(QModbusDevice::SerialParityParameter,
                                                 m_settingsDialog->settings().parity);
            modbusDevice->setConnectionParameter(QModbusDevice::SerialBaudRateParameter,
                                                 m_settingsDialog->settings().baud);
            modbusDevice->setConnectionParameter(QModbusDevice::SerialDataBitsParameter,
                                                 m_settingsDialog->settings().dataBits);
            modbusDevice->setConnectionParameter(QModbusDevice::SerialStopBitsParameter,
                                                 m_settingsDialog->settings().stopBits);
        } else {
            const QUrl url = QUrl::fromUserInput(ui->portEdit->text());
            modbusDevice->setConnectionParameter(QModbusDevice::NetworkPortParameter, url.port());
            modbusDevice->setConnectionParameter(QModbusDevice::NetworkAddressParameter, url.host());
        }
        modbusDevice->setTimeout(m_settingsDialog->settings().responseTime);
        modbusDevice->setNumberOfRetries(m_settingsDialog->settings().numberOfRetries);
        if (!modbusDevice->connectDevice()) {
            statusBar()->showMessage(tr("Connect failed: ") + modbusDevice->errorString(), 5000);
        } else {
            ui->actionConnect->setEnabled(false);
            ui->actionDisconnect->setEnabled(true);
        }
    } else {
        modbusDevice->disconnectDevice();
        ui->actionConnect->setEnabled(true);
        ui->actionDisconnect->setEnabled(false);
    }
}

void MainWindow::onStateChanged(int state)
{
    bool connected = (state != QModbusDevice::UnconnectedState);
    ui->actionConnect->setEnabled(!connected);
    ui->actionDisconnect->setEnabled(connected);

    if (state == QModbusDevice::UnconnectedState)
        ui->connectButton->setText(tr("Connect"));
    else if (state == QModbusDevice::ConnectedState)
        ui->connectButton->setText(tr("Disconnect"));
}

void MainWindow::on_readButton_clicked()
{
    if (!modbusDevice)
        return;
    ui->readValue->clear();
    statusBar()->clearMessage();

    if (auto *reply = modbusDevice->sendReadRequest(readRequest(), ui->serverEdit->value())) {
        if (!reply->isFinished())
            connect(reply, &QModbusReply::finished, this, &MainWindow::readReady);


        else
            delete reply; // broadcast replies return immediately
    } else {
        statusBar()->showMessage(tr("Read error: ") + modbusDevice->errorString(), 5000);
    }
}

void MainWindow::readReady()
{
    auto reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();
        for (uint i = 0; i < unit.valueCount(); i++)

        {
            const QString entry = tr("Address: %1, Value: %2").arg(unit.startAddress() + i)
                    .arg(QString::number(unit.value(i),
                                         unit.registerType() <= QModbusDataUnit::Coils ? 10 : 10));

                     qDebug()<<"内存地址:"<<unit.startAddress()+i<<"   通道采样值   "<<unit.value(i);

            //传递参数的传值
  //          channel_00 = unit.value(14);
  //          channel_01 = unit.value(15);
            channel_1 = unit.value(16);
            channel_2 = unit.value(17);
            channel_3 = unit.value(18);
            channel_4 = unit.value(19);
            channel_5 = unit.value(20);
            channel_6 = unit.value(21);
            channel_7 = unit.value(22);
            channel_8 = unit.value(23);
            deviceID = unit.value(24);
            DACfrequency = unit.value(25);
            DAChalf = unit.value(26);

            ui->readValue->addItem(entry);
        }
    } else if (reply->error() == QModbusDevice::ProtocolError) {
        statusBar()->showMessage(tr("Read response error: %1 (Mobus exception: 0x%2)").
                                 arg(reply->errorString()).
                                 arg(reply->rawResult().exceptionCode(), -1, 16), 5000);
    } else {
        statusBar()->showMessage(tr("Read response error: %1 (code: 0x%2)").
                                 arg(reply->errorString()).
                                 arg(reply->error(), -1, 16), 5000);
    }

    reply->deleteLater();
}

void MainWindow::on_writeButton_clicked()
{
    if (!modbusDevice)
        return;
    statusBar()->clearMessage();

    QModbusDataUnit writeUnit = writeRequest();
    QModbusDataUnit::RegisterType table = writeUnit.registerType();
    for (uint i = 0; i < writeUnit.valueCount(); i++) {
        if (table == QModbusDataUnit::Coils)
            writeUnit.setValue(i, writeModel->m_coils[i + writeUnit.startAddress()]);
        else
            writeUnit.setValue(i, writeModel->m_holdingRegisters[i + writeUnit.startAddress()]);
    }

    if (auto *reply = modbusDevice->sendWriteRequest(writeUnit, ui->serverEdit->value())) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, [this, reply]() {
                if (reply->error() == QModbusDevice::ProtocolError) {
                    statusBar()->showMessage(tr("Write response error: %1 (Mobus exception: 0x%2)")
                                             .arg(reply->errorString()).arg(reply->rawResult().exceptionCode(), -1, 16),
                                             5000);
                } else if (reply->error() != QModbusDevice::NoError) {
                    statusBar()->showMessage(tr("Write response error: %1 (code: 0x%2)").
                                             arg(reply->errorString()).arg(reply->error(), -1, 16), 5000);
                }
                reply->deleteLater();
            });
        } else {
            // broadcast replies return immediately
            reply->deleteLater();
        }
    } else {
        statusBar()->showMessage(tr("Write error: ") + modbusDevice->errorString(), 5000);
    }
}

void MainWindow::on_readWriteButton_clicked()
{
    if (!modbusDevice)
        return;
    ui->readValue->clear();
    statusBar()->clearMessage();

    QModbusDataUnit writeUnit = writeRequest();
    QModbusDataUnit::RegisterType table = writeUnit.registerType();
    for (uint i = 0; i < writeUnit.valueCount(); i++) {
        if (table == QModbusDataUnit::Coils)
            writeUnit.setValue(i, writeModel->m_coils[i + writeUnit.startAddress()]);
        else
            writeUnit.setValue(i, writeModel->m_holdingRegisters[i + writeUnit.startAddress()]);
    }

    if (auto *reply = modbusDevice->sendReadWriteRequest(readRequest(), writeUnit,
                                                         ui->serverEdit->value())) {
        if (!reply->isFinished())
            connect(reply, &QModbusReply::finished, this, &MainWindow::readReady);
        else
            delete reply; // broadcast replies return immediately
    } else {
        statusBar()->showMessage(tr("Read error: ") + modbusDevice->errorString(), 5000);
    }
}

void MainWindow::on_writeTable_currentIndexChanged(int index)
{
    const bool coilsOrHolding = index == 0 || index == 3;
    if (coilsOrHolding) {
        ui->writeValueTable->setColumnHidden(1, index != 0);
        ui->writeValueTable->setColumnHidden(2, index != 3);
        ui->writeValueTable->resizeColumnToContents(0);
    }

    ui->readWriteButton->setEnabled(index == 3);
    ui->writeButton->setEnabled(coilsOrHolding);
    ui->writeGroupBox->setEnabled(coilsOrHolding);
}

QModbusDataUnit MainWindow::readRequest() const
{
    const auto table =
            static_cast<QModbusDataUnit::RegisterType> (ui->writeTable->currentData().toInt());

    int startAddress = ui->readAddress->value();
    Q_ASSERT(startAddress >= 0 && startAddress < 32);

    // do not go beyond 10 entries
    int numberOfEntries = qMin(ui->readSize->currentText().toInt(), 32 - startAddress);
    return QModbusDataUnit(table, startAddress, numberOfEntries);
}

QModbusDataUnit MainWindow::writeRequest() const
{
    const auto table =
            static_cast<QModbusDataUnit::RegisterType> (ui->writeTable->currentData().toInt());

    int startAddress = ui->writeAddress->value();
    Q_ASSERT(startAddress >= 0 && startAddress < 32);

    // do not go beyond 10 entries
    int numberOfEntries = qMin(ui->writeSize->currentText().toInt(), 32 - startAddress);
    return QModbusDataUnit(table, startAddress, numberOfEntries);
}
void MainWindow::timerSlot()
{
    static QTime time(QTime::currentTime());
    // calculate two new data points:
    double key = time.elapsed()/1000.0; // time elapsed since start of demo, in seconds
    // calculate and add a new data point to each graph:

    mGraph1->addData(key, channel_1);
    mGraph2->addData(key, channel_2);
    mGraph3->addData(key, channel_3);
    mGraph4->addData(key, channel_4);
    mGraph5->addData(key, channel_5);
    mGraph6->addData(key, channel_6);
    mGraph7->addData(key, channel_7);
    mGraph8->addData(key, channel_8);

    //addData(x,y)
    // make key axis range scroll with the data:
    //    ui->Gwidget->xAxis->rescale();
    //   ui->Gwidget->yAxis->rescale();
    /*
    mGraph1->rescaleValueAxis(false, true);
    mGraph2->rescaleValueAxis(false, true);
    mGraph3->rescaleValueAxis(false, true);
    mGraph4->rescaleValueAxis(false, true);
    mGraph5->rescaleValueAxis(false, true);
    mGraph6->rescaleValueAxis(false, true);
    mGraph7->rescaleValueAxis(false, true);
    mGraph8->rescaleValueAxis(false, true);

*/
 //   ui->Gwidget->xAxis->rescale();
    //修改scale
    // ui->Gwidget->xAxis->setRange(key+1, 300, Qt::AlignRight);//设定x轴的范围
    double length =ui->Gwidget->xAxis->range().upper - ui->Gwidget->xAxis->range().lower;
    ui->Gwidget->xAxis->setRange(key, length, Qt::AlignVCenter);
  //  ui->Gwidget->yAxis2->setRange(-30, length*25);
  //  ui->Gwidget->yAxis2->setRange(-30, 2000);
    ui->Gwidget->replot();

    // update the vertical axis tag positions and texts to match the rightmost data point of the graphs:
    double graph1Value = mGraph1->dataMainValue(key-1);
    double graph2Value = mGraph2->dataMainValue(key-1);
    double graph3Value = mGraph3->dataMainValue(key-1);
    double graph4Value = mGraph4->dataMainValue(key-1);
    double graph5Value = mGraph5->dataMainValue(key-1);
    double graph6Value = mGraph6->dataMainValue(key-1);
    double graph7Value = mGraph7->dataMainValue(key-1);
    double graph8Value = mGraph8->dataMainValue(key-1);
    //-1是删除一个点，添加新的一个点
    //################################################################################################################
    //控制坐标轴意义
    ui->Gwidget->xAxis->setLabelColor(Qt::red);
    ui->Gwidget->xAxis->setLabel("时间(t)");
    ui->Gwidget->xAxis->setLabelPadding(5);
    ui->Gwidget->xAxis->setLabelFont(QFont(font().family(), 10));
    ui->Gwidget->yAxis->setLabelColor(Qt::red);
    ui->Gwidget->yAxis->setLabel("电压幅值（mV）");
    ui->Gwidget->yAxis->setLabelFont(QFont(font().family(), 10));

    //################################################################################################################
    //控制网格和背景
    ui->Gwidget->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    ui->Gwidget->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    ui->Gwidget->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    ui->Gwidget->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    ui->Gwidget->xAxis->grid()->setSubGridVisible(true);
    ui->Gwidget->yAxis->grid()->setSubGridVisible(true);
    ui->Gwidget->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    ui->Gwidget->yAxis->grid()->setZeroLinePen(Qt::NoPen);
    QLinearGradient plotGradient;
    plotGradient.setStart(0, 0);
    plotGradient.setFinalStop(0, 350);
    plotGradient.setColorAt(0, QColor(80, 80, 80));
    plotGradient.setColorAt(1, QColor(50, 50, 50));
    ui->Gwidget->setBackground(plotGradient);
    QLinearGradient axisRectGradient;
    axisRectGradient.setStart(0, 0);
    axisRectGradient.setFinalStop(0, 350);
    axisRectGradient.setColorAt(0, QColor(80, 80, 80));
    axisRectGradient.setColorAt(1, QColor(30, 30, 30));
    ui->Gwidget->axisRect()->setBackground(axisRectGradient);

    //################################################################################################################
    //ui->Gwidget->axisRect()->setRangeDrag(Qt::Horizontal);// Qt::Horizontal     Vertical
    // ui->Gwidget->xAxis->setRange(ui->Gwidget->xAxis->range().upper, ui->Gwidget->xAxis->range().lower-ui->Gwidget->xAxis->range().upper, Qt::AlignRight);
}

//此函数用于显示右键菜单,关于右键菜单的功能实现需要采用信号与槽机制来解决.

void MainWindow::contextMenuEvent(QContextMenuEvent *){
    // 主菜单
    QMenu *MainMenu = new QMenu(this);
    //主菜单的 子项
    QAction *option = new QAction(MainMenu);
    option->setText("选项");
    QAction *about = new QAction(MainMenu);
    about->setText("关于");

    QAction *screen = new QAction(MainMenu);
    screen->setText("显示");

    QAction *escape = new QAction(MainMenu);
    escape->setText("退出");

    QList<QAction*> actionList;
    actionList<<option\
             <<about\
            <<screen\
           <<escape;
    //添加子项到主菜单
    MainMenu->addActions(actionList);



    //子菜单1
    QMenu *childMenu = new QMenu();
    //子菜单1的 子项

    QAction *addfile = new QAction(childMenu);
    addfile->setText("开始存储");

    QAction *delfile = new QAction(childMenu);
    delfile->setText("终止存储");


    QList<QAction *> childActionList;
    childActionList<<addfile\
                  <<delfile ;
    childMenu->addActions(childActionList);



    //子菜单2
    QMenu *childMenu2 = new QMenu();
    //子菜单2的 子项
    QAction *channel_0 = new QAction(childMenu2);
    channel_0->setText("未指定通道1");



    
    QList<QAction *> childActionList2;

    childActionList2<<channel_0 ;

    childMenu2->addActions(childActionList2);


    //设置子菜单1 归属opion
    option->setMenu(childMenu);

    //主菜单添加子菜单1
    MainMenu->addMenu(childMenu);



    //设置子菜单2 归属screen
    screen->setMenu(childMenu2);

    //主菜单添加子菜单2
    MainMenu->addMenu(childMenu2);


    //!信号与槽的绑定 ###################################################################################################

    connect(escape, &QAction::triggered, this, close);

    //!信号与槽的绑定 ###################################################################################################

    // 移动到当前鼠标所在位置
    MainMenu->exec(QCursor::pos());
}

void MainWindow::writeTofile()
{
    static QTime time(QTime::currentTime());
    // calculate two new data points:
    int time1 = time.elapsed()/1000.0;

    QDateTime current_date_time =QDateTime::currentDateTime();
    QString current_date =current_date_time.toString("yyyy.MM.dd hh:mm:ss.zzz ddd");

    QFile file("23333333333333333333333333333.txt");
    if (!file.open(QIODevice::Append | QIODevice::Text))
    {      return;
        QMessageBox::critical(this,"error","open failed,info do not  save! ","sure");
    }
    QTextStream out(&file);
    out<<"Date:"<<current_date<<"\tchannel8-1:\t"<<"\t"<<"Time"<<"\t"<<time1<<"\t"<<deviceID<<"\t"
      <<DACfrequency<<"\t"<<DAChalf<<"\t"<<channel_8<<"\t"
     <<channel_7<<"\t"<<channel_6<<"\t"<<channel_5<<"\t"
    <<channel_4<<"\t"<<channel_3<<"\t"<<channel_2<<"\t"<<channel_1<<'\n';

    file.close();

}

void MainWindow::keyPressEvent(QKeyEvent *event)
{


    //压缩图像
    if(event->key() == Qt::Key_A)
    {
           double length1 =ui->Gwidget->yAxis2->range().upper - ui->Gwidget->yAxis2->range().lower;
     ui->Gwidget->yAxis2->setRange(-30, length1);
    }

    //伸展图像
    if(event->key() == Qt::Key_S)
    {
           double length1 =ui->Gwidget->yAxis2->range().upper - ui->Gwidget->yAxis2->range().lower;
     ui->Gwidget->yAxis2->setRange(-30, length1-60);

    }

    //开始存储，   当数据进行存储的时候， 应该弹出一个窗口，证明现在已经开始，不然可能无法得知是不是已经正在进行，当然终止存储也应该有相应的弹出窗口
   if(event->key() == Qt::Key_D)
   {
      connect(&mDataTimer, SIGNAL(timeout()), this, SLOT(writeTofile()));
   }
   //中止存储
   if(event->key() == Qt::Key_F)
   {
      disconnect(&mDataTimer, SIGNAL(timeout()), this, SLOT(writeTofile()));

   }

   //图像暂停
   if(event->key() == Qt::Key_G)
   {
        disconnect(&mDataTimer, SIGNAL(timeout()), this, SLOT(timerSlot()));
   }

   //图像继续开始
   if(event->key() == Qt::Key_H)
   {
        connect(&mDataTimer, SIGNAL(timeout()), this, SLOT(timerSlot()));
   }
 // 实现图像的删除与添加。

}
void MainWindow::titleDoubleClick(QMouseEvent* event)
{
  Q_UNUSED(event)
  if (QCPTextElement *title = qobject_cast<QCPTextElement*>(sender()))
  {
    // Set the plot title by double clicking on it
    bool ok;
    QString newTitle = QInputDialog::getText(this, "QCustomPlot example", "New plot title:", QLineEdit::Normal, title->text(), &ok);
    if (ok)
    {
      title->setText(newTitle);
      ui->Gwidget->replot();
    }
  }
}

void MainWindow::axisLabelDoubleClick(QCPAxis *axis, QCPAxis::SelectablePart part)
{
  // Set an axis label by double clicking on it
  if (part == QCPAxis::spAxisLabel) // only react when the actual axis label is clicked, not tick label or axis backbone
  {
    bool ok;
    QString newLabel = QInputDialog::getText(this, "QCustomPlot example", "New axis label:", QLineEdit::Normal, axis->label(), &ok);
    if (ok)
    {
      axis->setLabel(newLabel);
      ui->Gwidget->replot();
    }
  }
}

void MainWindow::legendDoubleClick(QCPLegend *legend, QCPAbstractLegendItem *item)
{
  // Rename a graph by double clicking on its legend item
  Q_UNUSED(legend)
  if (item) // only react if item was clicked (user could have clicked on border padding of legend where there is no item, then item is 0)
  {
    QCPPlottableLegendItem *plItem = qobject_cast<QCPPlottableLegendItem*>(item);
    bool ok;
    QString newName = QInputDialog::getText(this, "QCustomPlot example", "New graph name:", QLineEdit::Normal, plItem->plottable()->name(), &ok);
    if (ok)
    {
      plItem->plottable()->setName(newName);
      ui->Gwidget->replot();
    }
  }
}

void MainWindow::selectionChanged()
{
  /*
   normally, axis base line, axis tick labels and axis labels are selectable separately, but we want
   the user only to be able to select the axis as a whole, so we tie the selected states of the tick labels
   and the axis base line together. However, the axis label shall be selectable individually.

   The selection state of the left and right axes shall be synchronized as well as the state of the
   bottom and top axes.

   Further, we want to synchronize the selection of the graphs with the selection state of the respective
   legend item belonging to that graph. So the user can select a graph by either clicking on the graph itself
   or on its legend item.
  */

  // make top and bottom axes be selected synchronously, and handle axis and tick labels as one selectable object:
  if (ui->Gwidget->xAxis->selectedParts().testFlag(QCPAxis::spAxis) || ui->Gwidget->xAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
      ui->Gwidget->xAxis2->selectedParts().testFlag(QCPAxis::spAxis) || ui->Gwidget->xAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
  {
    ui->Gwidget->xAxis2->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
    ui->Gwidget->xAxis->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
  }
  // make left and right axes be selected synchronously, and handle axis and tick labels as one selectable object:
  if (ui->Gwidget->yAxis->selectedParts().testFlag(QCPAxis::spAxis) || ui->Gwidget->yAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
      ui->Gwidget->yAxis2->selectedParts().testFlag(QCPAxis::spAxis) || ui->Gwidget->yAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
  {
    ui->Gwidget->yAxis2->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
    ui->Gwidget->yAxis->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
  }

  // synchronize selection of graphs with selection of corresponding legend items:
  for (int i=0; i<ui->Gwidget->graphCount(); ++i)
  {
    QCPGraph *graph = ui->Gwidget->graph(i);
    QCPPlottableLegendItem *item = ui->Gwidget->legend->itemWithPlottable(graph);
    if (item->selected() || graph->selected())
    {
      item->setSelected(true);
      graph->setSelection(QCPDataSelection(graph->data()->dataRange()));
    }
  }
}

void MainWindow::mousePress()
{
  // if an axis is selected, only allow the direction of that axis to be dragged
  // if no axis is selected, both directions may be dragged

  if (ui->Gwidget->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
    ui->Gwidget->axisRect()->setRangeDrag(ui->Gwidget->xAxis->orientation());
  else if (ui->Gwidget->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
    ui->Gwidget->axisRect()->setRangeDrag(ui->Gwidget->yAxis->orientation());
  else
    ui->Gwidget->axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
}

void MainWindow::mouseWheel()
{
  // if an axis is selected, only allow the direction of that axis to be zoomed
  // if no axis is selected, both directions may be zoomed

  if (ui->Gwidget->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
    ui->Gwidget->axisRect()->setRangeZoom(ui->Gwidget->xAxis->orientation());
  else if (ui->Gwidget->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
    ui->Gwidget->axisRect()->setRangeZoom(ui->Gwidget->yAxis->orientation());
  else
    ui->Gwidget->axisRect()->setRangeZoom(Qt::Horizontal|Qt::Vertical);
}



void MainWindow::addRandomGraph()
{
  int n = 50; // number of points in graph
  double xScale = (rand()/(double)RAND_MAX + 0.5)*2;
  double yScale = (rand()/(double)RAND_MAX + 0.5)*2;
  double xOffset = (rand()/(double)RAND_MAX - 0.5)*4;
  double yOffset = (rand()/(double)RAND_MAX - 0.5)*10;
  double r1 = (rand()/(double)RAND_MAX - 0.5)*2;
  double r2 = (rand()/(double)RAND_MAX - 0.5)*2;
  double r3 = (rand()/(double)RAND_MAX - 0.5)*2;
  double r4 = (rand()/(double)RAND_MAX - 0.5)*2;
  QVector<double> x(n), y(n);
  for (int i=0; i<n; i++)
  {
    x[i] = (i/(double)n-0.5)*10.0*xScale + xOffset;
    y[i] = (qSin(x[i]*r1*5)*qSin(qCos(x[i]*r2)*r4*3)+r3*qCos(qSin(x[i])*r4*2))*yScale + yOffset;
  }

  ui->Gwidget->addGraph();
  ui->Gwidget->graph()->setName(QString("New graph %1").arg(ui->Gwidget->graphCount()-1));
  ui->Gwidget->graph()->setData(x, y);
  ui->Gwidget->graph()->setLineStyle((QCPGraph::LineStyle)(rand()%5+1));
  if (rand()%100 > 50)
    ui->Gwidget->graph()->setScatterStyle(QCPScatterStyle((QCPScatterStyle::ScatterShape)(rand()%14+1)));
  QPen graphPen;
  graphPen.setColor(QColor(rand()%245+10, rand()%245+10, rand()%245+10));
  graphPen.setWidthF(rand()/(double)RAND_MAX*2+1);
  ui->Gwidget->graph()->setPen(graphPen);
  ui->Gwidget->replot();
}



void MainWindow::removeSelectedGraph()
{
  if (ui->Gwidget->selectedGraphs().size() > 0)
  {
    ui->Gwidget->removeGraph(ui->Gwidget->selectedGraphs().first());
    ui->Gwidget->replot();
  }
}

void MainWindow::removeAllGraphs()
{
  ui->Gwidget->clearGraphs();
  ui->Gwidget->replot();
  disconnect(&mDataTimer, SIGNAL(timeout()), this, SLOT(timerSlot()));
}

void MainWindow::contextMenuRequest(QPoint pos)
{
  QMenu *menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose);

  if (ui->Gwidget->legend->selectTest(pos, false) >= 0) // context menu on legend requested
  {
    menu->addAction("Move to top left", this, SLOT(moveLegend()))->setData((int)(Qt::AlignTop|Qt::AlignLeft));
    menu->addAction("Move to top center", this, SLOT(moveLegend()))->setData((int)(Qt::AlignTop|Qt::AlignHCenter));
    menu->addAction("Move to top right", this, SLOT(moveLegend()))->setData((int)(Qt::AlignTop|Qt::AlignRight));
    menu->addAction("Move to bottom right", this, SLOT(moveLegend()))->setData((int)(Qt::AlignBottom|Qt::AlignRight));
    menu->addAction("Move to bottom left", this, SLOT(moveLegend()))->setData((int)(Qt::AlignBottom|Qt::AlignLeft));
  } else  // general context menu on graphs requested
  {
    menu->addAction("Add random graph", this, SLOT(addRandomGraph()));
     menu->addAction("Add All graph", this, SLOT(addAllGraph()));
    if (ui->Gwidget->selectedGraphs().size() > 0)
      menu->addAction("Remove selected graph", this, SLOT(removeSelectedGraph()));
    if (ui->Gwidget->graphCount() > 0)
      menu->addAction("Remove all graphs", this, SLOT(removeAllGraphs()));
  }

  menu->popup(ui->Gwidget->mapToGlobal(pos));
}

void MainWindow::moveLegend()
{
  if (QAction* contextAction = qobject_cast<QAction*>(sender())) // make sure this slot is really called by a context menu action, so it carries the data we need
  {
    bool ok;
    int dataInt = contextAction->data().toInt(&ok);
    if (ok)
    {
      ui->Gwidget->axisRect()->insetLayout()->setInsetAlignment(0, (Qt::Alignment)dataInt);
      ui->Gwidget->replot();
    }
  }
}

void MainWindow::graphClicked(QCPAbstractPlottable *plottable, int dataIndex)
{
  // since we know we only have QCPGraphs in the plot, we can immediately access interface1D()
  // usually it's better to first check whether interface1D() returns non-zero, and only then use it.
  double dataValue = plottable->interface1D()->dataMainValue(dataIndex);
  QString message = QString("Clicked on graph '%1' at data point #%2 with value %3.").arg(plottable->name()).arg(dataIndex).arg(dataValue);
  ui->statusBar->showMessage(message, 2500);
}

//实现的功能是将清除的图像重新展现在坐标轴上
void MainWindow::addAllGraph()
{

    connect(&mDataTimer, SIGNAL(timeout()), this, SLOT(timerSlot()));
    ui->Gwidget->replot();
}
