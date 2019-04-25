
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModbusDataUnit>
#include "qcustomplot.h"
#include "axistag.h"
QT_BEGIN_NAMESPACE

class QModbusClient;
class QModbusReply;

namespace Ui {
class MainWindow;
class SettingsDialog;
}

QT_END_NAMESPACE

class SettingsDialog;
class WriteRegisterModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

 void KeyPressEvent(QKeyEvent *e);

private:
    void initActions();
    QModbusDataUnit readRequest() const;
    QModbusDataUnit writeRequest() const;

private slots:
    void on_connectButton_clicked();
    void onStateChanged(int state);

    void on_readButton_clicked();
    void readReady();

    void on_writeButton_clicked();
    void on_readWriteButton_clicked();

    void on_connectType_currentIndexChanged(int);
    void on_writeTable_currentIndexChanged(int);




    void titleDoubleClick(QMouseEvent *event);
    void axisLabelDoubleClick(QCPAxis* axis, QCPAxis::SelectablePart part);
    void legendDoubleClick(QCPLegend* legend, QCPAbstractLegendItem* item);
    void selectionChanged();
    void mousePress();
    void mouseWheel();
    void addRandomGraph();
    void removeSelectedGraph();
    void removeAllGraphs();
    void contextMenuRequest(QPoint pos);
    void moveLegend();
    void graphClicked(QCPAbstractPlottable *plottable, int dataIndex);
    void addAllGraph();


    void timerSlot();
  //  void saveText();
    void contextMenuEvent(QContextMenuEvent *);
void writeTofile();



void keyPressEvent(QKeyEvent *event);


private:
    Ui::MainWindow *ui;
    QModbusReply *lastRequest;
    QModbusClient *modbusDevice;
    SettingsDialog *m_settingsDialog;
    WriteRegisterModel *writeModel;



 //   QCustomPlot *mPlot;
    QPointer<QCPGraph> mGraph1;
    QPointer<QCPGraph> mGraph2;
    QPointer<QCPGraph> mGraph3;
    QPointer<QCPGraph> mGraph4;
    QPointer<QCPGraph> mGraph5;
    QPointer<QCPGraph> mGraph6;
    QPointer<QCPGraph> mGraph7;
    QPointer<QCPGraph> mGraph8;

    QTimer mDataTimer;
};

#endif // MAINWINDOW_H
