/*
    Locally - LAN file transfer.
    Copyright (C) 2016 Abdul Aris R. <abdularisrahmanudin10@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "sendertablemodel.h"
#include "receivertablemodel.h"
#include "devicelistmodel.h"
#include "devicebroadcaster.h"
#include "transferserver.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event);

private Q_SLOTS:
    void onSendActionTriggered();
    void onSettingsActionTriggered();
    void onAboutActionTriggered();

    void onNewReceiverAdded(Receiver* rec);

    void onSenderTableDoubleClicked(const QModelIndex& index);
    void onSenderClearClicked();
    void onSenderCancelClicked();
    void onSenderPauseClicked();
    void onSenderResumeClicked();

    void onReceiverTableDoubleClicked(const QModelIndex& index);
    void onReceiverClearClicked();
    void onReceiverCancelClicked();
    void onReceiverPauseClicked();
    void onReceiverResumeClicked();

    void onSenderTableSelectionChanged(const QItemSelection& selected,
                                       const QItemSelection& deselected);
    void onReceiverTableSelectionChanged(const QItemSelection& selected,
                                         const QItemSelection& deselected);

    void onSenderTableContextMenuRequested(const QPoint& pos);
    void onReceiverTableContextMenuRequested(const QPoint& pos);

    void openSenderFileInCurrentIndex();
    void openSenderFolderInCurrentIndex();
    void removeSenderItemInCurrentIndex();

    void openReceiverFileInCurrentIndex();
    void openReceiverFolderInCurrentIndex();
    void removeReceiverItemInCurrentIndex();
    void deleteReceiverFileInCurrentIndex();

    void onSelectedSenderStateChanged(TransferState state);
    void onSelectedReceiverStateChanged(TransferState state);

private:
    void setupToolbar();
    void connectSignals();
    void sendFile(const QString& fileName, const Device& receiver);

    bool anyActiveSender();
    bool anyActiveReceiver();

    Ui::MainWindow *ui;

    SenderTableModel* mSenderModel;
    ReceiverTableModel* mReceiverModel;
    DeviceListModel* mDeviceModel;

    DeviceBroadcaster* mBroadcaster;
    TransferServer* mTransServer;


};

#endif // MAINWINDOW_H