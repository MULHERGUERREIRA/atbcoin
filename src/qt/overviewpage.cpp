// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "bitcoinunits.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "transactionfilterproxy.h"
#include "transactiontablemodel.h"
#include "walletmodel.h"
#include <QFontDatabase>

#include <QAbstractItemDelegate>
#include <QPainter>

#define DECORATION_SIZE 85
#define NUM_ITEMS 5
class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(const PlatformStyle *platformStyle):
        QAbstractItemDelegate(), unit(BitcoinUnits::BTC),
        platformStyle(platformStyle)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        int unit=static_cast<int>(BitcoinUnit::BTC);
        painter->save();
        QIcon icon;
        int type=qvariant_cast<int>(index.data(TransactionTableModel::TypeRole));
        switch (type) {
        case 1:
            icon=platformStyle->SingleColorIcon(":/icons/tx_mined",QColor::fromRgb(0x6c,0xba,0x07));
            break;
        case 4:
        case 5:
            icon=platformStyle->SingleColorIcon(":/icons/tx_input",QColor::fromRgb(0x6c,0xba,0x07));
            break;
        case 2:
        case 3:
            icon=platformStyle->SingleColorIcon(":/icons/tx_output",QColor::fromRgb(0xe3,0x3b,0x19));
            break;
        default:
            icon=platformStyle->SingleColorIcon(":/icons/tx_inout",QColor::fromRgb(0xe3,0x3b,0x19));
            break;
        }
        QRect mainRect = option.rect;
        mainRect.setWidth(mainRect.width()-40);

        QRect decorationRect(mainRect.topLeft()+QPoint(25,18), QSize(20, 20));
        QPainterPath path;
        path.addRoundedRect(QRect(mainRect.left(),mainRect.top()+3,mainRect.width()-1,mainRect.height()-3), 10, 10);
        painter->fillPath(path,Qt::white);
        painter->setPen(Qt::white);
        painter->drawPath(path);

        int xspace = DECORATION_SIZE -20;
        int ypad = 14;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect atbRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);

        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }
        painter->setPen(QColor::fromRgb(0x9d,0x9d,0x9d));
        QRect boundingRect;
        QFont font(painter->font());
        font.setBold(true);
#ifdef Q_OS_MAC
        font.setPointSize(13);
#endif
#ifdef Q_OS_WIN
        font.setPointSize(9);
#endif
#ifdef Q_OS_LINUX
        font.setPointSize(10);
#endif
        painter->setFont(font);
        if(painter->fontMetrics().width(address) > (mainRect.width() - 75)){
            address = address.left((mainRect.width() - 75) / painter->fontMetrics().width("V")) + " ...";
        }
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address, &boundingRect);


        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true, BitcoinUnits::separatorAlways,true);
        if(!confirmed)
        {
            amountText = QString("") + amountText + QString("");
        }
        amountText.remove("ATB");
        if(amount>=0)
            painter->setPen(QColor::fromRgb(0x6c,0xba,0x07));
        else
            painter->setPen(QColor::fromRgb(0xe3,0x3b,0x19));
        painter->drawText(amountRect, Qt::AlignLeft |Qt::AlignVCenter, amountText);
        painter->setPen(QColor::fromRgb(0x19, 0xa3, 0x7e));
        atbRect.setX(atbRect.x() + amountText.size() + painter->fontMetrics().width(amountText) - 5 );
        painter->drawText(atbRect,Qt::AlignLeft |Qt::AlignVCenter,"ATB");
        painter->setPen(QColor::fromRgb(0x9d,0x9d,0x9d));
        amountRect.setWidth(amountRect.width() - 15);
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;
    const PlatformStyle *platformStyle;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    currentBalance(-1),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    currentWatchOnlyBalance(-1),
    currentWatchUnconfBalance(-1),
    currentWatchImmatureBalance(-1),
    image(0),
    currentStake(-1),
    currentWatchOnlyStake(-1),
    
    txdelegate(new TxViewDelegate(platformStyle)),
    filter(0)
{
    ui->setupUi(this);    this->setWindowFlags(this->windowFlags()& ~Qt::WindowContextHelpButtonHint);

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = platformStyle->SingleColorIcon(":/icons/warning",Qt::white);
    icon.addPixmap(icon.pixmap(QSize(64,64), QIcon::Normal), QIcon::Disabled); // also set the disabled icon because we are using a disabled QPushButton to work around missing HiDPI support of QLabel (https://bugreports.qt.io/browse/QTBUG-42503)
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);
    image=new BackgroundImage(":/icons/overviewImage",this);
    image->resize(439,258);
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->LabelDollar->hide();
    ui->LabelDollar_watchonly->hide();
    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));
    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
}
/*void OverviewPage::resizeEvent(QResizeEvent *event){
}*/
void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        Q_EMIT transactionClicked(filter->mapToSource(index));
}

OverviewPage::~OverviewPage()
{
    delete ui;
}


void OverviewPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance, const CAmount& stake, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance, const CAmount& watchOnlyStake)

{
    currentBalance = balance;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    
    currentStake = stake;
    
    currentWatchOnlyBalance = watchOnlyBalance;
    currentWatchUnconfBalance = watchUnconfBalance;
    currentWatchImmatureBalance = watchImmatureBalance;
    
    currentWatchOnlyStake = watchOnlyStake;
    
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(BitcoinUnit::BTC_HTML, balance, false, BitcoinUnits::separatorAlways));
    ui->labelUnconfirmed->setText(BitcoinUnits::formatWithUnit(BitcoinUnit::BTC_HTML, unconfirmedBalance, false, BitcoinUnits::separatorAlways));
    ui->labelImmature->setText(BitcoinUnits::formatWithUnit(BitcoinUnit::BTC_HTML, immatureBalance, false, BitcoinUnits::separatorAlways));
    
    ui->labelStake->setText(BitcoinUnits::formatWithUnit(BitcoinUnit::BTC_HTML, stake, false, BitcoinUnits::separatorAlways));
    ui->labelTotal->setText(BitcoinUnits::formatWithUnit(BitcoinUnit::BTC_HTML, balance + unconfirmedBalance + immatureBalance + stake, false, BitcoinUnits::separatorAlways));
    
    ui->labelWatchAvailable->setText(BitcoinUnits::formatWithUnit(BitcoinUnit::BTC_HTML, watchOnlyBalance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchPending->setText(BitcoinUnits::formatWithUnit(BitcoinUnit::BTC_HTML, watchUnconfBalance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchImmature->setText(BitcoinUnits::formatWithUnit(BitcoinUnit::BTC_HTML, watchImmatureBalance, false, BitcoinUnits::separatorAlways));
    
    ui->labelWatchStake->setText(BitcoinUnits::formatWithUnit(BitcoinUnit::BTC_HTML, watchOnlyStake, false, BitcoinUnits::separatorAlways));
    ui->labelWatchTotal->setText(BitcoinUnits::formatWithUnit(BitcoinUnit::BTC_HTML, watchOnlyBalance + watchUnconfBalance + watchImmatureBalance + watchOnlyStake, false, BitcoinUnits::separatorAlways));
    
    ui->LabelDollar_watchonly->setText(BitcoinUnits::formatWithUnit(BitcoinUnit::Dollar, watchOnlyBalance + watchUnconfBalance + watchImmatureBalance + watchOnlyStake, false, BitcoinUnits::separatorAlways));
    ui->LabelDollar->setText(BitcoinUnits::formatWithUnit(BitcoinUnit::Dollar, balance + unconfirmedBalance + immatureBalance + stake, false, BitcoinUnits::separatorAlways));
    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    
    bool showStake = stake != 0;
    bool showWatchOnlyImmature = watchImmatureBalance != 0;
    bool showWatchOnlyStake = watchOnlyStake != 0;
    

    // for symmetry reasons also show immature label when the watch-only one is shown
    ui->labelImmature->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelImmatureText->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelWatchImmature->setVisible(showWatchOnlyImmature); // show watch-only immature balance
    
    ui->labelStake->setVisible(showStake || showWatchOnlyStake);
    ui->labelStakeText->setVisible(showStake || showWatchOnlyStake);
    ui->labelWatchStake->setVisible(showWatchOnlyStake); // show watch-only stake balance
    
}

// show/hide watch-only labels
void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly)
{
    ui->labelSpendable->setVisible(showWatchOnly);      // show spendable label (only when watch-only is active)
    ui->labelWatchonly->setVisible(showWatchOnly);      // show watch-only label
    ui->lineWatchBalance->setVisible(showWatchOnly);    // show watch-only balance separator line
    ui->labelWatchAvailable->setVisible(showWatchOnly); // show watch-only available balance
    ui->labelWatchPending->setVisible(showWatchOnly);   // show watch-only pending balance
    ui->labelWatchTotal->setVisible(showWatchOnly);     // show watch-only total balance
   // ui->LabelDollar_watchonly->setVisible(showWatchOnly);
    if (!showWatchOnly)
    {
        
        ui->labelWatchImmature->hide();
	ui->labelWatchStake->hide();
        
    }
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        
        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance(), model->getStake(),
                   model->getWatchBalance(), model->getWatchUnconfirmedBalance(), model->getWatchImmatureBalance(), model->getWatchStake());
        connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this, SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));
       

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        updateWatchOnlyLabels(model->haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));
    }

    // update the display unit, to not use the default ("ATB")
    updateDisplayUnit();
}
void OverviewPage::resizeEvent(QResizeEvent *){
    if(image)
        image->move(this->width()*0.01,this->height()-256);
}
void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if(currentBalance != -1)
        
            setBalance(currentBalance, currentUnconfirmedBalance, currentImmatureBalance, currentStake,
                       currentWatchOnlyBalance, currentWatchUnconfBalance, currentWatchImmatureBalance, currentWatchOnlyStake);
        

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}
