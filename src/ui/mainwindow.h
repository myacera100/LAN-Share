/*
    LANShare - LAN file transfer.
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
#include <QSystemTrayIcon>

#include "model/transfertablemodel.h"
#include "model/devicelistmodel.h"
#include "transfer/devicebroadcaster.h"
#include "transfer/transferserver.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

public Q_SLOTS:
    void setMainWindowVisibility(bool visible = true);

private Q_SLOTS:
    void onShowMainWindowTriggered();
    void onSendFilesActionTriggered();
    void onSendFolderActionTriggered();
    void onSettingsActionTriggered();
    void onAboutActionTriggered();

    void onNewReceivingTaskInitiated(Receiver* rec);

    void onSentItemDoubleClicked(const QModelIndex& index);
    void onSenderClearClicked();
    void onSenderCancelClicked();
    void onSenderPauseClicked();
    void onSenderResumeClicked();

    void onReceiverTableDoubleClicked(const QModelIndex& index);
    void onReceiverClearClicked();
    void onReceiverCancelClicked();
    void onReceiverPauseClicked();
    void onReceiverResumeClicked();

    void onSentItemSelectionChanged(const QItemSelection& selected,
                                       const QItemSelection& deselected);
    void onReceivedItemSelectionChanged(const QItemSelection& selected,
                                         const QItemSelection& deselected);

    void onSentItemContextMenu(const QPoint& pos);
    void onReceivedItemContextMenu(const QPoint& pos);

    void openSelectedSentFile();
    void openSelectedSentFolder();
    void removeSelectedSentItem();

    void openSelectedReceivedFile();
    void openSelectedReceivedFolder();
    void removeSelectedReceivedItem();
    void deleteReceivedItem();

    void onSelectedSenderStateChanged(TransferState state);
    void onSelectedReceiverStateChanged(TransferState state);

    void onTransferDone();
    void onTrayActivated(QSystemTrayIcon::ActivationReason);

    void quitApp();

private:
    void setupActions();
    void setupToolbar();
    void setupSystrayIcon();
    void connectSignals();
    void sendFile(const QString& folderName, const QString& fileName, const Device& receiver);
    void selectReceiversAndSendTheFiles(QVector<QPair<QString, QString> > dirNameAndFullPath);

    void openItem(TransferTableModel *const model, const QModelIndex &curIndex, bool isDir = false);
    void deleteItem(TransferTableModel *const model, const QModelIndex &curIndex, bool isDir = false);

    bool anyActiveSender();
    bool anyActiveReceiver();

    bool mForceQuit{false};
    Ui::MainWindow *ui;
    QSystemTrayIcon* mSystrayIcon;
    QMenu* mSystrayMenu;

    TransferTableModel* mSenderModel;
    TransferTableModel* mReceiverModel;
    DeviceListModel* mDeviceModel;

    DeviceBroadcaster* mBroadcaster;
    TransferServer* mTransServer;

    QAction* mShowMainWindowAction;
    QAction* mSendFilesAction;
    QAction* mSendFolderAction;
    QAction* mSettingsAction;
    QAction* mAboutAction;
    QAction* mAboutQtAction;
    QAction* mQuitAction;

    QAction* mSenderOpenAction;
    QAction* mSenderOpenFolderAction;
    QAction* mSenderRemoveAction;
    QAction* mSenderClearAction;
    QAction* mSenderPauseAction;
    QAction* mSenderResumeAction;
    QAction* mSenderCancelAction;

    QAction* mRecOpenAction;
    QAction* mRecOpenFolderAction;
    QAction* mRecRemoveAction;
    QAction* mRecDeleteAction;
    QAction* mRecClearAction;
    QAction* mRecPauseAction;
    QAction* mRecResumeAction;
    QAction* mRecCancelAction;
};

#endif // MAINWINDOW_H
