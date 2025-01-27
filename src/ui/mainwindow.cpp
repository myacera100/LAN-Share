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

#include <QDesktopServices>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QToolButton>
#include <QCloseEvent>
#include <QtDebug>
#include <QListView>
#include <QTreeView>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "receiverselectordialog.h"
#include "settingsdialog.h"
#include "settings.h"
#include "aboutdialog.h"
#include "util.h"
#include "transfer/sender.h"
#include "transfer/receiver.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupActions();
    setupToolbar();
    setupSystrayIcon();
    setWindowTitle(PROGRAM_NAME);

    mBroadcaster = new DeviceBroadcaster(this);
    mBroadcaster->start();
    mSenderModel = new TransferTableModel(this);
    mReceiverModel = new TransferTableModel(this);
    mDeviceModel = new DeviceListModel(mBroadcaster, this);
    mTransServer = new TransferServer(mDeviceModel, this);
    mTransServer->listen();

//    mSenderModel->setHeaderData((int) TransferTableModel::Column::Peer, Qt::Horizontal, tr("Receiver"));
//    mReceiverModel->setHeaderData((int) TransferTableModel::Column::Peer, Qt::Horizontal, tr("Sender"));

    ui->sentContentTbView->setModel(mSenderModel);
    ui->receivedContentTbView->setModel(mReceiverModel);

    ui->sentContentTbView->setColumnWidth((int)TransferTableModel::Column::FileName, 340);
    ui->sentContentTbView->setColumnWidth((int)TransferTableModel::Column::Progress, 160);

    ui->receivedContentTbView->setColumnWidth((int)TransferTableModel::Column::FileName, 340);
    ui->receivedContentTbView->setColumnWidth((int)TransferTableModel::Column::Progress, 160);

    connectSignals();
}

MainWindow::~MainWindow()
{
    delete ui;
}

/*
 * Sebelum ditutup, check apakah masih terdapat proses trasfer
 * yg berlangsung, (Sending atau Receiving)
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (mSystrayIcon && mSystrayIcon->isVisible() && !mForceQuit) {
        setMainWindowVisibility(false);
        event->ignore();
        return;
    }

    auto checkTransferState = [](Transfer* t) {
        if (!t)
            return false;
        TransferState state = t->getTransferInfo()->getState();
        return state == TransferState::Paused ||
                state == TransferState::Transfering ||
                state == TransferState::Waiting;
    };

    auto checkTransferModel = [&](TransferTableModel* model) {
        int count = model->rowCount();
        for (int i = 0; i < count; i++) {
            Transfer* t = model->getTransfer(i);
            if (checkTransferState(t)) {
                return true;
            }
        }

        return false;
    };

    bool needToConfirm = checkTransferModel(mSenderModel);
    if (!needToConfirm)
        needToConfirm = checkTransferModel(mReceiverModel);

    if (needToConfirm) {
        QMessageBox::StandardButton ret =
                QMessageBox::question(this, tr("Confirm close"),
                                      tr("You are about to close & abort all transfers. Do you want to continue?"));
        if (ret == QMessageBox::No) {
            event->ignore();
            mForceQuit = false;
            return;
        }
    }

    event->accept();
    qApp->quit();
}

void MainWindow::setMainWindowVisibility(bool visible)
{
    if (visible) {
        show();
        if (!isActiveWindow()) {
            raise();
        }
//        setWindowState(Qt::WindowNoState);
//        qApp->processEvents();
//        setWindowState(Qt::WindowActive);
//        qApp->processEvents();
        qApp->setActiveWindow(this);
        qApp->processEvents();
    }
    else {
        hide();
    }
}

void MainWindow::connectSignals()
{
    connect(mTransServer, &TransferServer::newReceiverAdded, this, &MainWindow::onNewReceivingTaskInitiated);

    QItemSelectionModel* senderSel = ui->sentContentTbView->selectionModel();
    connect(senderSel, &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onSentItemSelectionChanged);

    QItemSelectionModel* receiverSel = ui->receivedContentTbView->selectionModel();
    connect(receiverSel, &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onReceivedItemSelectionChanged);
}

void MainWindow::sendFile(const QString& folderName, const QString &filePath, const Device &receiver)
{
    Sender* sender = new Sender(receiver, folderName, filePath, this);
    sender->start();
    mSenderModel->insertTransfer(sender);
    QModelIndex progressIdx = mSenderModel->index(0, (int)TransferTableModel::Column::Progress);

    /*
     * tambah progress bar pada item transfer
     */
    QProgressBar* progress = new QProgressBar();
    connect(sender->getTransferInfo(), &TransferInfo::progressChanged, progress, &QProgressBar::setValue);

    ui->sentContentTbView->setIndexWidget(progressIdx, progress);
    ui->sentContentTbView->scrollToTop();
}

void MainWindow::selectReceiversAndSendTheFiles(QVector<QPair<QString, QString> > dirNameAndFullPath)
{
    ReceiverSelectorDialog dialog(mDeviceModel);
    if (dialog.exec() == QDialog::Accepted) {
        QVector<Device> receivers = dialog.getSelectedDevices();
        for (const Device& receiver : receivers) {
            if (receiver.isValid()) {

                /*
                 * Memastikan bahwa device/kompuer ini terdaftar di penerima
                 * Just to make sure.
                 */
                mBroadcaster->sendBroadcast();
                for (const auto& p : dirNameAndFullPath) {
                    sendFile(p.first, p.second, receiver);
                }

            }
        }
    }
}

void MainWindow::onShowMainWindowTriggered()
{
    setMainWindowVisibility(true);
}

void MainWindow::onSendFilesActionTriggered()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(this, tr("Select files"));
    if (fileNames.size() <= 0)
        return;

    QVector<QPair<QString, QString> > pairs;
    for (const auto& fName : fileNames)
        pairs.push_back( QPair<QString, QString>("", fName) );

    selectReceiversAndSendTheFiles(pairs);
}

void MainWindow::onSendFolderActionTriggered()
{
    QStringList dirs;
    QFileDialog fDialog(this);
    fDialog.setOption(QFileDialog::DontUseNativeDialog, true);
    fDialog.setFileMode(QFileDialog::Directory);
    fDialog.setOption(QFileDialog::ShowDirsOnly);

    /*
     * Enable multiple foder selection
     */
    QListView* lView = fDialog.findChild<QListView*>("listView");
    if (lView)
        lView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    QTreeView* tView = fDialog.findChild<QTreeView*>("treeView");
    if (tView)
        tView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    if (!fDialog.exec()) {
        return;
    }

    /*
     * Iterate through all selected folders
     */
    QVector< QPair<QString, QString> > pairs;
    QString dirName;
    dirs = fDialog.selectedFiles();
    foreach (dirName, dirs) {
        QDir dir(dirName);
        QVector< QPair<QString, QString> > ps =
                Util::getInnerDirNameAndFullFilePath(dir, dir.dirName());
        pairs.append(ps);
    }

    selectReceiversAndSendTheFiles(pairs);
}

void MainWindow::onSettingsActionTriggered()
{
    SettingsDialog dialog;
    dialog.exec();
}

void MainWindow::onAboutActionTriggered()
{
    AboutDialog dialog;
    dialog.exec();
}

void MainWindow::onNewReceivingTaskInitiated(Receiver *rec)
{
    QProgressBar* progress = new QProgressBar();
    TransferInfo *ti = rec->getTransferInfo();
    connect(ti, &TransferInfo::progressChanged, progress, &QProgressBar::setValue);
    connect(ti, &TransferInfo::done, this, &MainWindow::onTransferDone);
    mReceiverModel->insertTransfer(rec);
    QModelIndex progressIdx = mReceiverModel->index(0, (int)TransferTableModel::Column::Progress);

    ui->receivedContentTbView->setIndexWidget(progressIdx, progress);
    ui->receivedContentTbView->scrollToTop();
}

void MainWindow::onSentItemSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    if (selected.size() > 0) {

        QModelIndex first = selected.indexes().first();
        if (first.isValid()) {
            TransferInfo* ti = mSenderModel->getTransferInfo(first.row());
            ui->resumeSenderBtn->setEnabled(ti->canResume());
            ui->pauseSenderBtn->setEnabled(ti->canPause());
            ui->cancelSenderBtn->setEnabled(ti->canCancel());

            connect(ti, &TransferInfo::stateChanged, this, &MainWindow::onSelectedSenderStateChanged);
        }

    }

    if (deselected.size() > 0) {

        QModelIndex first = deselected.indexes().first();
        if (first.isValid()) {
            TransferInfo* ti = mSenderModel->getTransferInfo(first.row());
            disconnect(ti, &TransferInfo::stateChanged, this, &MainWindow::onSelectedSenderStateChanged);
        }

    }
}

void MainWindow::onSentItemDoubleClicked(const QModelIndex& index)
{
    openItem(mSenderModel, index);
}

void MainWindow::onSenderClearClicked()
{
    mSenderModel->clearCompleted();
}

void MainWindow::onSenderCancelClicked()
{
    QModelIndex currIndex = ui->sentContentTbView->currentIndex();
    if (currIndex.isValid()) {
        Transfer* sender = mSenderModel->getTransfer(currIndex.row());
        sender->cancel();
    }
}

void MainWindow::onSenderPauseClicked()
{
    QModelIndex currIndex = ui->sentContentTbView->currentIndex();
    if (currIndex.isValid()) {
        Transfer* sender = mSenderModel->getTransfer(currIndex.row());
        sender->pause();
    }
}

void MainWindow::onSenderResumeClicked()
{
    QModelIndex currIndex = ui->sentContentTbView->currentIndex();
    if (currIndex.isValid()) {
        Transfer* sender = mSenderModel->getTransfer(currIndex.row());
        sender->resume();
    }
}


void MainWindow::onReceiverTableDoubleClicked(const QModelIndex& index)
{
    if (index.isValid()) {
        TransferInfo* ti = mReceiverModel->getTransferInfo(index.row());
        if (ti && ti->getState() == TransferState::Finish)
            openSelectedReceivedFile();
    }
}

void MainWindow::onReceiverClearClicked()
{
    mReceiverModel->clearCompleted();
}

void MainWindow::onReceivedItemSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    if (selected.size() > 0) {

        QModelIndex first = selected.indexes().first();
        if (first.isValid()) {
            TransferInfo* ti = mReceiverModel->getTransferInfo(first.row());
            ui->resumeReceiverBtn->setEnabled(ti->canResume());
            ui->pauseReceiverBtn->setEnabled(ti->canPause());
            ui->cancelReceiverBtn->setEnabled(ti->canCancel());

            connect(ti, &TransferInfo::stateChanged, this, &MainWindow::onSelectedReceiverStateChanged);
        }

    }

    if (deselected.size() > 0) {

        QModelIndex first = deselected.indexes().first();
        if (first.isValid()) {
            TransferInfo* ti = mReceiverModel->getTransferInfo(first.row());
            disconnect(ti, &TransferInfo::stateChanged, this, &MainWindow::onSelectedReceiverStateChanged);
        }

    }
}

void MainWindow::onReceiverCancelClicked()
{
    QModelIndex currIndex = ui->receivedContentTbView->currentIndex();
    if (currIndex.isValid()) {
        Transfer* rec = mReceiverModel->getTransfer(currIndex.row());
        rec->cancel();
    }
}

void MainWindow::onReceiverPauseClicked()
{
    QModelIndex currIndex = ui->receivedContentTbView->currentIndex();
    if (currIndex.isValid()) {
        Transfer* rec = mReceiverModel->getTransfer(currIndex.row());
        rec->pause();
    }
}

void MainWindow::onReceiverResumeClicked()
{
    QModelIndex currIndex = ui->receivedContentTbView->currentIndex();
    if (currIndex.isValid()) {
        Transfer* rec = mReceiverModel->getTransfer(currIndex.row());
        rec->resume();
    }
}

void MainWindow::onSentItemContextMenu(const QPoint& pos)
{
    QModelIndex currIndex = ui->sentContentTbView->indexAt(pos);
    QMenu contextMenu;

    if (currIndex.isValid()) {
        TransferInfo* ti = mSenderModel->getTransferInfo(currIndex.row());
        TransferState state = ti->getState();
        bool enableRemove = state == TransferState::Finish ||
                            state == TransferState::Cancelled ||
                            state == TransferState::Disconnected ||
                            state == TransferState::Idle;

        mSenderRemoveAction->setEnabled(enableRemove);
        mSenderPauseAction->setEnabled(ti->canPause());
        mSenderResumeAction->setEnabled(ti->canResume());
        mSenderCancelAction->setEnabled(ti->canCancel());

        contextMenu.addAction(mSenderOpenAction);
        contextMenu.addAction(mSenderOpenFolderAction);
        contextMenu.addSeparator();
        contextMenu.addAction(mSendFilesAction);
        contextMenu.addAction(mSendFolderAction);
        contextMenu.addSeparator();
        contextMenu.addAction(mSenderRemoveAction);
        contextMenu.addAction(mSenderClearAction);
        contextMenu.addSeparator();
        contextMenu.addAction(mSenderPauseAction);
        contextMenu.addAction(mSenderResumeAction);
        contextMenu.addAction(mSenderCancelAction);
    }
    else {
        contextMenu.addAction(mSendFilesAction);
        contextMenu.addAction(mSendFolderAction);
        contextMenu.addSeparator();
        contextMenu.addAction(mSenderClearAction);
    }

    QPoint globPos = ui->sentContentTbView->mapToGlobal(pos);
    contextMenu.exec(globPos);
}

void MainWindow::onReceivedItemContextMenu(const QPoint& pos)
{
    QModelIndex currIndex = ui->receivedContentTbView->indexAt(pos);
    QMenu contextMenu;

    if (currIndex.isValid()) {
        TransferInfo* ti = mReceiverModel->getTransferInfo(currIndex.row());
        TransferState state = ti->getState();
        bool enableFileMenu = state == TransferState::Finish;
        bool enableRemove = state == TransferState::Finish ||
                            state == TransferState::Cancelled ||
                            state == TransferState::Disconnected ||
                            state == TransferState::Idle;

        mRecOpenAction->setEnabled(enableFileMenu);
        mRecOpenFolderAction->setEnabled(enableFileMenu);
        mRecRemoveAction->setEnabled(enableFileMenu | enableRemove);
        mRecDeleteAction->setEnabled(enableFileMenu);
        mRecPauseAction->setEnabled(ti->canPause());
        mRecResumeAction->setEnabled(ti->canResume());
        mRecCancelAction->setEnabled(ti->canCancel());

        contextMenu.addAction(mRecOpenAction);
        contextMenu.addAction(mRecOpenFolderAction);
        contextMenu.addAction(mRecRemoveAction);
        contextMenu.addAction(mRecDeleteAction);
        contextMenu.addAction(mRecClearAction);
        contextMenu.addSeparator();
        contextMenu.addAction(mRecPauseAction);
        contextMenu.addAction(mRecResumeAction);
        contextMenu.addAction(mRecCancelAction);
    }
    else {
        contextMenu.addAction(mRecClearAction);
    }

    QPoint globPos = ui->receivedContentTbView->mapToGlobal(pos);
    contextMenu.exec(globPos);
}

void MainWindow::openItem(TransferTableModel *const model, QModelIndex const &curIndex, bool isDir)
{
    QModelIndex fileNameIndex = model->index(curIndex.row(), (int)TransferTableModel::Column::FileName);
    QString fileName = model->data(fileNameIndex).toString();
    if (isDir) {
        fileName = QFileInfo(fileName).absoluteDir().absolutePath();
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(fileName));
}

void MainWindow::openSelectedSentFile()
{
    TransferTableModel *const model = static_cast<TransferTableModel *>(ui->sentContentTbView->model());
    openItem(model, ui->sentContentTbView->currentIndex());
}

void MainWindow::openSelectedSentFolder()
{
    TransferTableModel *const model = static_cast<TransferTableModel *>(ui->sentContentTbView->model());
    openItem(model, ui->sentContentTbView->currentIndex(), true);
}

void MainWindow::removeSelectedSentItem()
{
    QModelIndex currIndex = ui->sentContentTbView->currentIndex();
    mSenderModel->removeTransfer(currIndex.row());
}

void MainWindow::openSelectedReceivedFile()
{
    TransferTableModel *const model = static_cast<TransferTableModel *>(ui->receivedContentTbView->model());
    openItem(model, ui->receivedContentTbView->currentIndex());
}

void MainWindow::openSelectedReceivedFolder()
{
    TransferTableModel *const model = static_cast<TransferTableModel *>(ui->receivedContentTbView->model());
    openItem(model, ui->receivedContentTbView->currentIndex(), true);
}

void MainWindow::removeSelectedReceivedItem()
{
    QModelIndex currIndex = ui->receivedContentTbView->currentIndex();
    mReceiverModel->removeTransfer(currIndex.row());
}

void MainWindow::deleteItem(TransferTableModel *const model, QModelIndex const &curIndex, bool isDir)
{
    QModelIndex fileNameIndex = model->index(curIndex.row(), (int)TransferTableModel::Column::FileName);
    QString fileName = model->data(fileNameIndex).toString();

    QString str = tr("Are you sure wants to delete<p> %1?").arg(fileName);
    QMessageBox::StandardButton ret = QMessageBox::question(this, tr("Delete"), str);
    if (ret == QMessageBox::Yes) {
        QFile::remove(fileName);
        model->removeTransfer(curIndex.row());
    }
}

void MainWindow::deleteReceivedItem()
{
    QModelIndex curIndex = ui->receivedContentTbView->currentIndex();
    deleteItem(mReceiverModel, curIndex);
}

void MainWindow::onSelectedSenderStateChanged(TransferState state)
{
    ui->resumeSenderBtn->setEnabled(state == TransferState::Paused);
    ui->pauseSenderBtn->setEnabled(state == TransferState::Transfering || state == TransferState::Waiting);
    ui->cancelSenderBtn->setEnabled(state == TransferState::Transfering || state == TransferState::Waiting ||
                                    state == TransferState::Paused);
}

void MainWindow::onSelectedReceiverStateChanged(TransferState state)
{
    ui->resumeReceiverBtn->setEnabled(state == TransferState::Paused);
    ui->pauseReceiverBtn->setEnabled(state == TransferState::Transfering || state == TransferState::Waiting);
    ui->cancelReceiverBtn->setEnabled(state == TransferState::Transfering || state == TransferState::Waiting ||
                                    state == TransferState::Paused);
}

void MainWindow::onTransferDone()
{
    setMainWindowVisibility(true);
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason rs)
{
    if (rs == QSystemTrayIcon::DoubleClick) {
        if (isVisible()) {
            hide();
        } else {
            setMainWindowVisibility(true);
        }
    }
}

void MainWindow::quitApp()
{
    mForceQuit = true;
    close();
}

void MainWindow::setupToolbar()
{
    QMenu* sendMenu = new QMenu();
    sendMenu->addAction(mSendFilesAction);
    sendMenu->addAction(mSendFolderAction);

    QToolButton* sendBtn = new QToolButton();
    sendBtn->setPopupMode(QToolButton::InstantPopup);
    sendBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    sendBtn->setText(tr("Send"));
    sendBtn->setIcon(QIcon(":/img/send.png"));
    sendBtn->setMenu(sendMenu);
    ui->mainToolBar->addWidget(sendBtn);
    ui->mainToolBar->addSeparator();

    ui->mainToolBar->addAction(mSettingsAction);

    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    ui->mainToolBar->addWidget(spacer);

    QMenu* menu = new QMenu();
    menu->addAction(mAboutAction);
    menu->addAction(mAboutQtAction);

    QToolButton* aboutBtn = new QToolButton();
    aboutBtn->setText(tr("About"));
    aboutBtn->setToolTip(tr("About this program"));
    aboutBtn->setIcon(QIcon(":/img/about.png"));
    aboutBtn->setMenu(menu);
    aboutBtn->setPopupMode(QToolButton::InstantPopup);
    ui->mainToolBar->addWidget(aboutBtn);
}

void MainWindow::setupActions()
{
    mShowMainWindowAction = new QAction(tr("Show Main Window"), this);
    connect(mShowMainWindowAction, &QAction::triggered, this, &MainWindow::onShowMainWindowTriggered);
    mSendFilesAction = new QAction(QIcon(":/img/file.png"), tr("Send files..."), this);
    connect(mSendFilesAction, &QAction::triggered, this, &MainWindow::onSendFilesActionTriggered);
    mSendFolderAction = new QAction(QIcon(":/img/folder.png"), tr("Send folders..."), this);
    connect(mSendFolderAction, &QAction::triggered, this, &MainWindow::onSendFolderActionTriggered);
    mSettingsAction = new QAction(QIcon(":/img/settings.png"), tr("Settings"), this);
    connect(mSettingsAction, &QAction::triggered, this, &MainWindow::onSettingsActionTriggered);
    mAboutAction = new QAction(QIcon(":/img/about.png"), tr("About"), this);
    mAboutAction->setMenuRole(QAction::AboutRole);
    connect(mAboutAction, &QAction::triggered, this, &MainWindow::onAboutActionTriggered);
    mAboutQtAction = new QAction(tr("About Qt"), this);
    mAboutQtAction->setMenuRole(QAction::AboutQtRole);
    connect(mAboutQtAction, &QAction::triggered, QApplication::instance(), &QApplication::aboutQt);
    mQuitAction = new QAction(tr("Quit"), this);
    connect(mQuitAction, &QAction::triggered, this, &MainWindow::quitApp);

    mSenderOpenAction = new QAction(tr("Open"), this);
    connect(mSenderOpenAction, &QAction::triggered, this, &MainWindow::openSelectedSentFile);
    mSenderOpenFolderAction = new QAction(tr("Open folder"), this);
    connect(mSenderOpenFolderAction, &QAction::triggered, this, &MainWindow::openSelectedSentFolder);
    mSenderRemoveAction = new QAction(QIcon(":/img/remove.png"), tr("Remove"), this);
    connect(mSenderRemoveAction, &QAction::triggered, this, &MainWindow::removeSelectedSentItem);
    mSenderClearAction = new QAction(QIcon(":/img/clear.png"), tr("Clear"), this);
    connect(mSenderClearAction, &QAction::triggered, this, &MainWindow::onSenderClearClicked);
    mSenderPauseAction = new QAction(QIcon(":/img/pause.png"), tr("Pause"), this);
    connect(mSenderPauseAction, &QAction::triggered, this, &MainWindow::onSenderPauseClicked);
    mSenderResumeAction = new QAction(QIcon(":/img/resume.png"), tr("Resume"), this);
    connect(mSenderResumeAction, &QAction::triggered, this, &MainWindow::onSenderResumeClicked);
    mSenderCancelAction = new QAction(QIcon(":/img/cancel.png"), tr("Cancel"), this);
    connect(mSenderCancelAction, &QAction::triggered, this, &MainWindow::onSenderCancelClicked);

    mRecOpenAction = new QAction(tr("Open"), this);
    connect(mRecOpenAction, &QAction::triggered, this, &MainWindow::openSelectedReceivedFile);
    mRecOpenFolderAction = new QAction(tr("Open folder"), this);
    connect(mRecOpenFolderAction, &QAction::triggered, this, &MainWindow::openSelectedReceivedFolder);
    mRecRemoveAction = new QAction(QIcon(":/img/remove.png"), tr("Remove"), this);
    connect(mRecRemoveAction, &QAction::triggered, this, &MainWindow::removeSelectedReceivedItem);
    mRecDeleteAction = new QAction(tr("Delete from disk"), this);
    connect(mRecDeleteAction, &QAction::triggered, this, &MainWindow::deleteReceivedItem);
    mRecClearAction = new QAction(QIcon(":/img/clear.png"), tr("Clear"), this);
    connect(mRecClearAction, &QAction::triggered, this, &MainWindow::onReceiverClearClicked);
    mRecPauseAction = new QAction(QIcon(":/img/pause.png"), tr("Pause"), this);
    connect(mRecPauseAction, &QAction::triggered, this, &MainWindow::onReceiverPauseClicked);
    mRecResumeAction = new QAction(QIcon(":/img/resume.png"), tr("Resume"), this);
    connect(mRecResumeAction, &QAction::triggered, this, &MainWindow::onReceiverResumeClicked);
    mRecCancelAction = new QAction(QIcon(":/img/cancel.png"), tr("Cancel"), this);
    connect(mRecCancelAction, &QAction::triggered, this, &MainWindow::onReceiverCancelClicked);
}

void MainWindow::setupSystrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        mSystrayIcon = nullptr;
        return;
    }

    mSystrayMenu = new QMenu(this);
    mSystrayMenu->addAction(mShowMainWindowAction);
    mSystrayMenu->addSeparator();
    mSystrayMenu->addAction(mSendFilesAction);
    mSystrayMenu->addAction(mSendFolderAction);
    mSystrayMenu->addSeparator();
    mSystrayMenu->addAction(mAboutAction);
    mSystrayMenu->addAction(mAboutQtAction);
    mSystrayMenu->addSeparator();
    mSystrayMenu->addAction(mQuitAction);

    mSystrayIcon = new QSystemTrayIcon(QIcon(":/img/systray-icon.png"), this);
    mSystrayIcon->setToolTip(PROGRAM_NAME);
    mSystrayIcon->setContextMenu(mSystrayMenu);
    connect(mSystrayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);

    mSystrayIcon->show();
}
