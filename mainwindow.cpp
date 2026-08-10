#include "mainwindow.h"
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
#include <QGridLayout>
#include <QScrollArea>
#include <cstdlib>
#include <ctime>
#include <cmath>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , ui(nullptr)
    , mplayerProcess(nullptr)
    , currentSongIndex(0)
    , currentLyricIndex(-1)
    , lyricsMode(0)
    , isPlaying(false)
    , isSliderPressed(false)
    , isMuted(false)
    , loopMode(0)
    , themeMode(0)
    , eqMode(0)
    , totalDuration(0)
    , currentPosition(0)
    , lastPosition(0)
    , coverAnimStep(0)
    , currentTabIndex(3) // 默认显示播放页
{
    std::srand(std::time(nullptr));

    // 初始化UI
    initUI();

    // 加载配置
    loadConfig();

    // 扫描歌曲
    scanSongs();

    // 加载收藏
    loadFavorites();

    // 初始化mplayer
    initMplayer();

    // 定时器
    progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, this, &MainWindow::updateProgress);
    progressTimer->start(300);

    coverAnimTimer = new QTimer(this);
    connect(coverAnimTimer, &QTimer::timeout, [this]() {
        if (!isPlaying) return;
        coverAnimStep = (coverAnimStep + 1) % 360;
        int r = 26 + static_cast<int>(10 * sin(coverAnimStep * 3.14159 / 180));
        int g = 74 + static_cast<int>(20 * sin((coverAnimStep + 120) * 3.14159 / 180));
        int b = 58 + static_cast<int>(15 * sin((coverAnimStep + 240) * 3.14159 / 180));
        if (coverLabel) {
            coverLabel->setStyleSheet(
                QString("QLabel { background: rgb(%1,%2,%3); border-radius: 16px;"
                        "  font-size: 56px; color: rgba(255,255,255,15); }")
                .arg(r).arg(g).arg(b));
        }
    });
    coverAnimTimer->start(100);

    // 切换到播放页
    stackedWidget->setCurrentIndex(currentTabIndex);
    updateTabHighlight(currentTabIndex);
}

MainWindow::~MainWindow()
{
    saveConfig();
    if (mplayerProcess && mplayerProcess->state() != QProcess::NotRunning) {
        sendCommand("quit");
        mplayerProcess->waitForFinished(2000);
        mplayerProcess->kill();
    }
}

void MainWindow::initUI()
{
    setWindowFlags(Qt::FramelessWindowHint);
    setFixedSize(1024, 600);

    // 整体背景
    setStyleSheet("QWidget#MainWindow { background: #0a1628; }");
    setObjectName("MainWindow");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // QStackedWidget
    stackedWidget = new QStackedWidget(this);
    stackedWidget->addWidget(createHomePage());      // 0
    stackedWidget->addWidget(createSongsPage());     // 1
    stackedWidget->addWidget(createFavoritesPage()); // 2
    stackedWidget->addWidget(createPlayerPage());    // 3
    stackedWidget->addWidget(createSettingsPage());  // 4

    mainLayout->addWidget(stackedWidget, 1);
    mainLayout->addWidget(createTabBar(), 0);
}

// ============ Tab 栏 ============
QWidget* MainWindow::createTabBar()
{
    QWidget *tabBar = new QWidget(this);
    tabBar->setFixedHeight(56);
    tabBar->setStyleSheet("QWidget { background: rgba(0,0,0,220); border-top: 1px solid rgba(255,255,255,10); }");

    QHBoxLayout *layout = new QHBoxLayout(tabBar);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(0);

    QStringList tabNames = {"首页", "歌曲", "收藏", "播放", "设置"};
    QStringList tabIcons = {"H", "S", "F", "P", "C"}; // ASCII icons

    for (int i = 0; i < tabNames.size(); i++) {
        QPushButton *btn = new QPushButton(tabNames[i], tabBar);
        btn->setMinimumSize(80, 48);
        btn->setStyleSheet(
            "QPushButton { background: transparent; border: none; color: rgba(255,255,255,100);"
            "  font-size: 13px; }"
            "QPushButton:pressed { color: #31C27C; }");
        layout->addWidget(btn);
        tabButtons.append(btn);

        int idx = i;
        connect(btn, &QPushButton::clicked, [this, idx]() {
            stackedWidget->setCurrentIndex(idx);
            currentTabIndex = idx;
            updateTabHighlight(idx);
        });
    }

    return tabBar;
}

void MainWindow::updateTabHighlight(int idx)
{
    for (int i = 0; i < tabButtons.size(); i++) {
        if (i == idx) {
            tabButtons[i]->setStyleSheet(
                "QPushButton { background: transparent; border: none; color: #31C27C;"
                "  font-size: 13px; font-weight: bold; }");
        } else {
            tabButtons[i]->setStyleSheet(
                "QPushButton { background: transparent; border: none; color: rgba(255,255,255,100);"
                "  font-size: 13px; }");
        }
    }
}

// ============ 首页 ============
QWidget* MainWindow::createHomePage()
{
    homePage = new QWidget();
    homePage->setStyleSheet("background: qlineargradient(x1:0,y1:0,x2:0.5,y2:1,"
        "stop:0 #0a1628, stop:0.3 #0d2137, stop:0.7 #122a3e, stop:1 #0f2530);");

    QVBoxLayout *layout = new QVBoxLayout(homePage);
    layout->setContentsMargins(30, 20, 30, 10);

    // 标题
    QLabel *title = new QLabel("音乐播放器", homePage);
    title->setStyleSheet("color: white; font-size: 24px; font-weight: bold;");
    layout->addWidget(title);
    layout->addSpacing(10);

    // 当前播放卡片
    QWidget *card = new QWidget(homePage);
    card->setFixedHeight(80);
    card->setStyleSheet("QWidget { background: rgba(255,255,255,10); border-radius: 12px; }");
    QHBoxLayout *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(16, 12, 16, 12);

    QLabel *cardIcon = new QLabel("♪", card);
    cardIcon->setFixedSize(56, 56);
    cardIcon->setStyleSheet("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #2d6b4f,stop:1 #1a4a3a);"
        "border-radius: 8px; font-size: 20px; color: rgba(255,255,255,30);");
    cardIcon->setAlignment(Qt::AlignCenter);

    QVBoxLayout *infoLayout = new QVBoxLayout();
    QLabel *cardTitle = new QLabel("未播放", card);
    cardTitle->setObjectName("homeNowPlaying");
    cardTitle->setStyleSheet("color: white; font-size: 15px; font-weight: bold;");
    homeNowPlaying = cardTitle;

    QLabel *cardSub = new QLabel("点击播放按钮开始", card);
    cardSub->setStyleSheet("color: rgba(255,255,255,80); font-size: 12px;");

    infoLayout->addWidget(cardTitle);
    infoLayout->addWidget(cardSub);

    QPushButton *cardPlayBtn = new QPushButton("播放", card);
    cardPlayBtn->setFixedSize(48, 48);
    cardPlayBtn->setStyleSheet("QPushButton { background: #31C27C; border-radius: 24px;"
        "color: white; font-size: 14px; font-weight: bold; }"
        "QPushButton:pressed { background: #269e65; }");
    connect(cardPlayBtn, &QPushButton::clicked, [this]() {
        if (!isPlaying && !songList.isEmpty()) {
            playSong(currentSongIndex);
        }
        stackedWidget->setCurrentIndex(3);
        currentTabIndex = 3;
        updateTabHighlight(3);
    });

    cardLayout->addWidget(cardIcon);
    cardLayout->addLayout(infoLayout, 1);
    cardLayout->addWidget(cardPlayBtn);

    layout->addWidget(card);
    layout->addSpacing(20);

    // 最近播放
    QLabel *recentTitle = new QLabel("最近播放", homePage);
    recentTitle->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    layout->addWidget(recentTitle);
    layout->addSpacing(8);

    QWidget *recentGrid = new QWidget(homePage);
    QHBoxLayout *gridLayout = new QHBoxLayout(recentGrid);
    gridLayout->setSpacing(12);

    for (int i = 0; i < 3; i++) {
        QWidget *item = new QWidget(recentGrid);
        item->setFixedSize(160, 140);
        item->setStyleSheet("QWidget { background: rgba(255,255,255,8); border-radius: 10px; }"
            "QWidget:hover { background: rgba(255,255,255,15); }");
        QVBoxLayout *itemLayout = new QVBoxLayout(item);
        itemLayout->setContentsMargins(12, 12, 12, 8);

        QLabel *icon = new QLabel("♪", item);
        icon->setFixedSize(136, 80);
        icon->setStyleSheet("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "stop:0 #1a4a3a, stop:1 #2d6b4f); border-radius: 8px; font-size: 28px;"
            "color: rgba(255,255,255,15);");
        icon->setAlignment(Qt::AlignCenter);

        QLabel *name = new QLabel(i < songTitles.size() ? songTitles[i] : "---", item);
        name->setStyleSheet("color: rgba(255,255,255,180); font-size: 12px;");
        name->setWordWrap(true);

        itemLayout->addWidget(icon);
        itemLayout->addWidget(name);
        gridLayout->addWidget(item);

        int idx = i;
        // 点击卡片跳转播放页
        QPushButton *cardOverlay = new QPushButton("", item);
        cardOverlay->setStyleSheet("QPushButton { background: transparent; border: none; }");
        cardOverlay->setGeometry(0, 0, 160, 140);
        connect(cardOverlay, &QPushButton::clicked, [this, idx]() {
            if (idx < songList.size()) {
                playSong(idx);
                stackedWidget->setCurrentIndex(3);
                currentTabIndex = 3;
                updateTabHighlight(3);
            }
        });
    }

    layout->addWidget(recentGrid);
    layout->addStretch();

    return homePage;
}

// ============ 歌曲列表页 ============
QWidget* MainWindow::createSongsPage()
{
    songsPage = new QWidget();
    songsPage->setStyleSheet("background: qlineargradient(x1:0,y1:0,x2:0.5,y2:1,"
        "stop:0 #0a1628, stop:0.3 #0d2137, stop:0.7 #122a3e, stop:1 #0f2530);");

    QVBoxLayout *layout = new QVBoxLayout(songsPage);
    layout->setContentsMargins(20, 16, 20, 8);

    // 搜索框
    searchEdit = new QLineEdit(songsPage);
    searchEdit->setPlaceholderText("搜索歌曲...");
    searchEdit->setFixedHeight(40);
    searchEdit->setStyleSheet("QLineEdit { background: rgba(255,255,255,10); border: 1px solid rgba(255,255,255,20);"
        "border-radius: 20px; color: white; padding: 0 16px; font-size: 14px; }"
        "QLineEdit:focus { border-color: #31C27C; }");
    connect(searchEdit, &QLineEdit::textChanged, [this](const QString &text) {
        for (int i = 0; i < songsListWidget->count(); i++) {
            bool match = text.isEmpty() || songsListWidget->item(i)->text().contains(text, Qt::CaseInsensitive);
            songsListWidget->item(i)->setHidden(!match);
        }
    });
    layout->addWidget(searchEdit);
    layout->addSpacing(8);

    // 歌曲列表
    songsListWidget = new QListWidget(songsPage);
    songsListWidget->setStyleSheet(
        "QListWidget { background: transparent; border: none; color: rgba(255,255,255,200); }"
        "QListWidget::item { padding: 14px 16px; border-bottom: 1px solid rgba(255,255,255,8); font-size: 14px; }"
        "QListWidget::item:selected { background: rgba(49,194,124,30); color: #31C27C; }"
        "QListWidget::item:hover { background: rgba(255,255,255,10); }");
    connect(songsListWidget, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item) {
        int row = songsListWidget->row(item);
        if (row >= 0 && row < songList.size()) {
            playSong(row);
            stackedWidget->setCurrentIndex(3);
            currentTabIndex = 3;
            updateTabHighlight(3);
        }
    });
    layout->addWidget(songsListWidget, 1);

    return songsPage;
}

// ============ 收藏页 ============
QWidget* MainWindow::createFavoritesPage()
{
    favoritesPage = new QWidget();
    favoritesPage->setStyleSheet("background: qlineargradient(x1:0,y1:0,x2:0.5,y2:1,"
        "stop:0 #0a1628, stop:0.3 #0d2137, stop:0.7 #122a3e, stop:1 #0f2530);");

    QVBoxLayout *layout = new QVBoxLayout(favoritesPage);
    layout->setContentsMargins(20, 16, 20, 8);

    // 标题栏
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *favTitle = new QLabel("我的收藏", favoritesPage);
    favTitle->setStyleSheet("color: white; font-size: 20px; font-weight: bold;");

    QPushButton *playAllBtn = new QPushButton("播放全部", favoritesPage);
    playAllBtn->setFixedSize(80, 32);
    playAllBtn->setStyleSheet("QPushButton { background: #31C27C; border-radius: 16px;"
        "color: white; font-size: 12px; }"
        "QPushButton:pressed { background: #269e65; }");
    connect(playAllBtn, &QPushButton::clicked, [this]() {
        if (!favorites.isEmpty() && !songList.isEmpty()) {
            for (int i = 0; i < songNames.size(); i++) {
                if (favorites.contains(songNames[i])) {
                    playSong(i);
                    stackedWidget->setCurrentIndex(3);
                    currentTabIndex = 3;
                    updateTabHighlight(3);
                    break;
                }
            }
        }
    });

    headerLayout->addWidget(favTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(playAllBtn);
    layout->addLayout(headerLayout);
    layout->addSpacing(8);

    // 收藏列表
    favoritesListWidget = new QListWidget(favoritesPage);
    favoritesListWidget->setStyleSheet(
        "QListWidget { background: transparent; border: none; color: rgba(255,255,255,200); }"
        "QListWidget::item { padding: 14px 16px; border-bottom: 1px solid rgba(255,255,255,8); font-size: 14px; }"
        "QListWidget::item:selected { background: rgba(231,76,60,30); color: #e74c3c; }"
        "QListWidget::item:hover { background: rgba(255,255,255,10); }");
    connect(favoritesListWidget, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item) {
        int row = favoritesListWidget->row(item);
        // 找到对应的歌曲索引
        int songIdx = 0;
        int count = 0;
        for (int i = 0; i < songNames.size(); i++) {
            if (favorites.contains(songNames[i])) {
                if (count == row) { songIdx = i; break; }
                count++;
            }
        }
        playSong(songIdx);
        stackedWidget->setCurrentIndex(3);
        currentTabIndex = 3;
        updateTabHighlight(3);
    });
    layout->addWidget(favoritesListWidget, 1);

    return favoritesPage;
}

// ============ 播放页 ============
QWidget* MainWindow::createPlayerPage()
{
    playerPage = new QWidget();
    playerPage->setStyleSheet("background: qlineargradient(x1:0,y1:0,x2:0.5,y2:1,"
        "stop:0 #0a1628, stop:0.3 #0d2137, stop:0.7 #122a3e, stop:1 #0f2530);");

    QVBoxLayout *mainLayout = new QVBoxLayout(playerPage);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 内容区
    QWidget *content = new QWidget(playerPage);
    QHBoxLayout *contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(40, 30, 40, 0);
    contentLayout->setSpacing(30);

    // 左侧封面
    coverLabel = new QLabel("音乐", content);
    coverLabel->setFixedSize(300, 300);
    coverLabel->setAlignment(Qt::AlignCenter);
    coverLabel->setStyleSheet("QLabel { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "stop:0 #1a4a3a, stop:0.5 #2d6b4f, stop:1 #0f3a28);"
        "border-radius: 16px; font-size: 56px; color: rgba(255,255,255,30); }");
    contentLayout->addWidget(coverLabel);

    // 右侧信息+歌词
    QWidget *infoPanel = new QWidget(content);
    QVBoxLayout *infoLayout = new QVBoxLayout(infoPanel);
    infoLayout->setContentsMargins(0, 10, 0, 0);

    songTitleLabel = new QLabel("未播放", infoPanel);
    songTitleLabel->setStyleSheet("color: white; font-weight: bold; font-size: 24px;");

    songArtistLabel = new QLabel("", infoPanel);
    songArtistLabel->setStyleSheet("color: rgba(255,255,255,130); font-size: 14px;");

    lyricsList = new QListWidget(infoPanel);
    lyricsList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lyricsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lyricsList->setStyleSheet(
        "QListWidget { background: transparent; border: none; color: rgba(255,255,255,40); }"
        "QListWidget::item { padding: 10px 12px; border: none; }"
        "QListWidget::item:selected { background: transparent; }");

    infoLayout->addWidget(songTitleLabel);
    infoLayout->addWidget(songArtistLabel);
    infoLayout->addWidget(lyricsList, 1);

    contentLayout->addWidget(infoPanel, 1);

    // 播放列表面板
    playlistPanel = new QWidget(playerPage);
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
    connect(playlistList, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item) {
        int row = playlistList->row(item);
        if (row >= 0 && row < songList.size()) playSong(row);
    });

    plLayout->addWidget(plTitle);
    plLayout->addWidget(plInfoLabel);
    plLayout->addWidget(playlistList, 1);

    mainLayout->addWidget(content, 1);

    // 底部控制栏
    playerBar = new QWidget(playerPage);
    playerBar->setFixedHeight(80);
    playerBar->setStyleSheet("QWidget { background: rgba(0,0,0,200);"
        "border-top: 1px solid rgba(255,255,255,10); }");
    QHBoxLayout *barLayout = new QHBoxLayout(playerBar);
    barLayout->setContentsMargins(16, 8, 16, 4);

    // 左侧
    barThumbLabel = new QLabel("乐", playerBar);
    barThumbLabel->setFixedSize(44, 44);
    barThumbLabel->setAlignment(Qt::AlignCenter);
    barThumbLabel->setStyleSheet("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "stop:0 #2d6b4f, stop:1 #1a4a3a); border-radius: 6px;"
        "font-size: 16px; color: rgba(255,255,255,80);");

    QVBoxLayout *barInfoLayout = new QVBoxLayout();
    barNameLabel = new QLabel("未播放", playerBar);
    barNameLabel->setStyleSheet("color: rgba(255,255,255,230); font-size: 13px;");
    QLabel *barArtistLabel = new QLabel("", playerBar);
    barArtistLabel->setStyleSheet("color: rgba(255,255,255,100); font-size: 11px;");
    barInfoLayout->addWidget(barNameLabel);
    barInfoLayout->addWidget(barArtistLabel);

    likeBtn = new QPushButton("♥", playerBar);
    likeBtn->setMinimumSize(40, 40);
    likeBtn->setStyleSheet("QPushButton { background: transparent; border: none;"
        "color: rgba(255,255,255,80); font-size: 20px; padding: 6px; }"
        "QPushButton:pressed { color: #e74c3c; }");
    connect(likeBtn, &QPushButton::clicked, [this]() { on_likeBtn_clicked(); });

    barLayout->addWidget(barThumbLabel);
    barLayout->addLayout(barInfoLayout);
    barLayout->addWidget(likeBtn);

    // 中间控制
    QVBoxLayout *centerLayout = new QVBoxLayout();
    QHBoxLayout *ctrlLayout = new QHBoxLayout();
    ctrlLayout->setSpacing(18);

    QString ctrlStyle = "QPushButton { background: transparent; border: none;"
        "color: rgba(255,255,255,160); font-size: 18px; padding: 8px; }"
        "QPushButton:pressed { color: white; }";

    loopBtn = new QPushButton("列表循环", playerBar);
    loopBtn->setMinimumSize(64, 48);
    loopBtn->setStyleSheet(ctrlStyle);
    connect(loopBtn, &QPushButton::clicked, [this]() { on_loopBtn_clicked(); });

    prevBtn = new QPushButton("上一曲", playerBar);
    prevBtn->setMinimumSize(48, 48);
    prevBtn->setStyleSheet(ctrlStyle);
    connect(prevBtn, &QPushButton::clicked, [this]() { playPrev(); });

    playBtn = new QPushButton("播放", playerBar);
    playBtn->setFixedSize(44, 44);
    playBtn->setStyleSheet("QPushButton { background: #31C27C; border-radius: 22px;"
        "color: white; font-size: 16px; font-weight: bold; }"
        "QPushButton:pressed { background: #269e65; }");
    connect(playBtn, &QPushButton::clicked, [this]() { on_playBtn_clicked(); });

    nextBtn = new QPushButton("下一曲", playerBar);
    nextBtn->setMinimumSize(48, 48);
    nextBtn->setStyleSheet(ctrlStyle);
    connect(nextBtn, &QPushButton::clicked, [this]() { playNext(); });

    volumeBtn = new QPushButton("音量", playerBar);
    volumeBtn->setMinimumSize(48, 48);
    volumeBtn->setStyleSheet(ctrlStyle);
    connect(volumeBtn, &QPushButton::clicked, [this]() {
        volumePopup->setVisible(!volumePopup->isVisible());
    });

    ctrlLayout->addWidget(loopBtn);
    ctrlLayout->addWidget(prevBtn);
    ctrlLayout->addWidget(playBtn);
    ctrlLayout->addWidget(nextBtn);
    ctrlLayout->addWidget(volumeBtn);

    QHBoxLayout *progressLayout = new QHBoxLayout();
    curTimeLabel = new QLabel("00:00", playerBar);
    curTimeLabel->setStyleSheet("color: rgba(255,255,255,120); font-size: 11px;");
    curTimeLabel->setFixedWidth(40);
    curTimeLabel->setAlignment(Qt::AlignCenter);

    progressSlider = new QSlider(Qt::Horizontal, playerBar);
    progressSlider->setRange(0, 1000);
    progressSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 4px; background: rgba(255,255,255,30); border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #31C27C; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; }"
        "QSlider::sub-page:horizontal { background: #31C27C; border-radius: 2px; }"
        "QSlider::add-page:horizontal { background: rgba(255,255,255,20); border-radius: 2px; }");
    connect(progressSlider, &QSlider::sliderPressed, [this]() { isSliderPressed = true; });
    connect(progressSlider, &QSlider::sliderReleased, [this]() {
        isSliderPressed = false;
        if (totalDuration > 0) {
            int targetSec = (progressSlider->value() * totalDuration) / 1000;
            sendCommand(QString("seek %1 2").arg(targetSec));
        }
    });

    totalTimeLabel = new QLabel("00:00", playerBar);
    totalTimeLabel->setStyleSheet("color: rgba(255,255,255,120); font-size: 11px;");
    totalTimeLabel->setFixedWidth(40);
    totalTimeLabel->setAlignment(Qt::AlignCenter);

    progressLayout->addWidget(curTimeLabel);
    progressLayout->addWidget(progressSlider);
    progressLayout->addWidget(totalTimeLabel);

    centerLayout->addLayout(ctrlLayout);
    centerLayout->addLayout(progressLayout);
    barLayout->addLayout(centerLayout, 1);

    // 右侧功能
    QVBoxLayout *rightLayout = new QVBoxLayout();
    QHBoxLayout *funcLayout = new QHBoxLayout();

    QPushButton *modeBtn = new QPushButton("标准", playerBar);
    modeBtn->setMinimumSize(44, 28);
    modeBtn->setStyleSheet("QPushButton { background: transparent;"
        "border: 1px solid rgba(255,255,255,50); border-radius: 4px;"
        "color: rgba(255,255,255,120); font-size: 11px; }"
        "QPushButton:pressed { border-color: #31C27C; color: #31C27C; }");
    connect(modeBtn, &QPushButton::clicked, [this]() { setThemeMode((themeMode + 1) % 3); });

    QPushButton *lyricsBtn = new QPushButton("歌词", playerBar);
    lyricsBtn->setMinimumSize(40, 40);
    lyricsBtn->setStyleSheet(ctrlStyle);
    connect(lyricsBtn, &QPushButton::clicked, [this]() { setLyricsMode((lyricsMode + 1) % 4); });

    QPushButton *eqBtn = new QPushButton("均衡", playerBar);
    eqBtn->setMinimumSize(40, 40);
    eqBtn->setStyleSheet(ctrlStyle);
    connect(eqBtn, &QPushButton::clicked, [this]() { setEqMode((eqMode + 1) % 5); });

    playlistBtn = new QPushButton("列表", playerBar);
    playlistBtn->setMinimumSize(48, 48);
    playlistBtn->setStyleSheet(ctrlStyle);
    connect(playlistBtn, &QPushButton::clicked, [this]() {
        playlistPanel->setVisible(!playlistPanel->isVisible());
    });

    funcLayout->addWidget(modeBtn);
    funcLayout->addWidget(lyricsBtn);
    funcLayout->addWidget(eqBtn);
    funcLayout->addWidget(playlistBtn);

    // 音量弹出面板（竖向布局）
    volumePopup = new QWidget(playerPage);
    volumePopup->setFixedSize(60, 200);
    volumePopup->setVisible(false);
    volumePopup->setStyleSheet("QWidget { background: rgba(15,15,20,240);"
        "border: 1px solid rgba(255,255,255,30); border-radius: 12px; }");
    volumePopup->move(950, 300); // 在音量按钮上方弹出

    QVBoxLayout *volPopupLayout = new QVBoxLayout(volumePopup);
    volPopupLayout->setContentsMargins(10, 12, 10, 12);
    volPopupLayout->setSpacing(8);

    QPushButton *muteBtn = new QPushButton("🔊", volumePopup);
    muteBtn->setFixedSize(36, 36);
    muteBtn->setStyleSheet("QPushButton { background: transparent; border: none;"
        "font-size: 20px; color: rgba(255,255,255,180); }"
        "QPushButton:pressed { color: #31C27C; }");
    connect(muteBtn, &QPushButton::clicked, [this, muteBtn]() {
        isMuted = !isMuted;
        if (isMuted) {
            sendCommand("volume 0 1");
            muteBtn->setText("🔇");
            volumeBtn->setText("静音");
        } else {
            sendCommand(QString("volume %1 1").arg(volumeSlider->value()));
            muteBtn->setText("🔊");
            volumeBtn->setText("音量");
        }
    });

    volumeSlider = new QSlider(Qt::Vertical, volumePopup);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(75);
    volumeSlider->setMinimumSize(28, 120);
    volumeSlider->setStyleSheet(
        "QSlider::groove:vertical { width: 4px; background: rgba(255,255,255,30); border-radius: 2px; }"
        "QSlider::handle:vertical { background: #31C27C; width: 14px; height: 14px; margin: 0 -5px; border-radius: 7px; }"
        "QSlider::sub-page:vertical { background: #31C27C; border-radius: 2px; }"
        "QSlider::add-page:vertical { background: rgba(255,255,255,20); border-radius: 2px; }");
    connect(volumeSlider, &QSlider::valueChanged, [this](int val) {
        sendCommand(QString("volume %1 1").arg(val));
    });

    volPopupLayout->addWidget(muteBtn);
    volPopupLayout->addWidget(volumeSlider, 1);

    rightLayout->addLayout(funcLayout);
    barLayout->addLayout(rightLayout);

    mainLayout->addWidget(playerBar);

    return playerPage;
}

// ============ 设置页 ============
QWidget* MainWindow::createSettingsPage()
{
    settingsPage = new QWidget();
    settingsPage->setStyleSheet("background: qlineargradient(x1:0,y1:0,x2:0.5,y2:1,"
        "stop:0 #0a1628, stop:0.3 #0d2137, stop:0.7 #122a3e, stop:1 #0f2530);");

    QVBoxLayout *layout = new QVBoxLayout(settingsPage);
    layout->setContentsMargins(40, 20, 40, 20);

    QLabel *title = new QLabel("设置", settingsPage);
    title->setStyleSheet("color: white; font-size: 22px; font-weight: bold;");
    layout->addWidget(title);
    layout->addSpacing(20);

    // 播放设置
    QLabel *playSection = new QLabel("播放设置", settingsPage);
    playSection->setStyleSheet("color: #31C27C; font-size: 14px; font-weight: bold;");
    layout->addWidget(playSection);

    auto addSettingRow = [&](const QString &label, const QStringList &options, int current, std::function<void(int)> callback) {
        QWidget *row = new QWidget(settingsPage);
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
        layout->addWidget(row);
        layout->addSpacing(8);
    };

    addSettingRow("循环模式", {"列表循环", "单曲循环", "随机播放"}, loopMode, [this](int idx) { setLoopMode(idx); });
    addSettingRow("均衡器", {"默认", "摇滚", "流行", "古典", "低音"}, eqMode, [this](int idx) { setEqMode(idx); });

    // 音量
    QWidget *volRow = new QWidget(settingsPage);
    volRow->setFixedHeight(48);
    QHBoxLayout *volLayout = new QHBoxLayout(volRow);
    volLayout->setContentsMargins(16, 0, 16, 0);
    volRow->setStyleSheet("QWidget { background: rgba(255,255,255,5); border-radius: 8px; }");
    QLabel *volLabel = new QLabel("音量", volRow);
    volLabel->setStyleSheet("color: rgba(255,255,255,180); font-size: 14px;");
    QSlider *volSlider = new QSlider(Qt::Horizontal, volRow);
    volSlider->setRange(0, 100);
    volSlider->setValue(volumeSlider ? volumeSlider->value() : 75);
    volSlider->setMinimumSize(80, 28);
    volSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 6px; background: rgba(255,255,255,30); border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #31C27C; width: 18px; height: 18px; margin: -6px 0; border-radius: 9px; }"
        "QSlider::sub-page:horizontal { background: #31C27C; border-radius: 3px; }");
    connect(volSlider, &QSlider::valueChanged, [this](int val) { setVolume(val); });
    volLayout->addWidget(volLabel);
    volLayout->addWidget(volSlider, 1);
    layout->addWidget(volRow);
    layout->addSpacing(16);

    // 界面设置
    QLabel *uiSection = new QLabel("界面设置", settingsPage);
    uiSection->setStyleSheet("color: #31C27C; font-size: 14px; font-weight: bold;");
    layout->addWidget(uiSection);

    addSettingRow("主题", {"标准", "夜间", "车载"}, themeMode, [this](int idx) { setThemeMode(idx); });
    addSettingRow("歌词模式", {"双语", "中文", "英文", "隐藏"}, lyricsMode, [this](int idx) { setLyricsMode(idx); });

    layout->addSpacing(20);

    // 关于
    QLabel *aboutSection = new QLabel("关于", settingsPage);
    aboutSection->setStyleSheet("color: #31C27C; font-size: 14px; font-weight: bold;");
    layout->addWidget(aboutSection);

    QWidget *aboutRow = new QWidget(settingsPage);
    aboutRow->setStyleSheet("QWidget { background: rgba(255,255,255,5); border-radius: 8px; }");
    QVBoxLayout *aboutLayout = new QVBoxLayout(aboutRow);
    aboutLayout->setContentsMargins(16, 12, 16, 12);
    QLabel *verLabel = new QLabel("版本: v1.0.0  |  ARM Cortex-A72", aboutRow);
    verLabel->setStyleSheet("color: rgba(255,255,255,120); font-size: 13px;");
    QLabel *devLabel = new QLabel("基于 mplayer + Qt5 嵌入式音乐播放器", aboutRow);
    devLabel->setStyleSheet("color: rgba(255,255,255,80); font-size: 12px;");
    aboutLayout->addWidget(verLabel);
    aboutLayout->addWidget(devLabel);
    layout->addWidget(aboutRow);

    layout->addStretch();

    return settingsPage;
}

// ============ mplayer ============
void MainWindow::initMplayer()
{
    // 先删除旧的有名管道，避免权限问题
    QString appDir = QStringLiteral("/home/edu/work/Project/project02/music_player");
    QString fifoPath = appDir + "/fifo_cmd";
    unlink(fifoPath.toUtf8().constData());

    // 创建新的有名管道
    if (mkfifo(fifoPath.toUtf8().constData(), 0666) == -1) {
        qDebug() << "mkfifo failed or already exists";
    }

    mplayerProcess = new QProcess(this);
    mplayerProcess->setProcessChannelMode(QProcess::MergedChannels);

    QStringList arguments;
    arguments << "-slave" << "-quiet" << "-idle"
              << "-input" << ("file=" + fifoPath);
    mplayerProcess->start("mplayer", arguments);

    if (!mplayerProcess->waitForStarted(3000)) {
        qDebug() << "Failed to start mplayer!";
        songTitleLabel->setText("mplayer启动失败");
        return;
    }

    connect(mplayerProcess, &QProcess::readyReadStandardOutput,
            this, &MainWindow::readMplayerOutput);
}

void MainWindow::readMplayerOutput()
{
    while (mplayerProcess->canReadLine()) {
        QString line = QString::fromUtf8(mplayerProcess->readLine()).trimmed();

        if (line.contains("ANS_TIME_POSITION=")) {
            QStringList parts = line.split("=");
            if (parts.size() >= 2) {
                float pos = parts[1].toFloat();
                float delta = pos - lastPosition;

                if (isPlaying && totalDuration > 0 && pos > 0.5) {
                    if (pos >= totalDuration - 1.0 || (delta < -0.5 && lastPosition > 0)) {
                        onSongFinished();
                        lastPosition = 0;
                        return;
                    }
                }

                lastPosition = pos;
                currentPosition = static_cast<int>(pos);

                if (!isSliderPressed) {
                    curTimeLabel->setText(formatTime(currentPosition));
                    if (totalDuration > 0) {
                        progressSlider->setValue((currentPosition * 1000) / totalDuration);
                    }
                    emit progressUpdated(currentPosition, totalDuration);
                }
            }
        }

        if (line.contains("ANS_LENGTH=")) {
            QStringList parts = line.split("=");
            if (parts.size() >= 2) {
                totalDuration = static_cast<int>(parts[1].toFloat());
                totalTimeLabel->setText(formatTime(totalDuration));
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

// ============ 歌曲管理 ============
void MainWindow::readSongMeta(const QString &filePath, QString &title, QString &artist)
{
    title = QFileInfo(filePath).completeBaseName();
    artist = "未知歌手";

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;

    // 尝试 ID3v1（文件末尾 128 字节）
    if (file.size() >= 128) {
        file.seek(file.size() - 128);
        QByteArray tag = file.read(128);
        if (tag.left(3) == "TAG") {
            QString t = QString::fromLocal8Bit(tag.mid(3, 30)).trimmed();
            QString a = QString::fromLocal8Bit(tag.mid(33, 30)).trimmed();
            if (!t.isEmpty()) title = t;
            if (!a.isEmpty()) artist = a;
            file.close();
            return;
        }
    }

    // 尝试 ID3v2（文件开头）
    file.seek(0);
    QByteArray header = file.read(10);
    if (header.left(3) == "ID3") {
        int majorVer = (unsigned char)header[3];
        int size = ((header[6] & 0x7F) << 21) |
                   ((header[7] & 0x7F) << 14) |
                   ((header[8] & 0x7F) << 7) |
                   (header[9] & 0x7F);

        QByteArray id3data = file.read(size);
        int pos = 0;
        bool foundTitle = false, foundArtist = false;

        while (pos + 10 <= id3data.size() && (!foundTitle || !foundArtist)) {
            QByteArray frameId = id3data.mid(pos, 4);
            if (frameId[0] == 0) break; // padding

            int frameSize;
            if (majorVer >= 4) { // ID3v2.4 uses syncsafe integers
                frameSize = ((unsigned char)id3data[pos+4] << 21) |
                           ((unsigned char)id3data[pos+5] << 14) |
                           ((unsigned char)id3data[pos+6] << 7) |
                           (unsigned char)id3data[pos+7];
            } else {
                frameSize = ((unsigned char)id3data[pos+4] << 24) |
                           ((unsigned char)id3data[pos+5] << 16) |
                           ((unsigned char)id3data[pos+6] << 8) |
                           (unsigned char)id3data[pos+7];
            }

            if (frameSize <= 0 || pos + 10 + frameSize > id3data.size()) break;

            QByteArray frameData = id3data.mid(pos + 10, frameSize);

            auto parseTextFrame = [](const QByteArray &data) -> QString {
                if (data.size() <= 1) return "";
                char encoding = data[0];
                QByteArray textData = data.mid(1);
                // 移除末尾的 null 字节
                while (textData.size() > 0 && textData.back() == 0)
                    textData.chop(1);

                if (encoding == 0) return QString::fromLatin1(textData).trimmed();
                if (encoding == 3) return QString::fromUtf8(textData).trimmed();
                // encoding 1/2 = UTF-16
                return QString::fromUtf8(textData).trimmed();
            };

            if (frameId == "TIT2" && !foundTitle) {
                title = parseTextFrame(frameData);
                if (!title.isEmpty()) foundTitle = true;
            } else if (frameId == "TPE1" && !foundArtist) {
                artist = parseTextFrame(frameData);
                if (!artist.isEmpty()) foundArtist = true;
            }

            pos += 10 + frameSize;
        }
    }

    file.close();

    if (title.isEmpty()) title = QFileInfo(filePath).completeBaseName();
    if (artist.isEmpty()) artist = "未知歌手";
}

void MainWindow::scanSongs()
{
    songList.clear();
    songNames.clear();
    songTitles.clear();
    songArtists.clear();
    QDir dir(QStringLiteral("/home/edu/work/Project/project02/music_player") + "/song");

    if (!dir.exists()) return;

    QStringList filters;
    filters << "*.mp3" << "*.wav";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo &fi : fileList) {
        songList.append(fi.absoluteFilePath());
        songNames.append(fi.completeBaseName());

        // 读取 ID3 标签
        QString title, artist;
        readSongMeta(fi.absoluteFilePath(), title, artist);
        songTitles.append(title);
        songArtists.append(artist);
    }

    // 更新歌曲列表页
    if (songsListWidget) {
        songsListWidget->clear();
        for (int i = 0; i < songTitles.size(); i++) {
            songsListWidget->addItem(songTitles[i] + " - " + songArtists[i]);
        }
    }
}

void MainWindow::playSong(int index)
{
    if (index < 0 || index >= songList.size()) return;

    currentSongIndex = index;
    sendLoadFile(songList[index]);

    isPlaying = true;
    playBtn->setText("暂停");

    QString title = songTitles[index];
    QString artist = songArtists[index];
    songTitleLabel->setText(title);
    songArtistLabel->setText(artist);
    barNameLabel->setText(title);
    if (homeNowPlaying) homeNowPlaying->setText(title);
    plInfoLabel->setText(QString("共 %1 首 · 正在播放: %2").arg(songList.size()).arg(title));

    // 高亮播放列表
    if (playlistList) playlistList->setCurrentRow(index);
    if (songsListWidget) songsListWidget->setCurrentRow(index);

    // 收藏状态
    isLiked = favorites.contains(songNames[index]);

    // 重置进度
    currentPosition = 0;
    lastPosition = 0;
    totalDuration = 0;
    progressSlider->setValue(0);
    curTimeLabel->setText("00:00");
    totalTimeLabel->setText("00:00");

    loadLyrics(songList[index]);
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
    if (volumeSlider) volumeSlider->setValue(vol);
    sendCommand(QString("volume %1 1").arg(vol));
}

QString MainWindow::getCurrentSongName() const
{
    if (currentSongIndex >= 0 && currentSongIndex < songTitles.size())
        return songTitles[currentSongIndex];
    return "";
}

// ============ 按钮槽 ============
void MainWindow::on_playBtn_clicked()
{
    if (songList.isEmpty()) return;
    if (!isPlaying) {
        if (totalDuration == 0) playSong(currentSongIndex);
        else { sendCommand("pause"); isPlaying = true; playBtn->setText("暂停"); }
    } else {
        sendCommand("pause"); isPlaying = false; playBtn->setText("播放");
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
    likeBtn->setStyleSheet(isLiked
        ? "QPushButton { background: transparent; border: none; color: #e74c3c; font-size: 22px; padding: 6px; }"
        : "QPushButton { background: transparent; border: none; color: rgba(255,255,255,80); font-size: 20px; padding: 6px; }");

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
    if (favoritesListWidget) {
        favoritesListWidget->clear();
        for (const QString &favName : favorites) {
            // 查找对应的标题
            QString displayTitle = favName;
            for (int i = 0; i < songNames.size(); i++) {
                if (songNames[i] == favName) {
                    displayTitle = songTitles[i] + " - " + songArtists[i];
                    break;
                }
            }
            favoritesListWidget->addItem("♥ " + displayTitle);
        }
    }
}

// ============ 设置 ============
void MainWindow::setLoopMode(int mode)
{
    loopMode = mode;
    switch (mode) {
    case 0: loopBtn->setText("列表循环"); break;
    case 1: loopBtn->setText("单曲循环"); break;
    case 2: loopBtn->setText("随机播放"); break;
    }
}

void MainWindow::setThemeMode(int mode)
{
    themeMode = mode;
    // 主题切换由各页面自己处理
}

void MainWindow::setLyricsMode(int mode)
{
    lyricsMode = mode;
    // 重新过滤歌词
    filteredLyrics.clear();
    if (lyricsList) lyricsList->clear();

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
            if (lyricsList) lyricsList->addItem(l.text);
        }
    }
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
}

// ============ 歌词 ============
void MainWindow::loadLyrics(const QString &songPath)
{
    lyrics.clear();
    filteredLyrics.clear();
    currentLyricIndex = -1;
    if (lyricsList) lyricsList->clear();

    QFileInfo fi(songPath);
    QString lrcPath = fi.path() + "/" + fi.completeBaseName() + ".lrc";

    QFile file(lrcPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (lyricsList) lyricsList->addItem("暂无歌词");
        return;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");
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

    // 排序
    for (int i = 0; i < lyrics.size() - 1; i++)
        for (int j = i + 1; j < lyrics.size(); j++)
            if (lyrics[j].timeMs < lyrics[i].timeMs) qSwap(lyrics[i], lyrics[j]);

    setLyricsMode(lyricsMode);
}

// ============ 配置 ============
void MainWindow::loadFavorites()
{
    favorites.clear();
    QString favPath = QStringLiteral("/home/edu/work/Project/project02/music_player") + "/favorites.txt";
    QFile file(favPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) favorites.insert(line);
    }
    file.close();

    // 刷新收藏页
    if (favoritesListWidget) {
        favoritesListWidget->clear();
        for (const QString &favName : favorites) {
            QString displayTitle = favName;
            for (int i = 0; i < songNames.size(); i++) {
                if (songNames[i] == favName) {
                    displayTitle = songTitles[i] + " - " + songArtists[i];
                    break;
                }
            }
            favoritesListWidget->addItem("♥ " + displayTitle);
        }
    }
}

void MainWindow::saveFavorites()
{
    QString favPath = QStringLiteral("/home/edu/work/Project/project02/music_player") + "/favorites.txt";
    QFile file(favPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    for (const QString &name : favorites) out << name << "\n";
    file.close();
}

void MainWindow::loadConfig()
{
    QString configPath = QStringLiteral("/home/edu/work/Project/project02/music_player") + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    currentSongIndex = settings.value("player/songIndex", 0).toInt();
    int vol = settings.value("player/volume", 75).toInt();
    loopMode = settings.value("player/loopMode", 0).toInt();
    themeMode = settings.value("ui/theme", 0).toInt();
    lyricsMode = settings.value("ui/lyricsMode", 0).toInt();
    eqMode = settings.value("player/eqMode", 0).toInt();

    if (volumeSlider) volumeSlider->setValue(vol);
    setLoopMode(loopMode);
}

void MainWindow::saveConfig()
{
    QString configPath = QStringLiteral("/home/edu/work/Project/project02/music_player") + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    settings.setValue("player/songIndex", currentSongIndex);
    settings.setValue("player/volume", volumeSlider ? volumeSlider->value() : 75);
    settings.setValue("player/loopMode", loopMode);
    settings.setValue("ui/theme", themeMode);
    settings.setValue("ui/lyricsMode", lyricsMode);
    settings.setValue("player/eqMode", eqMode);
}

void MainWindow::sendCommand(const QString &cmd)
{
    if (!mplayerProcess || mplayerProcess->state() == QProcess::NotRunning) return;
    QString fifoPath = QStringLiteral("/home/edu/work/Project/project02/music_player") + "/fifo_cmd";
    FILE *fifo = fopen(fifoPath.toUtf8().constData(), "w");
    if (fifo) { fprintf(fifo, "%s\n", cmd.toUtf8().constData()); fflush(fifo); fclose(fifo); }
}

void MainWindow::sendLoadFile(const QString &path)
{
    sendCommand(QString("loadfile \"%1\"").arg(path));
}

void MainWindow::updateProgress()
{
    if (!isPlaying || !mplayerProcess) return;
    sendCommand("get_time_pos");
    sendCommand("get_time_length");
}

QString MainWindow::formatTime(int seconds)
{
    return QString("%1:%2").arg(seconds / 60, 2, 10, QChar('0')).arg(seconds % 60, 2, 10, QChar('0'));
}
