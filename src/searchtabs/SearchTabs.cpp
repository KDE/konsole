/*
    SPDX-FileCopyrightText: 2024 Troy Hoover <troytjh98@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "SearchTabs.h"

// Qt
#include <QApplication>
#include <QKeyEvent>
#include <QModelIndex>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>

// KDE
#include <KFuzzyMatcher>
#include <KLocalizedString>

// Konsole
#include "KonsoleSettings.h"

using namespace Konsole;

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                            Fuzzy Search Model                             */
/*                                                                           */
/* ------------------------------------------------------------------------- */

class SearchTabsFilterProxyModel final : public QSortFilterProxyModel
{
public:
    SearchTabsFilterProxyModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
    }

protected:
    bool lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const override
    {
        auto sm = sourceModel();

        const int l = static_cast<SearchTabsModel *>(sm)->idxScore(sourceLeft);
        const int r = static_cast<SearchTabsModel *>(sm)->idxScore(sourceRight);
        return l < r;
    }

    bool filterAcceptsRow(int sourceRow, const QModelIndex &parent) const override
    {
        Q_UNUSED(parent)

        if (pattern.isEmpty()) {
            return true;
        }

        auto sm = static_cast<SearchTabsModel *>(sourceModel());
        if (!sm->isValid(sourceRow)) {
            return false;
        }

        QStringView tabNameMatchPattern = pattern;

        const QString &name = sm->idxToName(sourceRow);

        // dont use the QStringView(QString) ctor
        if (tabNameMatchPattern.isEmpty()) {
            sm->setScoreForIndex(sourceRow, 1);
            return true;
        }

        auto result = filterByName(QStringView(name.data(), name.size()), tabNameMatchPattern);

        sm->setScoreForIndex(sourceRow, result.score);

        return result.matched;
    }

public Q_SLOTS:
    bool setFilterText(const QString &text)
    {
        beginResetModel();
        pattern = text;
        endResetModel();

        return true;
    }

private:
    static inline KFuzzyMatcher::Result filterByName(QStringView name, QStringView pattern)
    {
        return KFuzzyMatcher::match(pattern, name);
    }

private:
    QString pattern;
};

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Search Tabs                                */
/*                                                                           */
/* ------------------------------------------------------------------------- */

SearchTabs::SearchTabs(ViewManager *viewManager)
    : QFrame(viewManager->activeContainer()->window())
    , m_viewManager(viewManager)
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    setProperty("_breeze_force_frame", true);

    // handle resizing of MainWindow
    window()->installEventFilter(this);

    // ensure the components have some proper frame
    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(0);
    layout->setContentsMargins(QMargins());
    setLayout(layout);

    // create input line for search query
    m_inputLine = new QLineEdit(this);
    m_inputLine->setClearButtonEnabled(true);
    m_inputLine->addAction(QIcon::fromTheme(QStringLiteral("search")), QLineEdit::LeadingPosition);
    m_inputLine->setTextMargins(QMargins() + style()->pixelMetric(QStyle::PM_ButtonMargin));
    m_inputLine->setPlaceholderText(i18nc("@label:textbox", "Search..."));
    m_inputLine->setToolTip(i18nc("@info:tooltip", "Enter a tab name to search for here"));
    m_inputLine->setCursor(Qt::IBeamCursor);
    m_inputLine->setFont(QApplication::font());
    m_inputLine->setFrame(false);
    // When the widget focus is set, focus input box instead
    setFocusProxy(m_inputLine);

    layout->addWidget(m_inputLine);

    m_listView = new QTreeView(this);
    layout->addWidget(m_listView, 1);
    m_listView->setProperty("_breeze_borders_sides", QVariant::fromValue(QFlags(Qt::TopEdge)));
    m_listView->setTextElideMode(Qt::ElideLeft);
    m_listView->setUniformRowHeights(true);

    // model stores tab information
    m_model = new SearchTabsModel(this);

    // switch to selected tab
    connect(m_inputLine, &QLineEdit::returnPressed, this, &SearchTabs::slotReturnPressed);
    connect(m_listView, &QTreeView::activated, this, &SearchTabs::slotReturnPressed);
    connect(m_listView, &QTreeView::clicked, this, &SearchTabs::slotReturnPressed); // for single click

    m_inputLine->installEventFilter(this);
    m_listView->installEventFilter(this);
    m_listView->setHeaderHidden(true);
    m_listView->setRootIsDecorated(false);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setModel(m_model);

    // use fuzzy sort to identify tabs with matching titles
    connect(m_inputLine, &QLineEdit::textChanged, this, [this](const QString &text) {
        // initialize the proxy model when there is something to filter
        bool didFilter = false;
        if (!m_proxyModel) {
            m_proxyModel = new SearchTabsFilterProxyModel(this);
            m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
            didFilter = m_proxyModel->setFilterText(text);
            m_proxyModel->setSourceModel(m_model);
            m_listView->setModel(m_proxyModel);
        } else {
            didFilter = m_proxyModel->setFilterText(text);
        }
        if (didFilter) {
            m_listView->viewport()->update();
            reselectFirst();
        }
    });

    setHidden(true);

    // fill stuff
    updateState();
}

bool SearchTabs::eventFilter(QObject *obj, QEvent *event)
{
    // catch key presses + shortcut overrides to allow to have
    // ESC as application wide shortcut, too, see bug 409856
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (obj == m_inputLine) {
            const bool forward2list = (keyEvent->key() == Qt::Key_Up) || (keyEvent->key() == Qt::Key_Down) || (keyEvent->key() == Qt::Key_PageUp)
                || (keyEvent->key() == Qt::Key_PageDown);

            if (forward2list) {
                QCoreApplication::sendEvent(m_listView, event);
                return true;
            }

        } else if (obj == m_listView) {
            const bool forward2input = (keyEvent->key() != Qt::Key_Up) && (keyEvent->key() != Qt::Key_Down) && (keyEvent->key() != Qt::Key_PageUp)
                && (keyEvent->key() != Qt::Key_PageDown) && (keyEvent->key() != Qt::Key_Tab) && (keyEvent->key() != Qt::Key_Backtab);

            if (forward2input) {
                QCoreApplication::sendEvent(m_inputLine, event);
                return true;
            }
        }

        if (keyEvent->key() == Qt::Key_Escape) {
            hide();
            deleteLater();
            return true;
        }
    }

    if (event->type() == QEvent::FocusOut && !(m_inputLine->hasFocus() || m_listView->hasFocus())) {
        hide();
        deleteLater();
        return true;
    }

    // handle resizing
    if (window() == obj && event->type() == QEvent::Resize) {
        updateViewGeometry();
    }

    return QWidget::eventFilter(obj, event);
}

void SearchTabs::reselectFirst()
{
    int first = 0;
    const auto *model = m_listView->model();

    if (m_viewManager->viewProperties().size() > 1 && model->rowCount() > 1 && m_inputLine->text().isEmpty()) {
        first = 1;
    }

    QModelIndex index = model->index(first, 0);
    m_listView->setCurrentIndex(index);
}

void SearchTabs::updateState()
{
    m_model->refresh(m_viewManager);
    reselectFirst();

    updateViewGeometry();
    show();
    raise();
    setFocus();
}

void SearchTabs::slotReturnPressed()
{
    // switch to tab using the unique ViewProperties identifier
    // (the view identifier is off by 1)
    const QModelIndex index = m_listView->currentIndex();
    m_viewManager->setCurrentView(index.data(SearchTabsModel::View).toInt() - 1);

    hide();
    deleteLater();

    window()->setFocus();
}

void SearchTabs::updateViewGeometry()
{
    // find MainWindow rectangle
    QRect boundingRect = window()->contentsRect();

    // set search tabs window size
    static constexpr int minWidth = 125;
    const int maxWidth = boundingRect.width();
    const int preferredWidth = maxWidth / 4.8;

    static constexpr int minHeight = 250;
    const int maxHeight = boundingRect.height();
    const int preferredHeight = maxHeight / 4;

    const QSize size{qMin(maxWidth, qMax(preferredWidth, minWidth)), qMin(maxHeight, qMax(preferredHeight, minHeight))};

    // resize() doesn't work here, so use setFixedSize() instead
    setFixedSize(size);

    // set the position just below/above the tab bar
    int y;
    int mainWindowHeight = window()->geometry().height();
    int containerHeight = m_viewManager->activeContainer()->geometry().height();

    // only calculate the tab bar height if it's visible
    int tabBarHeight = 0;
    auto isTabBarVisible = m_viewManager->activeContainer()->tabBar()->isVisible();
    if (isTabBarVisible) {
        tabBarHeight = m_viewManager->activeContainer()->tabBar()->geometry().height();
    }

    // set the position above the south tab bar
    auto tabBarPos = (QTabWidget::TabPosition)KonsoleSettings::tabBarPosition();
    if (tabBarPos == QTabWidget::South && isTabBarVisible) {
        y = mainWindowHeight - tabBarHeight - size.height() - 6;
    }
    // set the position below the north tab bar
    // set the position to the top right corner if the tab bar is hidden
    else {
        y = mainWindowHeight - containerHeight + tabBarHeight + 6;
    }
    boundingRect.setTop(y);

    // set the position to the right of the window
    int scrollBarWidth = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    int mainWindowWidth = window()->geometry().width();
    int x = mainWindowWidth - size.width() - scrollBarWidth - 6;
    const QPoint position{x, y};
    move(position);
}
