#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "customkeyboard.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QTextStream>
#include <QRegExp>
#include <QFile>
#include <QAbstractItemView>
#include <QSettings>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QDateTime>
#include <QColor>
#include <QMessageBox>
#include <QRadialGradient>
#include <QImage>
#include <QFont>
#include <cstdlib>
#include <ctime>
#include <cmath>

#include <cstdio>
#include <cerrno>
#include <cstring>

#ifndef Q_OS_WIN
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// ========== RotatingCover 实现 ==========
RotatingCover::RotatingCover(QWidget *parent)
    : QWidget(parent)
    , m_angle(0)
    , m_accentColor(QColor(49, 194, 124))
    , m_size(300)
{
    setFixedSize(m_size, m_size);
    generateBasePixmap();
}

void RotatingCover::setAngle(double angle)
{
    m_angle = angle;
    update();
}

void RotatingCover::setAccentColor(const QColor &color)
{
    m_accentColor = color;
    generateBasePixmap();
    update();
}

void RotatingCover::setCoverImage(const QPixmap &pixmap)
{
    m_coverImage = pixmap;
    generateBasePixmap();
    update();
}

void RotatingCover::generateBasePixmap()
{
    m_basePixmap = QPixmap(m_size, m_size);
    m_basePixmap.fill(Qt::transparent);

    QPainter painter(&m_basePixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 绘制黑胶唱片风格的圆形背景
    int cx = m_size / 2;
    int cy = m_size / 2;
    int radius = m_size / 2 - 4;

    // 外圈 - 深色渐变背景
    QRadialGradient outerGrad(cx, cy, radius);
    QColor darkOuter = m_accentColor.darker(300);
    darkOuter.setAlpha(255);
    outerGrad.setColorAt(0.0, m_accentColor.lighter(130));
    outerGrad.setColorAt(0.3, m_accentColor);
    outerGrad.setColorAt(0.7, m_accentColor.darker(150));
    outerGrad.setColorAt(1.0, darkOuter);
    painter.setBrush(outerGrad);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPoint(cx, cy), radius, radius);

    // 唱片纹路 - 同心圆环
    painter.setPen(QPen(QColor(255, 255, 255, 12), 1));
    painter.setBrush(Qt::NoBrush);
    for (int r = 60; r < radius - 10; r += 8) {
        painter.drawEllipse(QPoint(cx, cy), r, r);
    }

    // 中心区域 - 绘制专辑封面
    int coverRadius = 55;
    if (!m_coverImage.isNull()) {
        // 圆形裁剪路径
        QPainterPath clipPath;
        clipPath.addEllipse(QPoint(cx, cy), coverRadius, coverRadius);
        painter.setClipPath(clipPath);

        // 将封面图片缩放并居中绘制到中心圆区域
        QPixmap scaledCover = m_coverImage.scaled(coverRadius * 2, coverRadius * 2,
                                                   Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        int drawX = cx - scaledCover.width() / 2;
        int drawY = cy - scaledCover.height() / 2;
        painter.drawPixmap(drawX, drawY, scaledCover);

        // 取消裁剪
        painter.setClipping(false);
    } else {
        // 没有封面时，绘制默认中心圆
        QRadialGradient centerGrad(cx, cy, coverRadius);
        centerGrad.setColorAt(0.0, QColor(255, 255, 255, 40));
        centerGrad.setColorAt(0.8, QColor(255, 255, 255, 15));
        centerGrad.setColorAt(1.0, QColor(0, 0, 0, 60));
        painter.setBrush(centerGrad);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPoint(cx, cy), coverRadius, coverRadius);

        // 音乐符号
        painter.setPen(QPen(QColor(255, 255, 255, 50), 2));
        QFont musicFont;
        musicFont.setPixelSize(28);
        painter.setFont(musicFont);
        painter.drawText(QRect(cx - 25, cy - 20, 50, 40), Qt::AlignCenter, "♪");
    }

    // 中心圆环线（在封面之上）
    painter.setClipping(false);
    painter.setPen(QPen(QColor(255, 255, 255, 30), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPoint(cx, cy), coverRadius, coverRadius);

    // 中心孔
    painter.setBrush(QColor(10, 22, 40));
    painter.setPen(QPen(QColor(255, 255, 255, 30), 1));
    painter.drawEllipse(QPoint(cx, cy), 8, 8);

    painter.end();

    // 预缩放（已在paintEvent中处理）
}

void RotatingCover::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 旋转绘制
    QTransform transform;
    transform.translate(width() / 2.0, height() / 2.0);
    transform.rotate(m_angle);
    transform.translate(-width() / 2.0, -height() / 2.0);

    painter.setTransform(transform);
    painter.drawPixmap(0, 0, m_basePixmap);
}

// ========== MainWindow 实现 ==========
MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWindow)
    , mplayerProcess(nullptr)
    , m_keyboard(nullptr)
    , currentSongIndex(0)
    , currentLyricIndex(-1)
    , lyricsMode(0)
    , isPlaying(false)
    , songEnded(false)
    , isSliderPressed(false)
    , isMuted(false)
    , loopMode(0)
    , themeMode(0)
    , eqMode(0)
    , totalDuration(0)
    , currentPosition(0)
    , lastPosition(0)
    , coverAngle(0)
    , accentColor(49, 194, 124)
    , currentTabIndex(3) // 默认显示播放页
    , longPressTriggered(false)
    , playbackSpeed(1.0)
    , sleepTimer(nullptr)
    , sleepRemaining(0)
    , sleepDuration(0)
    , sortMode(0)
    , playHistoryIdx(-1)
    , settingsLoopCombo(nullptr)
    , settingsEqCombo(nullptr)
    , settingsSpeedCombo(nullptr)
    , settingsLyricsCombo(nullptr)
    , settingsThemeCombo(nullptr)
    , volumePopup(nullptr)
    , volumeSlider(nullptr)
    , volumeLabel(nullptr)
{
    std::srand(std::time(nullptr));

    // 初始化UI（由 .ui 文件生成）
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint);
    setFixedSize(1024, 600);

    // 收集 Tab 按钮指针
    tabButtons = {ui->tabBtn_0, ui->tabBtn_1, ui->tabBtn_2,
                  ui->tabBtn_3, ui->tabBtn_4};

    // 创建音量弹出面板（需要绝对定位，不在 .ui 中）
    volumePopup = new QWidget(ui->playerPage);
    volumePopup->setFixedSize(80, 260);
    volumePopup->setVisible(false);
    volumePopup->setStyleSheet("QWidget { background: rgba(15,15,20,240);"
        "border: 1px solid rgba(255,255,255,30); border-radius: 12px; }");
    volumePopup->move(594, 280);

    QVBoxLayout *volPopupLayout = new QVBoxLayout(volumePopup);
    volPopupLayout->setContentsMargins(10, 15, 10, 15);
    volPopupLayout->setSpacing(10);

    volumeSlider = new QSlider(Qt::Vertical, volumePopup);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(25);  // 实际音量75%: 显示值 = 100 - sliderValue
    volumeSlider->setMinimumSize(30, 140);
    volumeSlider->setStyleSheet(
        "QSlider::groove:vertical { width: 4px; background: rgba(255,255,255,30); border-radius: 2px; }"
        "QSlider::handle:vertical { background: #31C27C; width: 16px; height: 16px; margin: 0 -6px; border-radius: 8px; }"
        "QSlider::sub-page:vertical { background: rgba(255,255,255,20); border-radius: 2px; }"
        "QSlider::add-page:vertical { background: #31C27C; border-radius: 2px; }");
    connect(volumeSlider, &QSlider::valueChanged, [this](int val) {
        int vol = 100 - val;  // 反转: slider底部=100%音量, 顶部=0%音量
        sendCommand(QString("volume %1 1").arg(vol));
        if (volumeLabel) volumeLabel->setText(QString("%1%").arg(vol));
    });

    volumeLabel = new QLabel("75%", volumePopup);
    volumeLabel->setAlignment(Qt::AlignCenter);
    volumeLabel->setStyleSheet("color: rgba(255,255,255,200); font-size: 14px; font-weight: bold;"
        "background: transparent; border: none;");

    QPushButton *muteBtn = new QPushButton("静音", volumePopup);
    muteBtn->setFixedSize(60, 30);
    muteBtn->setStyleSheet("QPushButton { background: transparent; border: none;"
        "font-size: 13px; color: rgba(255,255,255,180); }"
        "QPushButton:hover { color: #31C27C; }"
        "QPushButton:pressed { color: #31C27C; }");
    connect(muteBtn, &QPushButton::clicked, [this, muteBtn]() {
        isMuted = !isMuted;
        if (isMuted) {
            sendCommand("volume 0 1");
            muteBtn->setText("取消静音");
            ui->volumeBtn->setText("静音");
        } else {
            sendCommand(QString("volume %1 1").arg(100 - volumeSlider->value()));
            muteBtn->setText("静音");
            ui->volumeBtn->setText("音量");
        }
    });

    volPopupLayout->addWidget(volumeSlider, 1, Qt::AlignHCenter);
    volPopupLayout->addWidget(volumeLabel, 0, Qt::AlignHCenter);
    volPopupLayout->addWidget(muteBtn, 0, Qt::AlignHCenter);

    // 创建播放列表面板（绝对定位浮动面板）
    playlistPanel = new QWidget(ui->playerPage);
    playlistPanel->setFixedSize(324, 460);
    playlistPanel->move(700, 0);
    playlistPanel->setVisible(false);
    playlistPanel->setStyleSheet("QWidget { background: rgba(20,20,25,240);"
        "border-left: 1px solid rgba(255,255,255,20); }");

    QVBoxLayout *plLayout = new QVBoxLayout(playlistPanel);
    plLayout->setContentsMargins(16, 16, 16, 8);

    QLabel *plTitle = new QLabel("播放队列", playlistPanel);
    plTitle->setStyleSheet("color: white; font-size: 15px; font-weight: bold;");
    plInfoLabel = new QLabel("共 0 首歌曲", playlistPanel);
    plInfoLabel->setStyleSheet("color: rgba(255,255,255,80); font-size: 12px;");

    playlistList = new QListWidget(playlistPanel);
    playlistList->setStyleSheet(
        "QListWidget { background: transparent; border: none; color: rgba(255,255,255,200); }"
        "QListWidget::item { padding: 12px 16px; border-bottom: 1px solid rgba(255,255,255,10); font-size: 13px; }"
        "QListWidget::item:selected { background: rgba(49,194,124,30); color: #31C27C; }");
    playlistList->installEventFilter(this);

    plLayout->addWidget(plTitle);
    plLayout->addWidget(plInfoLabel);
    plLayout->addWidget(playlistList, 1);

    connect(playlistList, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item) {
        int row = playlistList->row(item);
        if (row >= 0 && row < songList.size()) playSong(row);
    });

    // 初始化动态UI内容
    initDynamicUI();

    // 连接信号槽
    setupConnections();

    // 加载配置
    loadConfig();

    // 扫描歌曲
    scanSongs();

    // 加载收藏
    loadFavorites();

    // 加载最近播放
    loadRecentPlayed();

    // 初始化mplayer
    initMplayer();

    // 初始化软键盘
    m_keyboard = new CustomKeyboard(this);
    m_keyboard->hide();
    connect(m_keyboard, &CustomKeyboard::enterPressed, [this]() {
        m_keyboard->hideKeyboard();
    });

    // 定时器
    progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, this, &MainWindow::updateProgress);
    progressTimer->start(300);

    coverAnimTimer = new QTimer(this);
    connect(coverAnimTimer, &QTimer::timeout, this, &MainWindow::updateCoverRotation);
    coverAnimTimer->start(50);

    // 睡眠定时器
    sleepTimer = new QTimer(this);
    connect(sleepTimer, &QTimer::timeout, [this]() {
        if (sleepRemaining > 0) {
            sleepRemaining--;
            updateSleepDisplay();
            if (sleepRemaining <= 0) {
                if (isPlaying) {
                    on_playBtn_clicked();
                }
                sleepDuration = 0;
                sleepTimer->stop();
                ui->sleepBtn->setText("睡眠");
            }
        }
    });

    // 加载播放次数
    loadPlayCounts();

    // #5修复: 刷新首页每日推荐和最近播放
    refreshHomePage();

    // 切换到播放页
    ui->stackedWidget->setCurrentIndex(currentTabIndex);
    updateTabHighlight(currentTabIndex);
}

MainWindow::~MainWindow()
{
    saveConfig();
    saveRecentPlayed();
    savePlayCounts();
    if (mplayerProcess && mplayerProcess->state() != QProcess::NotRunning) {
        sendCommand("quit");
        mplayerProcess->waitForFinished(2000);
        if (mplayerProcess->state() != QProcess::NotRunning) {
            mplayerProcess->kill();
            mplayerProcess->waitForFinished(3000);  // #20: 防止僵尸进程
        }
    }
    delete ui;
}

// ============ 信号槽连接 ============
void MainWindow::setupConnections()
{
    // Tab 按钮
    for (int i = 0; i < tabButtons.size(); i++) {
        int idx = i;
        connect(tabButtons[i], &QPushButton::clicked, [this, idx]() {
            ui->stackedWidget->setCurrentIndex(idx);
            currentTabIndex = idx;
            updateTabHighlight(idx);
            // MINOR-2: 切换到首页时刷新推荐和最近播放
            if (idx == 0) refreshHomePage();
        });
    }

    // 首页卡片播放按钮
    connect(ui->cardPlayBtn, &QPushButton::clicked, [this]() {
        if (!isPlaying && !songList.isEmpty()) {
            playSong(currentSongIndex);
        }
        ui->stackedWidget->setCurrentIndex(3);
        currentTabIndex = 3;
        updateTabHighlight(3);
    });

    // 歌曲页搜索框
    ui->searchEdit->installEventFilter(this);
    connect(ui->searchEdit, &QLineEdit::textChanged, [this](const QString &text) {
        for (int i = 0; i < ui->songsListWidget->count(); i++) {
            if (text.isEmpty()) {
                ui->songsListWidget->item(i)->setHidden(false);
                continue;
            }
            QString itemText = ui->songsListWidget->item(i)->text();
            // 中文搜索：直接匹配显示文本
            bool match = itemText.contains(text, Qt::CaseInsensitive);
            // 也搜索歌曲名和歌手名（去除显示文本中的特殊字符）
            if (!match && i < songTitles.size()) {
                match = songTitles[i].contains(text, Qt::CaseInsensitive);
            }
            if (!match && i < songArtists.size()) {
                match = songArtists[i].contains(text, Qt::CaseInsensitive);
            }
            // 拼音首字母搜索
            if (!match && i < songNames.size()) {
                QString pinyin = getPinyinInitials(songTitles[i] + songArtists[i]);
                match = pinyin.contains(text.toLower(), Qt::CaseInsensitive);
            }
            ui->songsListWidget->item(i)->setHidden(!match);
        }
    });

    // 排序按钮
    QVector<QPushButton*> sortBtns = {ui->sortDefaultBtn, ui->sortNameBtn,
                                       ui->sortArtistBtn, ui->sortRecentBtn, ui->sortCountBtn};
    connect(ui->sortDefaultBtn, &QPushButton::clicked, [this, sortBtns]() {
        for (auto *b : sortBtns) b->setChecked(b == ui->sortDefaultBtn);
        sortSongs(0);
    });
    connect(ui->sortNameBtn, &QPushButton::clicked, [this, sortBtns]() {
        for (auto *b : sortBtns) b->setChecked(b == ui->sortNameBtn);
        sortSongs(1);
    });
    connect(ui->sortArtistBtn, &QPushButton::clicked, [this, sortBtns]() {
        for (auto *b : sortBtns) b->setChecked(b == ui->sortArtistBtn);
        sortSongs(2);
    });
    connect(ui->sortRecentBtn, &QPushButton::clicked, [this, sortBtns]() {
        for (auto *b : sortBtns) b->setChecked(b == ui->sortRecentBtn);
        sortSongs(3);
    });
    connect(ui->sortCountBtn, &QPushButton::clicked, [this, sortBtns]() {
        for (auto *b : sortBtns) b->setChecked(b == ui->sortCountBtn);
        sortSongs(4);
    });

    // 歌曲列表双击
    ui->songsListWidget->installEventFilter(this);
    connect(ui->songsListWidget, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item) {
        int row = ui->songsListWidget->row(item);
        if (row >= 0 && row < songList.size()) {
            playSong(row);
            ui->stackedWidget->setCurrentIndex(3);
            currentTabIndex = 3;
            updateTabHighlight(3);
        }
    });

    // 歌词双击跳转
    connect(ui->lyricsList, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item) {
        int row = ui->lyricsList->row(item);
        if (row >= 0 && row < filteredLyrics.size()) {
            int timeMs = filteredLyrics[row].timeMs;
            // 跳转到指定时间（秒）
            sendCommand(QString("seek %1 2").arg(timeMs / 1000));
            // 更新进度条
            if (totalDuration > 0) {
                ui->progressSlider->setValue(timeMs / 1000);
            }
            // 更新当前时间显示
            ui->curTimeLabel->setText(formatTime(timeMs / 1000));
            qDebug() << "Lyrics jump to:" << timeMs << "ms";
        }
    });

    // 收藏页
    connect(ui->playAllBtn, &QPushButton::clicked, [this]() {
        if (!favorites.isEmpty() && !songList.isEmpty()) {
            for (int i = 0; i < songNames.size(); i++) {
                if (favorites.contains(songNames[i])) {
                    playSong(i);
                    ui->stackedWidget->setCurrentIndex(3);
                    currentTabIndex = 3;
                    updateTabHighlight(3);
                    break;
                }
            }
        }
    });
    ui->favoritesListWidget->installEventFilter(this);
    connect(ui->favoritesListWidget, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item) {
        int row = ui->favoritesListWidget->row(item);
        int songIdx = 0;
        int count = 0;
        for (int i = 0; i < songNames.size(); i++) {
            if (favorites.contains(songNames[i])) {
                if (count == row) { songIdx = i; break; }
                count++;
            }
        }
        playSong(songIdx);
        ui->stackedWidget->setCurrentIndex(3);
        currentTabIndex = 3;
        updateTabHighlight(3);
    });

    // 播放控制按钮
    connect(ui->likeBtn, &QPushButton::clicked, [this]() { on_likeBtn_clicked(); });
    connect(ui->loopBtn, &QPushButton::clicked, [this]() { on_loopBtn_clicked(); });
    connect(ui->prevBtn, &QPushButton::clicked, [this]() { playPrev(); });
    connect(ui->playBtn, &QPushButton::clicked, [this]() { on_playBtn_clicked(); });
    connect(ui->nextBtn, &QPushButton::clicked, [this]() { playNext(); });
    connect(ui->volumeBtn, &QPushButton::clicked, [this]() {
        volumePopup->setVisible(!volumePopup->isVisible());
    });

    // 进度条
    connect(ui->progressSlider, &QSlider::sliderPressed, [this]() { isSliderPressed = true; });
    connect(ui->progressSlider, &QSlider::sliderReleased, [this]() {
        isSliderPressed = false;
        if (totalDuration > 0) {
            int targetSec = (ui->progressSlider->value() * totalDuration) / 1000;
            sendCommand(QString("seek %1 2").arg(targetSec));
        }
    });

    // 功能按钮
    connect(ui->modeBtn, &QPushButton::clicked, [this]() {
        themeMode = (themeMode + 1) % 2;
        setThemeMode(themeMode);
    });
    connect(ui->lyricsBtn, &QPushButton::clicked, [this]() { setLyricsMode((lyricsMode + 1) % 4); });
    connect(ui->eqBtn, &QPushButton::clicked, [this]() {
        eqMode = (eqMode + 1) % 5;
        setEqMode(eqMode);
    });
    connect(ui->speedBtn, &QPushButton::clicked, [this]() {
        static const double speeds[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
        int idx = 0;
        for (int i = 0; i < 6; i++) {
            if (qAbs(playbackSpeed - speeds[i]) < 0.01) {
                idx = (i + 1) % 6;
                break;
            }
        }
        setPlaybackSpeed(speeds[idx]);
    });
    connect(ui->sleepBtn, &QPushButton::clicked, [this]() {
        static const int durations[] = {0, 15, 30, 60};
        int idx = 0;
        for (int i = 0; i < 4; i++) {
            if (sleepDuration == durations[i]) {
                idx = (i + 1) % 4;
                break;
            }
        }
        // #13修复: 切换前先停止旧定时器，避免残留状态
        sleepTimer->stop();
        sleepRemaining = 0;
        sleepDuration = durations[idx];
        if (sleepDuration > 0) {
            sleepRemaining = sleepDuration * 60;
            sleepTimer->start(1000);
            ui->sleepBtn->setText(QString("%1分").arg(sleepDuration));
        } else {
            sleepRemaining = 0;
            sleepTimer->stop();
            ui->sleepBtn->setText("睡眠");
        }
    });
    connect(ui->playlistBtn, &QPushButton::clicked, [this]() {
        playlistPanel->setVisible(!playlistPanel->isVisible());
    });

    // 长按定时器
    longPressTimer = new QTimer(this);
    longPressTimer->setSingleShot(true);
    connect(longPressTimer, &QTimer::timeout, [this]() {
        longPressTriggered = true;
        QPoint currentPos = QCursor::pos();
        if ((currentPos - mapToGlobal(longPressStartPos)).manhattanLength() > 30) {
            longPressTriggered = false;
            return;
        }
        QListWidget *lists[] = { ui->songsListWidget, ui->favoritesListWidget, playlistList };
        for (QListWidget *list : lists) {
            if (!list || !list->isVisible()) continue;
            QPoint localPos = list->mapFromGlobal(currentPos);
            if (list->itemAt(localPos)) {
                list->setCurrentItem(list->itemAt(localPos));
                showLongPressMenu(list, currentPos);
                break;
            }
        }
        longPressTriggered = false;
    });
}

// ============ 动态UI初始化 ============
void MainWindow::initDynamicUI()
{
    // === 首页：功能快捷入口 ===
    QHBoxLayout *quickLayout = qobject_cast<QHBoxLayout*>(ui->quickRow->layout());
    if (quickLayout) {
        struct QuickEntry { QString icon; QString title; int tabIndex; };
        QVector<QuickEntry> entries = {
            {"歌", "全部歌曲", 1},
            {"心", "我的收藏", 2},
            {"设", "设置", 4}
        };

        for (const QuickEntry &entry : entries) {
            QWidget *quickCard = new QWidget(ui->quickRow);
            quickCard->setFixedHeight(70);
            quickCard->setStyleSheet("QWidget { background: rgba(255,255,255,8); border-radius: 10px; }"
                "QWidget:hover { background: rgba(255,255,255,15); }");
            QVBoxLayout *qLayout = new QVBoxLayout(quickCard);
            qLayout->setContentsMargins(16, 12, 16, 12);

            QLabel *qIcon = new QLabel(entry.icon, quickCard);
            qIcon->setStyleSheet("font-size: 18px; font-weight: bold; color: #31C27C; background: transparent;");
            qIcon->setAlignment(Qt::AlignCenter);

            QLabel *qTitle = new QLabel(entry.title, quickCard);
            qTitle->setStyleSheet("color: rgba(255,255,255,180); font-size: 12px; background: transparent;");
            qTitle->setAlignment(Qt::AlignCenter);

            qLayout->addWidget(qIcon);
            qLayout->addWidget(qTitle);

            // #19修复: 布局完成后设置覆盖按钮尺寸，适配实际卡片宽度
            QPushButton *overlay = new QPushButton("", quickCard);
            overlay->setStyleSheet("QPushButton { background: transparent; border: none; }");
            overlay->setFixedSize(200, 70);  // 直接设置固定尺寸，避免 QTimer 悬空指针
            int tabIdx = entry.tabIndex;
            connect(overlay, &QPushButton::clicked, [this, tabIdx]() {
                ui->stackedWidget->setCurrentIndex(tabIdx);
                currentTabIndex = tabIdx;
                updateTabHighlight(tabIdx);
            });

            quickLayout->addWidget(quickCard);
        }
    }

    // === #5修复: 首页：每日推荐（随机推荐歌曲） ===
    QHBoxLayout *recLayout = qobject_cast<QHBoxLayout*>(ui->recommendGrid->layout());
    if (recLayout) {
        // 延迟到scanSongs后填充，先创建占位，后续由refreshHomePage()更新
    }

    // === #5修复: 首页：最近播放 ===
    QHBoxLayout *recentLayout = qobject_cast<QHBoxLayout*>(ui->recentGrid->layout());
    if (recentLayout) {
        // 延迟到scanSongs后填充，先创建占位，后续由refreshHomePage()更新
    }

    // === 首页：问候语（定时刷新） ===
    updateGreeting();
    QTimer *greetingTimer = new QTimer(this);
    connect(greetingTimer, &QTimer::timeout, this, &MainWindow::updateGreeting);
    greetingTimer->start(30000);  // 每30秒刷新一次

    // === 设置页：动态生成设置项 ===
    QVBoxLayout *settingsContentLayout = qobject_cast<QVBoxLayout*>(ui->settingsContent->layout());
    if (settingsContentLayout) {
        auto addSettingRow = [&](const QString &label, const QStringList &options, int current, std::function<void(int)> callback) -> QComboBox* {
            QWidget *row = new QWidget(ui->settingsContent);
            row->setFixedHeight(48);
            QHBoxLayout *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(16, 0, 16, 0);
            row->setStyleSheet("QWidget { background: rgba(255,255,255,5); border-radius: 8px; }");

            QLabel *lbl = new QLabel(label, row);
            lbl->setStyleSheet("color: rgba(255,255,255,180); font-size: 14px;");

            QComboBox *combo = new QComboBox(row);
            combo->addItems(options);
            combo->setCurrentIndex(current);
            combo->setFixedSize(120, 32);
            combo->setStyleSheet("QComboBox { background: rgba(255,255,255,10); border: 1px solid rgba(255,255,255,30);"
                "border-radius: 6px; color: white; padding: 4px 8px; font-size: 13px; }"
                "QComboBox::drop-down { border: none; }"
                "QComboBox QAbstractItemView { background: #1a1a2e; color: white; selection-background-color: #31C27C; }");
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), callback);

            rowLayout->addWidget(lbl);
            rowLayout->addStretch();
            rowLayout->addWidget(combo);
            settingsContentLayout->addWidget(row);
            return combo;
        };

        settingsLoopCombo = addSettingRow("循环模式", {"列表循环", "单曲循环", "随机播放"}, loopMode,
            [this](int idx) { setLoopMode(idx); });
        settingsEqCombo = addSettingRow("均衡器", {"默认", "摇滚", "流行", "古典", "低音"}, eqMode,
            [this](int idx) { setEqMode(idx); });

        int speedIdx = 2;
        static const double speedValues[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
        for (int i = 0; i < 6; i++) {
            if (qAbs(playbackSpeed - speedValues[i]) < 0.01) { speedIdx = i; break; }
        }
        settingsSpeedCombo = addSettingRow("播放速度", {"0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x"}, speedIdx,
            [this](int idx) { setPlaybackSpeed(speedValues[idx]); });

        // 音量滑块行
        QWidget *volRow = new QWidget(ui->settingsContent);
        volRow->setFixedHeight(48);
        QHBoxLayout *volLayout = new QHBoxLayout(volRow);
        volLayout->setContentsMargins(16, 0, 16, 0);
        volRow->setStyleSheet("QWidget { background: rgba(255,255,255,5); border-radius: 8px; }");
        QLabel *volLabel = new QLabel("音量", volRow);
        volLabel->setStyleSheet("color: rgba(255,255,255,180); font-size: 14px;");
        QSlider *volSlider = new QSlider(Qt::Horizontal, volRow);
        volSlider->setRange(0, 100);
        volSlider->setValue(volumeSlider ? (100 - volumeSlider->value()) : 75);
        volSlider->setMinimumSize(200, 30);
        volSlider->setTracking(true);
        volSlider->setStyleSheet(
            "QSlider::groove:horizontal { height: 8px; background: rgba(255,255,255,30); border-radius: 4px; }"
            "QSlider::handle:horizontal { background: #31C27C; width: 24px; height: 24px; margin: -8px 0; border-radius: 12px; }"
            "QSlider::sub-page:horizontal { background: #31C27C; border-radius: 4px; }");
        connect(volSlider, &QSlider::valueChanged, [this](int val) { setVolume(val); });
        volLayout->addWidget(volLabel);
        volLayout->addWidget(volSlider, 1);
        settingsContentLayout->addWidget(volRow);

        // 界面设置标题
        QLabel *uiSection = new QLabel("界面设置", ui->settingsContent);
        uiSection->setStyleSheet("color: #31C27C; font-size: 14px; font-weight: bold;");
        settingsContentLayout->addWidget(uiSection);
        settingsContentLayout->addSpacing(8);

        settingsThemeCombo = addSettingRow("主题", {"深色", "浅色"}, themeMode,
            [this](int idx) { setThemeMode(idx); });
        settingsLyricsCombo = addSettingRow("歌词模式", {"双语", "中文", "英文", "隐藏"}, lyricsMode,
            [this](int idx) { setLyricsMode(idx); });
    }
}

// ============ Tab 栏高亮 ============
void MainWindow::updateTabHighlight(int idx)
{
    for (int i = 0; i < tabButtons.size(); i++) {
        if (i == idx) {
            tabButtons[i]->setStyleSheet(
                "QPushButton { background: transparent; border: none; color: #31C27C;"
                "  font-size: 13px; font-weight: bold; }");
        } else {
            // 根据主题设置未选中Tab颜色
            QString tabColor = (themeMode == 0) ? "rgba(255,255,255,100)" : "rgba(0,0,0,100)";
            tabButtons[i]->setStyleSheet(
                "QPushButton { background: transparent; border: none; color: " + tabColor + ";"
                "  font-size: 13px; }");
        }
    }
}

// ============ 按钮槽 ============
void MainWindow::on_playBtn_clicked()
{
    if (songList.isEmpty()) return;
    if (!isPlaying) {
        if (totalDuration == 0) playSong(currentSongIndex);
        else { sendCommand("pause"); isPlaying = true; ui->playBtn->setText("暂停"); }
    } else {
        sendCommand("pause"); isPlaying = false; ui->playBtn->setText("播放");
    }
    emit playStateChanged(isPlaying);
}

void MainWindow::on_volumeBtn_clicked()
{
    volumePopup->setVisible(!volumePopup->isVisible());
}

void MainWindow::on_likeBtn_clicked()
{
    if (currentSongIndex >= 0 && currentSongIndex < songNames.size())
        toggleFavorite(currentSongIndex);
}

void MainWindow::on_loopBtn_clicked()
{
    setLoopMode((loopMode + 1) % 3);
}

void MainWindow::toggleFavorite(int index)
{
    if (index < 0 || index >= songNames.size()) return;
    QString name = songNames[index];
    if (favorites.contains(name)) favorites.remove(name);
    else favorites.insert(name);
    saveFavorites();

    isLiked = favorites.contains(name);
    // #9修复: 样式随主题切换
    if (isLiked) {
        ui->likeBtn->setStyleSheet(
            "QPushButton { background: transparent; border: none; color: #e74c3c; font-size: 22px; padding: 6px; }");
    } else {
        QString unlikeColor = (themeMode == 0) ? "rgba(255,255,255,80)" : "rgba(0,0,0,80)";
        ui->likeBtn->setStyleSheet(
            "QPushButton { background: transparent; border: none; color: " + unlikeColor + "; font-size: 20px; padding: 6px; }");
    }

    // 刷新播放列表
    if (playlistList) {
        playlistList->clear();
        for (int i = 0; i < songTitles.size(); i++) {
            QString prefix = favorites.contains(songNames[i]) ? "♥ " : "";
            playlistList->addItem(prefix + songTitles[i] + " - " + songArtists[i]);
        }
        playlistList->setCurrentRow(currentSongIndex);
    }

    // 刷新收藏页
    if (ui->favoritesListWidget) {
        ui->favoritesListWidget->clear();
        for (const QString &favName : favorites) {
            QString displayTitle = favName;
            for (int i = 0; i < songNames.size(); i++) {
                if (songNames[i] == favName) {
                    displayTitle = songTitles[i] + " - " + songArtists[i];
                    break;
                }
            }
            ui->favoritesListWidget->addItem("♥ " + displayTitle);
        }
    }
}

// ============ 设置 ============
void MainWindow::setLoopMode(int mode)
{
    loopMode = mode;
    switch (mode) {
    case 0: ui->loopBtn->setText("列表循环"); break;
    case 1: ui->loopBtn->setText("单曲循环"); break;
    case 2: ui->loopBtn->setText("随机播放"); break;
    }
    // #17修复: 同步设置页ComboBox
    if (settingsLoopCombo && settingsLoopCombo->currentIndex() != mode)
        settingsLoopCombo->setCurrentIndex(mode);
}

void MainWindow::setThemeMode(int mode)
{
    themeMode = mode;
    if (ui->modeBtn) ui->modeBtn->setText(mode == 0 ? "深色" : "浅色");

    QString bgGrad, cardBg, textMain, textSub, accent, cardBorder;
    if (mode == 0) {
        bgGrad = "qlineargradient(x1:0,y1:0,x2:0.5,y2:1,"
            "stop:0 #0a1628, stop:0.3 #0d2137, stop:0.7 #122a3e, stop:1 #0f2530)";
        cardBg = "rgba(255,255,255,8)";
        textMain = "white";
        textSub = "rgba(255,255,255,120)";
        accent = "#31C27C";
        cardBorder = "rgba(255,255,255,10)";
    } else {
        bgGrad = "qlineargradient(x1:0,y1:0,x2:0.5,y2:1,"
            "stop:0 #f0f4f8, stop:0.3 #e8edf2, stop:0.7 #dde3ea, stop:1 #d5dbe3)";
        cardBg = "rgba(0,0,0,6)";
        textMain = "#1a1a2e";
        textSub = "rgba(0,0,0,120)";
        accent = "#0a8f50";
        cardBorder = "rgba(0,0,0,10)";
    }

    // 应用到各页面
    if (ui->homePage) {
        ui->homePage->setStyleSheet("background: " + bgGrad + ";");
        ui->greetingLabel->setStyleSheet("color: " + textMain + "; font-size: 22px; font-weight: bold;");
    }
    if (ui->songsPage) {
        ui->songsPage->setStyleSheet("background: " + bgGrad + ";");
        if (mode == 0) {
            ui->searchEdit->setStyleSheet("QLineEdit { background: rgba(255,255,255,10); border: 1px solid rgba(255,255,255,20);"
                "border-radius: 20px; color: white; padding: 0 16px; font-size: 14px; }"
                "QLineEdit:focus { border-color: #31C27C; }");
            ui->songsListWidget->setStyleSheet(
                "QListWidget { background: transparent; border: none; color: rgba(255,255,255,200); }"
                "QListWidget::item { padding: 14px 16px; border-bottom: 1px solid rgba(255,255,255,8); font-size: 14px; }"
                "QListWidget::item:selected { background: rgba(49,194,124,30); color: #31C27C; }"
                "QListWidget::item:hover { background: rgba(255,255,255,10); }");
        } else {
            ui->searchEdit->setStyleSheet("QLineEdit { background: rgba(0,0,0,6); border: 1px solid rgba(0,0,0,15);"
                "border-radius: 20px; color: #1a1a2e; padding: 0 16px; font-size: 14px; }"
                "QLineEdit:focus { border-color: #0a8f50; }");
            ui->songsListWidget->setStyleSheet(
                "QListWidget { background: transparent; border: none; color: rgba(0,0,0,180); }"
                "QListWidget::item { padding: 14px 16px; border-bottom: 1px solid rgba(0,0,0,8); font-size: 14px; }"
                "QListWidget::item:selected { background: rgba(49,194,124,20); color: #0a8f50; }"
                "QListWidget::item:hover { background: rgba(0,0,0,5); }");
        }
    }
    if (ui->favoritesPage) {
        ui->favoritesPage->setStyleSheet("background: " + bgGrad + ";");
        if (mode == 0) {
            ui->favoritesListWidget->setStyleSheet(
                "QListWidget { background: transparent; border: none; color: rgba(255,255,255,200); }"
                "QListWidget::item { padding: 14px 16px; border-bottom: 1px solid rgba(255,255,255,8); font-size: 14px; }"
                "QListWidget::item:selected { background: rgba(231,76,60,30); color: #e74c3c; }"
                "QListWidget::item:hover { background: rgba(255,255,255,10); }");
        } else {
            ui->favoritesListWidget->setStyleSheet(
                "QListWidget { background: transparent; border: none; color: rgba(0,0,0,180); }"
                "QListWidget::item { padding: 14px 16px; border-bottom: 1px solid rgba(0,0,0,8); font-size: 14px; }"
                "QListWidget::item:selected { background: rgba(231,76,60,20); color: #c0392b; }"
                "QListWidget::item:hover { background: rgba(0,0,0,5); }");
        }
    }
    if (ui->playerPage) ui->playerPage->setStyleSheet("background: " + bgGrad + ";");
    if (ui->settingsPage) ui->settingsPage->setStyleSheet("background: " + bgGrad + ";");

    if (ui->songTitleLabel) ui->songTitleLabel->setStyleSheet("color: " + textMain + "; font-weight: bold; font-size: 24px;");
    if (ui->songArtistLabel) ui->songArtistLabel->setStyleSheet("color: " + textSub + "; font-size: 14px;");
    if (ui->barNameLabel) ui->barNameLabel->setStyleSheet("color: " + textMain + "; font-size: 13px;");

    // UI-2修复: 播放页功能按钮颜色
    QString btnStyle = "QPushButton { background: transparent; border: none; font-size: 12px; color: " + textSub + "; padding: 6px 10px; }"
        "QPushButton:hover { color: " + accent + "; }"
        "QPushButton:pressed { color: " + accent + "; }";
    if (ui->modeBtn) ui->modeBtn->setStyleSheet(btnStyle);
    if (ui->lyricsBtn) ui->lyricsBtn->setStyleSheet(btnStyle);
    if (ui->eqBtn) ui->eqBtn->setStyleSheet(btnStyle);
    if (ui->speedBtn) ui->speedBtn->setStyleSheet(btnStyle);
    if (ui->sleepBtn) ui->sleepBtn->setStyleSheet(btnStyle);
    if (ui->playlistBtn) ui->playlistBtn->setStyleSheet(btnStyle);

    // UI-3修复: 歌词列表颜色
    if (ui->lyricsList) {
        if (mode == 0) {
            ui->lyricsList->setStyleSheet(
                "QListWidget { background: transparent; border: none; color: rgba(255,255,255,40); }"
                "QListWidget::item { padding: 8px 16px; font-size: 14px; }"
                "QListWidget::item:selected { color: #31C27C; font-weight: bold; }");
        } else {
            ui->lyricsList->setStyleSheet(
                "QListWidget { background: transparent; border: none; color: rgba(0,0,0,40); }"
                "QListWidget::item { padding: 8px 16px; font-size: 14px; }"
                "QListWidget::item:selected { color: #0a8f50; font-weight: bold; }");
        }
    }

    // UI-4修复: 底部控制栏颜色
    if (ui->playerBar) {
        if (mode == 0) {
            ui->playerBar->setStyleSheet("background: rgba(0,0,0,200); border-top: 1px solid rgba(255,255,255,10);");
        } else {
            ui->playerBar->setStyleSheet("background: rgba(255,255,255,200); border-top: 1px solid rgba(0,0,0,10);");
        }
    }

    // UI-5修复: 排序按钮栏颜色
    if (ui->sortLabel) {
        ui->sortLabel->setStyleSheet("color: " + textSub + "; font-size: 12px;");
    }
    QString sortBtnStyle = "QPushButton { background: transparent; border: none; font-size: 12px; color: " + textSub + "; padding: 6px 12px; border-radius: 12px; }"
        "QPushButton:checked { background: " + accent + "; color: white; }"
        "QPushButton:hover { color: " + accent + "; }";
    if (ui->sortDefaultBtn) ui->sortDefaultBtn->setStyleSheet(sortBtnStyle);
    if (ui->sortNameBtn) ui->sortNameBtn->setStyleSheet(sortBtnStyle);
    if (ui->sortArtistBtn) ui->sortArtistBtn->setStyleSheet(sortBtnStyle);
    if (ui->sortRecentBtn) ui->sortRecentBtn->setStyleSheet(sortBtnStyle);
    if (ui->sortCountBtn) ui->sortCountBtn->setStyleSheet(sortBtnStyle);

    // #17修复: 同步设置页ComboBox
    if (settingsThemeCombo && settingsThemeCombo->currentIndex() != mode)
        settingsThemeCombo->setCurrentIndex(mode);
}

void MainWindow::setLyricsMode(int mode)
{
    lyricsMode = mode;
    currentLyricIndex = -1;
    filteredLyrics.clear();
    if (ui->lyricsList) ui->lyricsList->clear();

    for (const LyricLine &l : lyrics) {
        bool show = false;
        switch (lyricsMode) {
        case 0: show = true; break;
        case 1: show = l.isChinese; break;
        case 2: show = !l.isChinese; break;
        case 3: show = false; break;
        }
        if (show) {
            filteredLyrics.append(l);
            if (ui->lyricsList) ui->lyricsList->addItem(l.text);
        }
    }
    // #17修复: 同步设置页ComboBox
    if (settingsLyricsCombo && settingsLyricsCombo->currentIndex() != mode)
        settingsLyricsCombo->setCurrentIndex(mode);
}

void MainWindow::setEqMode(int mode)
{
    eqMode = mode;
    switch (mode) {
    case 0: sendCommand("af_eq_set_bass=0"); break;
    case 1: sendCommand("af_eq_set_bass=8"); break;
    case 2: sendCommand("af_eq_set_bass=4"); break;
    case 3: sendCommand("af_eq_set_bass=-2"); break;
    case 4: sendCommand("af_eq_set_bass=12"); break;
    }
    if (ui->eqBtn) {
        switch (mode) {
        case 0: ui->eqBtn->setText("均衡"); break;
        case 1: ui->eqBtn->setText("摇滚"); break;
        case 2: ui->eqBtn->setText("流行"); break;
        case 3: ui->eqBtn->setText("古典"); break;
        case 4: ui->eqBtn->setText("低音"); break;
        }
    }
    // #17修复: 同步设置页ComboBox
    if (settingsEqCombo && settingsEqCombo->currentIndex() != mode)
        settingsEqCombo->setCurrentIndex(mode);
}

// ============ 歌词 ============
void MainWindow::loadLyrics(const QString &songPath)
{
    lyrics.clear();
    filteredLyrics.clear();
    currentLyricIndex = -1;
    if (ui->lyricsList) ui->lyricsList->clear();

    QFileInfo fi(songPath);
    QString baseName = fi.completeBaseName();
    QString lrcPath;

    QStringList possibleLrcNames;
    possibleLrcNames << baseName + ".lrc" << baseName + "歌词.lrc";

    for (const QString &name : possibleLrcNames) {
        QString testPath = fi.path() + "/" + name;
        if (QFile::exists(testPath)) { lrcPath = testPath; break; }
    }

    QFile file(lrcPath);
    if (lrcPath.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LyricLine placeholder;
        placeholder.timeMs = -1;
        placeholder.text = "暂无歌词";
        placeholder.isChinese = true;
        filteredLyrics.append(placeholder);
        if (ui->lyricsList) ui->lyricsList->addItem("暂无歌词");
        return;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");  // 强制UTF-8编码，解决中文乱码
    QRegExp regex("\\[(\\d{2}):(\\d{2})[.:](\\d{2,3})\\](.*)");
    QRegExp chineseReg("[\\x4e00-\\x9fa5]");

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        int pos = 0;
        while ((pos = regex.indexIn(line, pos)) != -1) {
            int min = regex.cap(1).toInt();
            int sec = regex.cap(2).toInt();
            int ms = regex.cap(3).toInt();
            if (regex.cap(3).length() == 2) ms *= 10;
            QString text = regex.cap(4).trimmed();

            LyricLine lyric;
            lyric.timeMs = min * 60000 + sec * 1000 + ms;
            lyric.text = text.isEmpty() ? "..." : text;
            lyric.isChinese = chineseReg.indexIn(text) >= 0;
            lyrics.append(lyric);
            pos += regex.matchedLength();
        }
    }
    file.close();

    // BUG-1修复: 空数组时 lyrics.size()-1 会下溢(unsigned)，必须先检查
    if (lyrics.size() > 1) {
        for (int i = 0; i < lyrics.size() - 1; i++)
            for (int j = i + 1; j < lyrics.size(); j++)
                if (lyrics[j].timeMs < lyrics[i].timeMs) qSwap(lyrics[i], lyrics[j]);
    }

    setLyricsMode(lyricsMode);
}

// ============ 配置 ============
void MainWindow::loadFavorites()
{
    favorites.clear();
    QString favPath = QDir::currentPath() + "/favorites.txt";
    QFile file(favPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&file);
    in.setCodec("UTF-8");
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) favorites.insert(line);
    }
    file.close();

    if (ui->favoritesListWidget) {
        ui->favoritesListWidget->clear();
        for (const QString &favName : favorites) {
            QString displayTitle = favName;
            for (int i = 0; i < songNames.size(); i++) {
                if (songNames[i] == favName) {
                    displayTitle = songTitles[i] + " - " + songArtists[i];
                    break;
                }
            }
            ui->favoritesListWidget->addItem("♥ " + displayTitle);
        }
    }
}

void MainWindow::saveFavorites()
{
    QString favPath = QDir::currentPath() + "/favorites.txt";
    QFile file(favPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    out.setCodec("UTF-8");
    for (const QString &name : favorites) out << name << "\n";
    file.close();
}

void MainWindow::loadConfig()
{
    QString configPath = QDir::currentPath() + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    currentSongIndex = settings.value("player/songIndex", 0).toInt();
    int vol = settings.value("player/volume", 75).toInt();
    loopMode = settings.value("player/loopMode", 0).toInt();
    themeMode = settings.value("ui/theme", 0).toInt();
    lyricsMode = settings.value("ui/lyricsMode", 0).toInt();
    eqMode = settings.value("player/eqMode", 0).toInt();
    playbackSpeed = settings.value("player/speed", 1.0).toDouble();

    if (volumeSlider) volumeSlider->setValue(100 - vol);
    setLoopMode(loopMode);
    setThemeMode(themeMode);
    setLyricsMode(lyricsMode);  // #14修复: 恢复歌词模式
    setEqMode(eqMode);
    setPlaybackSpeed(playbackSpeed);
}

void MainWindow::saveConfig()
{
    QString configPath = QDir::currentPath() + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    settings.setValue("player/songIndex", currentSongIndex);
    settings.setValue("player/volume", volumeSlider ? (100 - volumeSlider->value()) : 75);
    settings.setValue("player/loopMode", loopMode);
    settings.setValue("ui/theme", themeMode);
    settings.setValue("ui/lyricsMode", lyricsMode);
    settings.setValue("player/eqMode", eqMode);
    settings.setValue("player/speed", playbackSpeed);
}

void MainWindow::sendCommand(const QString &cmd)
{
    if (!mplayerProcess || mplayerProcess->state() == QProcess::NotRunning) {
        qDebug() << "Cannot send command - mplayer not running";
        return;
    }
    if (fifoPath.isEmpty()) {
        qDebug() << "FIFO path not set!";
        return;
    }
    FILE *fifo = fopen(fifoPath.toUtf8().constData(), "w");
    if (fifo) {
        fprintf(fifo, "%s\n", cmd.toUtf8().constData());
        fflush(fifo);
        fclose(fifo);
        qDebug() << "Sent command:" << cmd;
    } else {
        qDebug() << "Failed to open FIFO for writing:" << strerror(errno);
    }
}

void MainWindow::sendLoadFile(const QString &path)
{
    sendCommand(QString("loadfile \"%1\"").arg(path));
}

void MainWindow::updateProgress()
{
    if (!isPlaying || !mplayerProcess) return;
    sendCommand("get_time_pos");
    // #3修复: 每10秒查询一次暂停状态，防止UI与mplayer不同步
    static int syncCounter = 0;
    if (++syncCounter >= 33) {  // 33 * 300ms ≈ 10s
        syncCounter = 0;
        sendCommand("get_property pause");
    }
    // #1修复: get_time_length 只在歌曲切换时查询（在 playSong 中），不再每300ms重复
}

QString MainWindow::formatTime(int seconds)
{
    return QString("%1:%2").arg(seconds / 60, 2, 10, QChar('0')).arg(seconds % 60, 2, 10, QChar('0'));
}

// ============ 封面旋转动画 ============
void MainWindow::updateCoverRotation()
{
    if (!isPlaying) return;
    coverAngle += 0.8;
    if (coverAngle >= 360.0) coverAngle -= 360.0;
    if (ui->coverLabel) ui->coverLabel->setAngle(coverAngle);
}

// ============ 歌词同步滚动 ============
void MainWindow::updateLyricsHighlight(int currentTimeMs)
{
    if (filteredLyrics.isEmpty() || !ui->lyricsList) return;

    int newIndex = -1;
    for (int i = filteredLyrics.size() - 1; i >= 0; i--) {
        if (currentTimeMs >= filteredLyrics[i].timeMs) {
            newIndex = i;
            break;
        }
    }

    if (newIndex == currentLyricIndex) return;
    currentLyricIndex = newIndex;

    if (currentLyricIndex < 0 || currentLyricIndex >= filteredLyrics.size()) return;

    for (int i = 0; i < filteredLyrics.size(); i++) {
        QListWidgetItem *item = ui->lyricsList->item(i);
        if (!item) continue;

        if (i == currentLyricIndex) {
            item->setForeground(QBrush(QColor(255, 255, 255, 255)));
            QFont font = item->font();
            font.setPointSize(20);
            font.setBold(true);
            item->setFont(font);
        } else if (abs(i - currentLyricIndex) <= 3) {
            int alpha = 180 - abs(i - currentLyricIndex) * 40;
            item->setForeground(QBrush(QColor(255, 255, 255, alpha)));
            QFont font = item->font();
            font.setPointSize(16);
            font.setBold(false);
            item->setFont(font);
        } else {
            item->setForeground(QBrush(QColor(255, 255, 255, 40)));
            QFont font = item->font();
            font.setPointSize(16);
            font.setBold(false);
            item->setFont(font);
        }
    }

    if (currentLyricIndex >= 0 && currentLyricIndex < ui->lyricsList->count()) {
        ui->lyricsList->setCurrentRow(currentLyricIndex);
        ui->lyricsList->scrollTo(ui->lyricsList->model()->index(currentLyricIndex, 0),
            QAbstractItemView::PositionAtCenter);
    }
}

// ============ 长按事件过滤器 ============
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    QListWidget *list = qobject_cast<QListWidget*>(obj);
    QLineEdit *edit = qobject_cast<QLineEdit*>(obj);

    if (edit == ui->searchEdit) {
        if (event->type() == QEvent::FocusIn) {
            if (m_keyboard) {
                m_keyboard->setTargetLineEdit(ui->searchEdit);
                int kbY = height() - m_keyboard->height() - 56;
                m_keyboard->setGeometry(0, kbY, width(), m_keyboard->height());
                m_keyboard->showKeyboard();
            }
        } else if (event->type() == QEvent::FocusOut) {
            QTimer::singleShot(200, [this]() {
                if (m_keyboard && !ui->searchEdit->hasFocus()) {
                    QWidget *fw = QApplication::focusWidget();
                    if (!fw || !m_keyboard->isAncestorOf(fw)) {
                        m_keyboard->hideKeyboard();
                    }
                }
            });
        }
        return QWidget::eventFilter(obj, event);
    }

    if (!list) return QWidget::eventFilter(obj, event);

    if (list != ui->songsListWidget && list != ui->favoritesListWidget && list != playlistList)
        return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            longPressStartPos = me->pos();
            longPressTriggered = false;
            longPressTimer->start(600);
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        longPressTimer->stop();
        if (longPressTriggered) {
            longPressTriggered = false;
            return true;
        }
    } else if (event->type() == QEvent::MouseMove) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if ((me->pos() - longPressStartPos).manhattanLength() > 20) {
            longPressTimer->stop();
            longPressTriggered = false;
        }
    }

    return QWidget::eventFilter(obj, event);
}

// ============ 长按菜单 ============
void MainWindow::showLongPressMenu(QListWidget *list, const QPoint &pos)
{
    int row = list->currentRow();
    if (row < 0) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background: rgba(20, 20, 30, 240); border: 1px solid rgba(255,255,255,30);"
        "border-radius: 8px; color: white; padding: 4px; }"
        "QMenu::item { padding: 8px 24px; border-radius: 4px; }"
        "QMenu::item:selected { background: rgba(49, 194, 124, 80); }");

    if (list == ui->songsListWidget) {
        if (row < songList.size()) {
            QAction *playAct = menu.addAction("▶ 播放");
            QString songName = songNames[row];
            QAction *favAct = menu.addAction(favorites.contains(songName) ? "♥ 取消收藏" : "♡ 添加收藏");

            QAction *selected = menu.exec(pos);
            if (selected == playAct) {
                playSong(row);
                ui->stackedWidget->setCurrentIndex(3);
                currentTabIndex = 3;
                updateTabHighlight(3);
            } else if (selected == favAct) {
                toggleFavorite(row);
            }
        }
    } else if (list == ui->favoritesListWidget) {
        QAction *playAct = menu.addAction("▶ 播放");
        QAction *removeAct = menu.addAction("✕ 取消收藏");

        int songIdx = 0;
        int count = 0;
        for (int i = 0; i < songNames.size(); i++) {
            if (favorites.contains(songNames[i])) {
                if (count == row) { songIdx = i; break; }
                count++;
            }
        }

        QAction *selected = menu.exec(pos);
        if (selected == playAct) {
            playSong(songIdx);
            ui->stackedWidget->setCurrentIndex(3);
            currentTabIndex = 3;
            updateTabHighlight(3);
        } else if (selected == removeAct) {
            toggleFavorite(songIdx);
        }
    } else if (list == playlistList) {
        if (row < songList.size()) {
            QAction *playAct = menu.addAction("▶ 播放");
            QString songName = songNames[row];
            QAction *favAct = menu.addAction(favorites.contains(songName) ? "♥ 取消收藏" : "♡ 添加收藏");

            QAction *selected = menu.exec(pos);
            if (selected == playAct) {
                playSong(row);
            } else if (selected == favAct) {
                toggleFavorite(row);
            }
        }
    }
}

void MainWindow::loadRecentPlayed()
{
    recentPlayed.clear();
    QString path = QDir::currentPath() + "/recent.txt";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&file);
    in.setCodec("UTF-8");
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) recentPlayed.append(line);
    }
    file.close();
    while (recentPlayed.size() > 20) recentPlayed.removeLast();
}

void MainWindow::saveRecentPlayed()
{
    QString path = QDir::currentPath() + "/recent.txt";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    out.setCodec("UTF-8");
    for (const QString &name : recentPlayed) out << name << "\n";
    file.close();
}

void MainWindow::addToRecent(int index)
{
    if (index < 0 || index >= songNames.size()) return;
    QString name = songNames[index];
    recentPlayed.removeAll(name);
    recentPlayed.prepend(name);
    while (recentPlayed.size() > 20) recentPlayed.removeLast();
    saveRecentPlayed();
    // MINOR-2修复: 仅在首页可见时刷新，避免频繁重建UI卡片
    if (currentTabIndex == 0) {
        QTimer::singleShot(100, this, &MainWindow::refreshHomePage);
    }
}

// ============ 强调色提取 ============
QColor MainWindow::extractDominantColor(const QString &imagePath)
{
    QImage image(imagePath);
    if (image.isNull()) return QColor(49, 194, 124);

    image = image.scaled(50, 50, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    image = image.convertToFormat(QImage::Format_RGB32);

    QMap<int, int> hueCount;
    QMap<int, int> hueSumR, hueSumG, hueSumB;

    int greyCount = 0;
    int greySumR = 0, greySumG = 0, greySumB = 0;

    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < image.width(); x++) {
            QRgb pixel = image.pixel(x, y);
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);

            QColor color(r, g, b);
            int hue = color.hue();
            if (hue < 0) {
                // #21修复: 灰色像素不再跳过，计入灰色统计
                greyCount++;
                greySumR += r;
                greySumG += g;
                greySumB += b;
                continue;
            }

            int bucket = (hue / 10) * 10;
            hueCount[bucket]++;
            hueSumR[bucket] += r;
            hueSumG[bucket] += g;
            hueSumB[bucket] += b;
        }
    }

    int maxCount = 0;
    int maxBucket = 0;
    for (auto it = hueCount.begin(); it != hueCount.end(); ++it) {
        if (it.value() > maxCount) {
            maxCount = it.value();
            maxBucket = it.key();
        }
    }

    // #21修复: 如果灰色像素占多数，使用灰色作为主色调
    if (greyCount > maxCount && greyCount > 0) {
        int avgR = greySumR / greyCount;
        int avgG = greySumG / greyCount;
        int avgB = greySumB / greyCount;
        QColor result(avgR, avgG, avgB);
        int h, s, v;
        result.getHsv(&h, &s, &v);
        // 给灰色增加一点饱和度，使其在UI中更显眼
        s = qMin(255, s + 30);
        v = qBound(80, v, 200);
        result.setHsv(h, s, v);
        return result;
    }

    if (maxCount == 0) return QColor(49, 194, 124);

    int avgR = hueSumR[maxBucket] / maxCount;
    int avgG = hueSumG[maxBucket] / maxCount;
    int avgB = hueSumB[maxBucket] / maxCount;

    QColor result(avgR, avgG, avgB);
    int h, s, v;
    result.getHsv(&h, &s, &v);
    s = qMin(255, s + 60);
    v = qBound(80, v, 200);
    result.setHsv(h, s, v);

    return result;
}

void MainWindow::applyAccentColor(const QColor &color)
{
    accentColor = color;
    if (ui->coverLabel) ui->coverLabel->setAccentColor(color);

    if (ui->playerPage) {
        QColor dark = color.darker(400);
        ui->playerPage->setStyleSheet(
            QString("background: qlineargradient(x1:0,y1:0,x2:0.5,y2:1,"
                "stop:0 %1, stop:0.3 %2, stop:0.7 %3, stop:1 %4);")
            .arg(dark.name()).arg(dark.lighter(115).name())
            .arg(dark.lighter(130).name()).arg(dark.lighter(110).name()));
    }
}

// ============ 拼音首字母转换 ============
static const struct { wchar_t code; char letter; } pinyinBounds[] = {
    {0x554A, 'A'}, {0x82AD, 'B'}, {0x64E6, 'C'}, {0x642D, 'D'},
    {0x8BFE, 'E'}, {0x53D1, 'F'}, {0x560E, 'G'}, {0x54C8, 'H'},
    {0x4E0C, 'J'}, {0x5494, 'K'}, {0x5783, 'L'}, {0x5988, 'M'},
    {0x62FF, 'N'}, {0x5662, 'O'}, {0x8DB4, 'P'}, {0x4E03, 'Q'},
    {0x7136, 'R'}, {0x6492, 'S'}, {0x584C, 'T'}, {0x6316, 'W'},
    {0x5938, 'X'}, {0x538B, 'Y'}, {0x5E00, 'Z'}
};
static const int pinyinBoundsCount = sizeof(pinyinBounds) / sizeof(pinyinBounds[0]);

static char getPinyinInitial(wchar_t ch)
{
    if (ch < 0x4E00 || ch > 0x9FFF) return 0;
    char result = 'A';
    for (int i = 0; i < pinyinBoundsCount; i++) {
        if (ch < pinyinBounds[i].code) break;
        result = pinyinBounds[i].letter;
    }
    return result;
}

QString MainWindow::getPinyinInitials(const QString &text)
{
    QString result;
    for (int i = 0; i < text.length(); i++) {
        QChar ch = text.at(i);
        if (ch.unicode() >= 0x4E00 && ch.unicode() <= 0x9FFF) {
            char initial = getPinyinInitial(ch.unicode());
            if (initial) result += QChar(initial);
        } else if (ch.isLetterOrNumber()) {
            result += ch.toLower();
        }
    }
    return result;
}

// ============ 歌曲管理 ============
static void readLrcMetadata(const QString &audioPath, QString &title, QString &artist)
{
    title = QFileInfo(audioPath).completeBaseName();
    artist = "未知歌手";

    QFileInfo audioInfo(audioPath);
    QString baseName = audioInfo.completeBaseName();
    QStringList possibleLrcNames;
    possibleLrcNames << baseName + ".lrc" << baseName + "歌词.lrc";

    QString songDir = audioInfo.absolutePath();
    QString lrcPath;

    for (const QString &name : possibleLrcNames) {
        QString testPath = songDir + "/" + name;
        if (QFile::exists(testPath)) { lrcPath = testPath; break; }
    }

    if (lrcPath.isEmpty()) {
        qDebug() << "No LRC found for:" << audioInfo.fileName() << "using filename";
        return;
    }

    qDebug() << "Found LRC:" << lrcPath;

    QFile file(lrcPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open LRC file";
        return;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");  // 强制UTF-8编码，解决中文乱码
    QRegExp titleReg("\\[ti:(.*)\\]");
    QRegExp artistReg("\\[ar:(.*)\\]");
    // 匹配第一行歌词中的 "标题 - 歌手" 格式
    QRegExp firstLineReg("\\[\\d{2}:\\d{2}[.:]\\d{2,3}\\](.*)");

    bool foundArtist = false;
    bool foundTitle = false;
    bool checkedFirstLyric = false;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (titleReg.indexIn(line) != -1) {
            QString t = titleReg.cap(1).trimmed();
            if (!t.isEmpty()) { title = t; foundTitle = true; }
        }
        if (artistReg.indexIn(line) != -1) {
            QString a = artistReg.cap(1).trimmed();
            if (!a.isEmpty()) { artist = a; foundArtist = true; }
        }
        // 从第一行有效歌词中解析 "标题 - 歌手" 格式
        if (!checkedFirstLyric && firstLineReg.indexIn(line) != -1) {
            checkedFirstLyric = true;
            QString content = firstLineReg.cap(1).trimmed();
            if (!content.isEmpty()) {
                // 只在有 " - " 分隔符时解析，且必须是非歌词内容（通常第一行是元数据）
                int dashPos = content.indexOf(" - ");
                if (dashPos > 0 && dashPos < content.size() - 3) {
                    // 检查是否像元数据（不包含常见歌词特征）
                    QString possibleArtist = content.mid(dashPos + 3).trimmed();
                    if (!foundTitle) {
                        title = content.left(dashPos).trimmed();
                    }
                    if (!foundArtist && !possibleArtist.isEmpty()) {
                        artist = possibleArtist;
                    }
                }
            }
        }
    }
    file.close();
}

QString MainWindow::findSongDir()
{
    QString currentDir = QDir::currentPath();
    QDir dir1(currentDir + "/song");
    if (dir1.exists()) return dir1.absolutePath();

    QDir dir2(QFileInfo(currentDir).absolutePath() + "/song");
    if (dir2.exists()) return dir2.absolutePath();

    QString appDir = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
    QDir dir3(appDir + "/song");
    if (dir3.exists()) return dir3.absolutePath();

    QDir dir4(QFileInfo(appDir).absolutePath() + "/song");
    if (dir4.exists()) return dir4.absolutePath();

    return currentDir + "/song";
}

void MainWindow::scanSongs()
{
    songList.clear();
    songNames.clear();
    songTitles.clear();
    songArtists.clear();

    QString songDirPath = findSongDir();
    QDir dir(songDirPath);

    if (!dir.exists()) {
        qDebug() << "Song directory not found!";
        return;
    }

    QStringList filters;
    filters << "*.mp3" << "*.wav";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo &fi : fileList) {
        songList.append(fi.absoluteFilePath());
        songNames.append(fi.completeBaseName());

        QString title, artist;
        readLrcMetadata(fi.absoluteFilePath(), title, artist);
        songTitles.append(title);
        songArtists.append(artist);
    }

    if (ui->songsListWidget) {
        ui->songsListWidget->clear();
        for (int i = 0; i < songTitles.size(); i++) {
            ui->songsListWidget->addItem(songTitles[i] + " - " + songArtists[i]);
        }
    }

    if (playlistList) {
        playlistList->clear();
        for (int i = 0; i < songTitles.size(); i++) {
            QString prefix = favorites.contains(songNames[i]) ? "♥ " : "";
            playlistList->addItem(prefix + songTitles[i] + " - " + songArtists[i]);
        }
        if (currentSongIndex >= 0 && currentSongIndex < songList.size()) {
            playlistList->setCurrentRow(currentSongIndex);
        }
    }

    if (ui->homeNowPlaying && !songTitles.isEmpty()) {
        if (currentSongIndex >= 0 && currentSongIndex < songTitles.size()) {
            ui->homeNowPlaying->setText(songTitles[currentSongIndex]);
        }
    }

    // #7修复: 刷新收藏列表，移除不存在的歌曲
    QSet<QString> validFavorites;
    for (const QString &favName : favorites) {
        if (songNames.contains(favName)) {
            validFavorites.insert(favName);
        }
    }
    if (validFavorites.size() != favorites.size()) {
        favorites = validFavorites;
        saveFavorites();
    }
    if (ui->favoritesListWidget) {
        ui->favoritesListWidget->clear();
        for (const QString &favName : favorites) {
            int idx = songNames.indexOf(favName);
            QString displayTitle = (idx >= 0) ? (songTitles[idx] + " - " + songArtists[idx]) : favName;
            ui->favoritesListWidget->addItem("♥ " + displayTitle);
        }
    }
}

void MainWindow::playSong(int index, bool addToHistory)
{
    if (index < 0 || index >= songList.size()) return;

    currentSongIndex = index;
    songEnded = false;
    sendLoadFile(songList[index]);

    // 修复: 歌曲切换后恢复音量，防止声音丢失
    int vol = volumeSlider ? (100 - volumeSlider->value()) : 75;
    sendCommand(QString("volume %1 1").arg(vol));

    // #15修复: 记录播放历史（随机模式下「上一首」用）
    // NEW-1修复: addToHistory=false 时跳过追加（用于 playPrev 历史回退）
    if (addToHistory) {
        if (playHistoryIdx < playHistory.size() - 1) {
            playHistory.resize(playHistoryIdx + 1);
        }
        playHistory.append(index);
        playHistoryIdx = playHistory.size() - 1;
        while (playHistory.size() > 100) {
            playHistory.removeFirst();
            playHistoryIdx--;
        }
    }

    isPlaying = true;
    ui->playBtn->setText("暂停");

    QString title = songTitles[index];
    QString artist = songArtists[index];
    ui->songTitleLabel->setText(title);
    ui->songArtistLabel->setText(artist);
    ui->barNameLabel->setText(title);
    if (ui->homeNowPlaying) ui->homeNowPlaying->setText(title);
    plInfoLabel->setText(QString("共 %1 首 · 正在播放: %2").arg(songList.size()).arg(title));

    if (playlistList) playlistList->setCurrentRow(index);
    if (ui->songsListWidget) ui->songsListWidget->setCurrentRow(index);

    isLiked = favorites.contains(songNames[index]);

    currentPosition = 0;
    lastPosition = 0;
    totalDuration = 0;
    ui->progressSlider->setValue(0);
    ui->curTimeLabel->setText("00:00");
    ui->totalTimeLabel->setText("00:00");

    // #1修复: 歌曲切换时查询一次总时长
    sendCommand("get_time_length");

    loadLyrics(songList[index]);
    addToRecent(index);

    if (index < songNames.size()) {
        playCounts[songNames[index]] = playCounts.value(songNames[index], 0) + 1;
    }

    // 更新封面 - 只在song文件夹查找歌曲同名图片
    QFileInfo songInfo(songList[index]);
    QString coverPath;
    QStringList coverNames;
    coverNames << songInfo.completeBaseName() + ".jpg"
               << songInfo.completeBaseName() + ".png";

    for (const QString &name : coverNames) {
        QString testPath = songInfo.path() + "/" + name;
        if (QFile::exists(testPath)) { coverPath = testPath; break; }
    }

    if (!coverPath.isEmpty()) {
        QColor accent = extractDominantColor(coverPath);
        applyAccentColor(accent);

        // 封面加载：先尝试 QPixmap，失败则用 QImage 降级
        // 对大图片进行缩放，避免嵌入式设备内存不足
        QPixmap coverPixmap(coverPath);
        if (coverPixmap.isNull()) {
            QImage img(coverPath);
            if (!img.isNull()) {
                // 如果图片太大（超过500x500），先缩放再转换
                if (img.width() > 500 || img.height() > 500) {
                    img = img.scaled(474, 474, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                coverPixmap = QPixmap::fromImage(img);
            }
        } else {
            // QPixmap 加载成功，但如果太大也需要缩放
            if (coverPixmap.width() > 500 || coverPixmap.height() > 500) {
                coverPixmap = coverPixmap.scaled(474, 474, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }

        if (!coverPixmap.isNull() && ui->coverLabel) {
            ui->coverLabel->setCoverImage(coverPixmap);
        } else if (coverPixmap.isNull() && ui->coverLabel) {
            ui->coverLabel->setCoverImage(QPixmap());
        }

        if (!coverPixmap.isNull() && ui->barThumbLabel) {
            ui->barThumbLabel->setPixmap(makeRoundedPixmap(coverPixmap, 44));
        } else if (coverPixmap.isNull() && ui->barThumbLabel) {
            ui->barThumbLabel->clear();
            ui->barThumbLabel->setText("乐");
        }
    } else {
        // 没有歌曲专属封面，尝试使用默认 music.jpg
        QString defaultCover = songInfo.path() + "/music.jpg";
        if (QFile::exists(defaultCover)) {
            QPixmap defaultPixmap(defaultCover);
            if (defaultPixmap.isNull()) {
                QImage img(defaultCover);
                if (!img.isNull()) defaultPixmap = QPixmap::fromImage(img);
            }
            if (!defaultPixmap.isNull()) {
                QColor accent = extractDominantColor(defaultCover);
                applyAccentColor(accent);
                if (ui->coverLabel) ui->coverLabel->setCoverImage(defaultPixmap);
                if (ui->barThumbLabel) {
                    ui->barThumbLabel->setPixmap(makeRoundedPixmap(defaultPixmap, 44));
                }
            } else {
                // music.jpg 加载失败，清除旧封面
                if (ui->coverLabel) ui->coverLabel->setCoverImage(QPixmap());
                if (ui->barThumbLabel) {
                    ui->barThumbLabel->clear();
                    ui->barThumbLabel->setText("乐");
                }
            }
        } else {
            // music.jpg 不存在，清除旧封面
            if (ui->coverLabel) ui->coverLabel->setCoverImage(QPixmap());
            if (ui->barThumbLabel) {
                ui->barThumbLabel->clear();
                ui->barThumbLabel->setText("乐");
            }
        }
    }

    emit songChanged(index);
}

void MainWindow::playNext()
{
    if (songList.isEmpty()) return;
    if (loopMode == 2) {
        if (songList.size() > 1) {
            int idx;
            do { idx = std::rand() % songList.size(); } while (idx == currentSongIndex);
            playSong(idx);
        }
    } else {
        playSong((currentSongIndex + 1) % songList.size());
    }
}

void MainWindow::playPrev()
{
    if (songList.isEmpty()) return;
    if (loopMode == 2) {
        // #15修复: 随机模式下「上一首」回到历史记录
        if (playHistoryIdx > 0) {
            playHistoryIdx--;
            int idx = playHistory[playHistoryIdx];
            if (idx >= 0 && idx < songList.size()) {
                // NEW-1修复: 传 addToHistory=false 避免历史重复污染
                playSong(idx, false);
                return;
            }
        }
        // 没有历史可回退，随机选一首
        if (songList.size() > 1) {
            int idx;
            do { idx = std::rand() % songList.size(); } while (idx == currentSongIndex);
            playSong(idx);
        }
    } else {
        playSong((currentSongIndex - 1 + songList.size()) % songList.size());
    }
}

void MainWindow::togglePlay()
{
    on_playBtn_clicked();
}

void MainWindow::setVolume(int vol)
{
    // MINOR-3修复: volumeSlider->setValue 会触发 valueChanged 信号（已连接 sendCommand）
    // 所以这里只需设置滑块值，不需要再直接调用 sendCommand
    if (volumeSlider) volumeSlider->setValue(100 - vol);
}

// ============ mplayer ============
void MainWindow::initMplayer()
{
    QString songDir = findSongDir();
    QString appDir = QFileInfo(songDir).absolutePath();
    fifoPath = appDir + "/fifo_cmd";  // #2修复: 使用成员变量

    QDir appDirObj(appDir);
    if (!appDirObj.exists()) QDir().mkpath(appDir);

    QFile::remove(fifoPath);

#ifdef Q_OS_WIN
    qDebug() << "Windows: skipping mkfifo";
#else
    int result = mkfifo(fifoPath.toUtf8().constData(), 0666);
    if (result == -1) {
        qDebug() << "mkfifo failed:" << strerror(errno);
    }
#endif

    mplayerProcess = new QProcess(this);
    mplayerProcess->setProcessChannelMode(QProcess::MergedChannels);

    QStringList arguments;
    arguments << "-slave" << "-quiet" << "-idle"
              << "-input" << ("file=" + fifoPath);
    mplayerProcess->start("mplayer", arguments);

    if (!mplayerProcess->waitForStarted(3000)) {
        qDebug() << "Failed to start mplayer!";
        if (ui->songTitleLabel) ui->songTitleLabel->setText("mplayer启动失败");
        ui->playBtn->setEnabled(false);
        ui->prevBtn->setEnabled(false);
        ui->nextBtn->setEnabled(false);
        ui->progressSlider->setEnabled(false);
        return;
    }

    // 修复: mplayer 启动后设置初始音量
    int vol = volumeSlider ? (100 - volumeSlider->value()) : 75;
    sendCommand(QString("volume %1 1").arg(vol));

    connect(mplayerProcess, &QProcess::readyReadStandardOutput,
            this, &MainWindow::readMplayerOutput);

    // 修复: mplayer 进程异常退出时自动重启
    connect(mplayerProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        qDebug() << "mplayer exited with code:" << exitCode << "status:" << exitStatus;
        if (isPlaying && !songEnded) {
            qDebug() << "mplayer crashed during playback, restarting...";
            initMplayer();
            if (!songList.isEmpty() && currentSongIndex >= 0 && currentSongIndex < songList.size()) {
                playSong(currentSongIndex);
            }
        }
    });
}

void MainWindow::readMplayerOutput()
{
    while (mplayerProcess->canReadLine()) {
        QString line = QString::fromUtf8(mplayerProcess->readLine()).trimmed();

        if (line.contains("EOF") || line.contains("End of file") || line.contains("exit")) {
            if (!songEnded && isPlaying) {
                songEnded = true;
                onSongFinished();
                return;
            }
        }

        if (line.contains("ANS_TIME_POSITION=")) {
            QStringList parts = line.split("=");
            if (parts.size() >= 2) {
                float pos = parts[1].toFloat();

                // #4修复: 移除时间回退检测的EOF误判逻辑
                // mplayer会通过EOF消息明确通知歌曲结束
                // 时间回退只在接近总时长且delta合理时才触发（保守策略）
                if (isPlaying && totalDuration > 0 && pos > 0.5) {
                    if (pos >= totalDuration - 1.0 && !songEnded) {
                        songEnded = true;
                        onSongFinished();
                        return;
                    }
                }

                lastPosition = pos;
                currentPosition = static_cast<int>(pos);

                if (!isSliderPressed) {
                    ui->curTimeLabel->setText(formatTime(currentPosition));
                    if (totalDuration > 0) {
                        ui->progressSlider->setValue((currentPosition * 1000) / totalDuration);
                    }
                    updateLyricsHighlight(static_cast<int>(pos * 1000));
                    emit progressUpdated(currentPosition, totalDuration);
                }
            }
        }

        if (line.contains("ANS_LENGTH=")) {
            QStringList parts = line.split("=");
            if (parts.size() >= 2) {
                totalDuration = static_cast<int>(parts[1].toFloat());
                ui->totalTimeLabel->setText(formatTime(totalDuration));
            }
        }

        // #3修复: 处理 pause 状态查询响应
        if (line.contains("ANS_pause=")) {
            bool paused = line.contains("yes");
            if (isPlaying && paused) {
                isPlaying = false;
                ui->playBtn->setText("播放");
                emit playStateChanged(false);
            } else if (!isPlaying && !paused && totalDuration > 0) {
                isPlaying = true;
                ui->playBtn->setText("暂停");
                emit playStateChanged(true);
            }
        }
    }
}

void MainWindow::onSongFinished()
{
    switch (loopMode) {
    case 0: playNext(); break;
    case 1: playSong(currentSongIndex); break;
    case 2:
        if (songList.size() > 1) {
            int idx;
            do { idx = std::rand() % songList.size(); } while (idx == currentSongIndex);
            playSong(idx);
        }
        break;
    }
}

// ============ 新增功能 ============
void MainWindow::setPlaybackSpeed(double speed)
{
    playbackSpeed = speed;
    if (ui->speedBtn) {
        if (speed == 1.0) {
            ui->speedBtn->setText("1.0x");
        } else {
            ui->speedBtn->setText(QString("%1x").arg(speed, 0, 'f', speed == (int)speed ? 1 : 2));
        }
    }
    sendCommand(QString("speed_set %1").arg(speed));
    // #17修复: 同步设置页ComboBox
    static const double speedValues[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
    for (int i = 0; i < 6; i++) {
        if (qAbs(playbackSpeed - speedValues[i]) < 0.01) {
            if (settingsSpeedCombo && settingsSpeedCombo->currentIndex() != i)
                settingsSpeedCombo->setCurrentIndex(i);
            break;
        }
    }
    saveConfig();
}

void MainWindow::sortSongs(int mode)
{
    sortMode = mode;
    if (songList.isEmpty()) return;

    QVector<int> indices(songList.size());
    for (int i = 0; i < indices.size(); i++) indices[i] = i;

    switch (mode) {
    case 1:
        std::sort(indices.begin(), indices.end(), [this](int a, int b) {
            return songTitles[a].toLower() < songTitles[b].toLower();
        });
        break;
    case 2:
        std::sort(indices.begin(), indices.end(), [this](int a, int b) {
            return songArtists[a].toLower() < songArtists[b].toLower();
        });
        break;
    case 3:
        std::sort(indices.begin(), indices.end(), [this](int a, int b) {
            int posA = recentPlayed.indexOf(songNames[a]);
            int posB = recentPlayed.indexOf(songNames[b]);
            if (posA < 0) posA = recentPlayed.size();
            if (posB < 0) posB = recentPlayed.size();
            return posA < posB;
        });
        break;
    case 4:
        std::sort(indices.begin(), indices.end(), [this](int a, int b) {
            int countA = playCounts.value(songNames[a], 0);
            int countB = playCounts.value(songNames[b], 0);
            return countA > countB;
        });
        break;
    default:
        break;
    }

    QVector<QString> sortedPaths, sortedNames, sortedTitles, sortedArtists;
    for (int idx : indices) {
        sortedPaths.append(songList[idx]);
        sortedNames.append(songNames[idx]);
        sortedTitles.append(songTitles[idx]);
        sortedArtists.append(songArtists[idx]);
    }

    // #6修复: 排序前保存原始 currentSongIndex，排序后映射到新位置
    int oldSongIndex = currentSongIndex;
    songList = sortedPaths;
    songNames = sortedNames;
    songTitles = sortedTitles;
    songArtists = sortedArtists;

    for (int i = 0; i < indices.size(); i++) {
        if (indices[i] == oldSongIndex) {
            currentSongIndex = i;
            break;
        }
    }

    // NEW-2修复: 排序后同步更新 playHistory 中的索引
    for (int i = 0; i < playHistory.size(); i++) {
        int oldIdx = playHistory[i];
        if (oldIdx >= 0 && oldIdx < indices.size()) {
            // indices[newPos] = oldIdx → 找到 oldIdx 对应的 newPos
            for (int newPos = 0; newPos < indices.size(); newPos++) {
                if (indices[newPos] == oldIdx) {
                    playHistory[i] = newPos;
                    break;
                }
            }
        }
    }

    if (ui->songsListWidget) {
        ui->songsListWidget->clear();
        for (int i = 0; i < songTitles.size(); i++) {
            ui->songsListWidget->addItem(songTitles[i] + " - " + songArtists[i]);
        }
    }
    if (playlistList) {
        playlistList->clear();
        for (int i = 0; i < songTitles.size(); i++) {
            QString prefix = favorites.contains(songNames[i]) ? "♥ " : "";
            playlistList->addItem(prefix + songTitles[i] + " - " + songArtists[i]);
        }
    }
}

void MainWindow::loadPlayCounts()
{
    QString path = QDir::currentPath() + "/play_counts.txt";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&file);
    in.setCodec("UTF-8");
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        QStringList parts = line.split("|");
        if (parts.size() >= 2) {
            playCounts[parts[0]] = parts[1].toInt();
        }
    }
    file.close();
}

void MainWindow::savePlayCounts()
{
    QString path = QDir::currentPath() + "/play_counts.txt";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    out.setCodec("UTF-8");
    for (auto it = playCounts.begin(); it != playCounts.end(); ++it) {
        out << it.key() << "|" << it.value() << "\n";
    }
    file.close();
}

void MainWindow::updateSleepDisplay()
{
    if (!ui->sleepBtn) return;
    if (sleepDuration > 0 && sleepRemaining > 0) {
        int mins = sleepRemaining / 60;
        int secs = sleepRemaining % 60;
        ui->sleepBtn->setText(QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0')));
    }
}

// ============ 辅助函数：创建圆角缩略图 ============
QPixmap MainWindow::makeRoundedPixmap(const QPixmap &src, int size, int radius)
{
    if (src.isNull()) return QPixmap();
    QPixmap scaled = src.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPixmap rounded(size, size);
    rounded.fill(Qt::transparent);
    QPainterPath path;
    path.addRoundedRect(0, 0, size, size, radius, radius);
    QPainter p(&rounded);
    p.setClipPath(path);
    p.drawPixmap(0, 0, scaled);
    p.end();
    return rounded;
}

// ============ 更新问候语和时间日期 ============
void MainWindow::updateGreeting()
{
    QDateTime now = QDateTime::currentDateTime();
    int hour = now.time().hour();
    QString greeting;
    if (hour < 6) greeting = "夜深了，注意休息";
    else if (hour < 12) greeting = "早上好";
    else if (hour < 14) greeting = "中午好";
    else if (hour < 18) greeting = "下午好";
    else greeting = "晚上好";

    QStringList weekDays = {"周一", "周二", "周三", "周四", "周五", "周六", "周日"};
    QString dateStr = now.toString("MM月dd日") + " " + weekDays[now.date().dayOfWeek() - 1];
    if (ui->greetingLabel) ui->greetingLabel->setText(greeting + "\n" + dateStr);
}

// ============ #5修复: 刷新首页每日推荐和最近播放 ============
void MainWindow::refreshHomePage()
{
    // 辅助 lambda: 查找歌曲封面，无封面时返回默认 music.jpg
    auto findCover = [this](int idx) -> QPixmap {
        if (idx < 0 || idx >= songList.size()) return QPixmap();
        QFileInfo songInfo(songList[idx]);
        QStringList coverNames;
        coverNames << songInfo.completeBaseName() + ".jpg"
                   << songInfo.completeBaseName() + ".png";
        for (const QString &name : coverNames) {
            QString testPath = songInfo.path() + "/" + name;
            if (QFile::exists(testPath)) {
                QPixmap px(testPath);
                if (px.isNull()) {
                    // QImage 降级：嵌入式设备 QPixmap 可能加载失败
                    QImage img(testPath);
                    if (!img.isNull()) {
                        // 如果图片太大，先缩放再转换
                        if (img.width() > 500 || img.height() > 500) {
                            img = img.scaled(474, 474, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        }
                        px = QPixmap::fromImage(img);
                        qDebug() << "  findCover QImage fallback succeeded:" << testPath;
                    }
                }
                if (!px.isNull()) return px;
            }
        }
        // 默认封面 music.jpg
        QString defaultCover = songInfo.path() + "/music.jpg";
        if (QFile::exists(defaultCover)) {
            QPixmap px(defaultCover);
            if (px.isNull()) {
                QImage img(defaultCover);
                if (!img.isNull()) {
                    // 如果图片太大，先缩放再转换
                    if (img.width() > 500 || img.height() > 500) {
                        img = img.scaled(474, 474, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }
                    px = QPixmap::fromImage(img);
                }
            }
            if (!px.isNull()) return px;
        }
        return QPixmap();
    };

    // --- 每日推荐：随机选择最多4首歌曲 ---
    QHBoxLayout *recLayout = qobject_cast<QHBoxLayout*>(ui->recommendGrid->layout());
    if (recLayout) {
        // 清空旧内容
        while (recLayout->count() > 0) {
            QLayoutItem *item = recLayout->takeAt(0);
            if (item->widget()) delete item->widget();
            delete item;
        }

        if (!songList.isEmpty()) {
            QVector<int> indices;
            for (int i = 0; i < songList.size(); i++) indices.append(i);

            // 简单洗牌
            for (int i = indices.size() - 1; i > 0; i--) {
                int j = std::rand() % (i + 1);
                qSwap(indices[i], indices[j]);
            }

            int count = qMin(4, indices.size());
            for (int k = 0; k < count; k++) {
                int idx = indices[k];
                QWidget *card = new QWidget();
                card->setFixedHeight(130);
                card->setStyleSheet("QWidget { background: rgba(255,255,255,8); border-radius: 10px; }"
                    "QWidget:hover { background: rgba(255,255,255,15); }");
                card->setMinimumWidth(160);
                QVBoxLayout *cLayout = new QVBoxLayout(card);
                cLayout->setContentsMargins(12, 8, 12, 8);

                // 专辑封面
                QLabel *coverLabel = new QLabel(card);
                coverLabel->setFixedSize(60, 60);
                coverLabel->setAlignment(Qt::AlignCenter);
                QPixmap cover = findCover(idx);
                if (!cover.isNull()) {
                    coverLabel->setPixmap(makeRoundedPixmap(cover, 60));
                } else {
                    coverLabel->setStyleSheet("background: rgba(255,255,255,10); border-radius: 8px; font-size: 20px; font-weight: bold;");
                    coverLabel->setText("乐");
                }

                QString titleText = (idx < songTitles.size()) ? songTitles[idx] : "未知歌曲";
                QLabel *title = new QLabel(titleText, card);
                title->setStyleSheet("color: white; font-size: 12px; font-weight: bold; background: transparent;");
                title->setAlignment(Qt::AlignCenter);
                title->setWordWrap(true);

                cLayout->addWidget(coverLabel, 0, Qt::AlignCenter);
                cLayout->addWidget(title);

                // 点击播放
                int songIdx = idx;
                QPushButton *overlay = new QPushButton("", card);
                overlay->setStyleSheet("QPushButton { background: transparent; border: none; }"
                    "QPushButton:pressed { background: rgba(255,255,255,10); }");
                overlay->setFixedSize(160, 130);  // 直接设置固定尺寸，避免 QTimer 悬空指针
                connect(overlay, &QPushButton::clicked, [this, songIdx]() {
                    playSong(songIdx);
                    ui->stackedWidget->setCurrentIndex(3);
                    currentTabIndex = 3;
                    updateTabHighlight(3);
                });

                recLayout->addWidget(card);
            }
        }
    }

    // --- 最近播放：显示最近播放的最多4首歌曲 ---
    QHBoxLayout *recentLayout = qobject_cast<QHBoxLayout*>(ui->recentGrid->layout());
    if (recentLayout) {
        while (recentLayout->count() > 0) {
            QLayoutItem *item = recentLayout->takeAt(0);
            if (item->widget()) delete item->widget();
            delete item;
        }

        int count = qMin(4, recentPlayed.size());
        for (int k = 0; k < count; k++) {
            const QString &name = recentPlayed[k];
            int songIdx = songNames.indexOf(name);
            if (songIdx < 0) continue;

            QWidget *card = new QWidget();
            card->setFixedHeight(130);
            card->setStyleSheet("QWidget { background: rgba(255,255,255,8); border-radius: 10px; }"
                "QWidget:hover { background: rgba(255,255,255,15); }");
            card->setMinimumWidth(160);
            QVBoxLayout *cLayout = new QVBoxLayout(card);
            cLayout->setContentsMargins(12, 8, 12, 8);

            // 专辑封面
            QLabel *coverLabel = new QLabel(card);
            coverLabel->setFixedSize(60, 60);
            coverLabel->setAlignment(Qt::AlignCenter);
            QPixmap cover = findCover(songIdx);
            if (!cover.isNull()) {
                coverLabel->setPixmap(makeRoundedPixmap(cover, 60));
            } else {
                coverLabel->setStyleSheet("background: rgba(255,255,255,10); border-radius: 8px; font-size: 20px; font-weight: bold;");
                coverLabel->setText("乐");
            }

            QString titleText = (songIdx < songTitles.size()) ? songTitles[songIdx] : "未知歌曲";
            QLabel *title = new QLabel(titleText, card);
            title->setStyleSheet("color: white; font-size: 12px; font-weight: bold; background: transparent;");
            title->setAlignment(Qt::AlignCenter);
            title->setWordWrap(true);

            cLayout->addWidget(coverLabel, 0, Qt::AlignCenter);
            cLayout->addWidget(title);

            int si = songIdx;
            QPushButton *overlay = new QPushButton("", card);
            overlay->setStyleSheet("QPushButton { background: transparent; border: none; }"
                "QPushButton:pressed { background: rgba(255,255,255,10); }");
            overlay->setFixedSize(160, 130);  // 直接设置固定尺寸，避免 QTimer 悬空指针
            connect(overlay, &QPushButton::clicked, [this, si]() {
                playSong(si);
                ui->stackedWidget->setCurrentIndex(3);
                currentTabIndex = 3;
                updateTabHighlight(3);
            });

            recentLayout->addWidget(card);
        }

        // 如果没有最近播放记录，显示提示
        if (count == 0) {
            QLabel *emptyLabel = new QLabel("暂无播放记录");
            emptyLabel->setStyleSheet("color: rgba(255,255,255,60); font-size: 13px;");
            emptyLabel->setAlignment(Qt::AlignCenter);
            recentLayout->addWidget(emptyLabel);
        }
    }
}
