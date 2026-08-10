#include "widget.h"
#include "ui_widget.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QTextStream>
#include <QRegExp>
#include <QFile>
#include <QAbstractItemView>
#include <QSettings>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <cstdlib>
#include <ctime>
#include <cmath>

// POSIX headers for mkfifo/pipe
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <unistd.h>
#include <cerrno>
#include <cstring>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , mplayerProcess(nullptr)
    , currentSongIndex(0)
    , currentLyricIndex(-1)
    , lyricsMode(0)
    , isPlaying(false)
    , isSliderPressed(false)
    , playlistVisible(false)
    , isMuted(false)
    , isLiked(false)
    , loopMode(0)
    , themeMode(0)
    , eqMode(0)
    , totalDuration(0)
    , currentPosition(0)
    , lastPosition(0)
    , m_coverOpacity(1.0)
    , coverAnimStep(0)
    , volumePopup(nullptr)
    , volumePopupVisible(false)
{
    ui->setupUi(this);

    // 固定窗口大小
    setFixedSize(1024, 600);

    // 初始化随机数
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

    // 创建定时器，每200ms更新进度
    progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, this, &Widget::updateProgress);
    progressTimer->start(200);

    // 封面动画定时器
    coverAnimTimer = new QTimer(this);
    connect(coverAnimTimer, &QTimer::timeout, this, &Widget::updateCoverAnimation);
    coverAnimTimer->start(100);
}

Widget::~Widget()
{
    saveConfig();
    if (mplayerProcess && mplayerProcess->state() != QProcess::NotRunning) {
        sendCommand("quit");
        mplayerProcess->waitForFinished(2000);
        mplayerProcess->kill();
    }
    delete ui;
}

void Widget::initUI()
{
    // 嵌入式适配：全屏 + 无窗口边框
    setWindowFlags(Qt::FramelessWindowHint);
    setFixedSize(1024, 600);

    // 进度条和音量
    ui->progressSlider->setRange(0, 1000);
    ui->progressSlider->setValue(0);
    ui->volumeSlider->setRange(0, 100);
    ui->volumeSlider->setValue(75);

    // 初始文字
    ui->curTimeLabel->setText("00:00");
    ui->totalTimeLabel->setText("00:00");
    ui->songTitleLabel->setText("未播放");
    ui->songArtistLabel->setText("");
    ui->barNameLabel->setText("未播放");
    ui->barArtistLabel->setText("");

    // 封面
    ui->coverLabel->setStyleSheet(
        "QLabel {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "    stop:0 #1a4a3a, stop:0.5 #2d6b4f, stop:1 #0f3a28);"
        "  border-radius: 16px;"
        "  font-size: 56px;"
        "  color: rgba(255,255,255,30);"
        "}"
    );
    ui->coverLabel->setText("音乐");

    // 底部栏
    ui->playerBar->setStyleSheet(
        "QWidget#playerBar {"
        "  background: rgba(0,0,0,200);"
        "  border-top: 1px solid rgba(255,255,255,10);"
        "}"
    );

    // 歌词列表
    ui->lyricsList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->lyricsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->lyricsList->setStyleSheet(
        "QListWidget { background: transparent; border: none; color: rgba(255,255,255,40); }"
        "QListWidget::item { padding: 10px 12px; border: none; }"
        "QListWidget::item:selected { background: transparent; }"
    );

    // 播放列表面板
    ui->playlistPanel->setStyleSheet(
        "QWidget#playlistPanel {"
        "  background: rgba(20,20,25,240);"
        "  border-left: 1px solid rgba(255,255,255,20);"
        "}"
    );
    ui->plTitleLabel->setStyleSheet("color: white; font-weight: bold; font-size: 16px;");
    ui->plInfoLabel->setStyleSheet("color: rgba(255,255,255,80); font-size: 12px;");

    // 播放按钮
    ui->playBtn->setMinimumSize(44, 44);
    ui->playBtn->setStyleSheet(
        "QPushButton {"
        "  background: #31C27C;"
        "  border-radius: 22px;"
        "  color: white;"
        "  font-size: 20px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:pressed { background: #269e65; }"
    );

    // 控制按钮
    QString ctrlStyle =
        "QPushButton { background: transparent; border: none; color: rgba(255,255,255,160); font-size: 18px; padding: 8px; }"
        "QPushButton:pressed { color: white; }";
    ui->prevBtn->setMinimumSize(48, 48);
    ui->nextBtn->setMinimumSize(48, 48);
    ui->volumeBtn->setMinimumSize(48, 48);
    ui->loopBtn->setMinimumSize(64, 48);
    ui->playlistBtn->setMinimumSize(48, 48);
    ui->prevBtn->setStyleSheet(ctrlStyle);
    ui->nextBtn->setStyleSheet(ctrlStyle);
    ui->volumeBtn->setStyleSheet(ctrlStyle);
    ui->loopBtn->setStyleSheet(ctrlStyle);
    ui->playlistBtn->setStyleSheet(ctrlStyle);

    // 爱心
    ui->likeBtn->setMinimumSize(40, 40);
    ui->likeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; color: rgba(255,255,255,80); font-size: 20px; padding: 6px; }"
        "QPushButton:pressed { color: #e74c3c; }"
    );

    // 模式按钮
    ui->modeBtn->setMinimumSize(44, 28);
    ui->modeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: 1px solid rgba(255,255,255,50);"
        "  border-radius: 4px; color: rgba(255,255,255,120); font-size: 11px; }"
        "QPushButton:pressed { border-color: #31C27C; color: #31C27C; }"
    );
    ui->lyricsBtn->setMinimumSize(40, 40);
    ui->eqBtn->setMinimumSize(40, 40);
    ui->lyricsBtn->setStyleSheet(ctrlStyle);
    ui->eqBtn->setStyleSheet(ctrlStyle);

    // 滑块
    QString sliderStyle =
        "QSlider::groove:horizontal { height: 4px; background: rgba(255,255,255,30); border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #31C27C; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; }"
        "QSlider::sub-page:horizontal { background: #31C27C; border-radius: 2px; }"
        "QSlider::add-page:horizontal { background: rgba(255,255,255,20); border-radius: 2px; }";
    ui->progressSlider->setStyleSheet(sliderStyle);

    // 隐藏原 .ui 中的音量滑块，改用弹出面板
    ui->volumeSlider->setVisible(false);

    // 音量弹出面板（竖向布局）- 放在音量按钮正上方
    volumePopup = new QWidget(this);
    volumePopup->setFixedSize(60, 200);
    volumePopup->setVisible(false);
    volumePopup->setStyleSheet("QWidget { background: rgba(15,15,20,240);"
        "border: 1px solid rgba(255,255,255,30); border-radius: 12px; }");
    // 音量按钮绝对位置: x=618, y=545 (playerBar y=520 + 按钮 y=25)
    // 面板中心对齐按钮中心: x=618+16-30=604
    // 面板底部紧贴按钮上方: y=545-200-5=340
    volumePopup->move(604, 340);

    QVBoxLayout *volPopupLayout = new QVBoxLayout(volumePopup);
    volPopupLayout->setContentsMargins(10, 12, 10, 12);
    volPopupLayout->setSpacing(8);

    QPushButton *muteBtn = new QPushButton("🔊", volumePopup);
    muteBtn->setFixedSize(36, 36);
    muteBtn->setStyleSheet("QPushButton { background: transparent; border: none;"
        "font-size: 20px; color: rgba(255,255,255,180); }"
        "QPushButton:pressed { color: #31C27C; }");

    QSlider *popupVolumeSlider = new QSlider(Qt::Vertical, volumePopup);
    popupVolumeSlider->setRange(0, 100);
    popupVolumeSlider->setValue(ui->volumeSlider->value());
    popupVolumeSlider->setMinimumSize(28, 120);
    popupVolumeSlider->setInvertedAppearance(true);  // 最下面是0%，最上面是100%
    popupVolumeSlider->setStyleSheet(
        "QSlider::groove:vertical { width: 4px; background: rgba(255,255,255,30); border-radius: 2px; }"
        "QSlider::handle:vertical { background: #31C27C; width: 14px; height: 14px; margin: 0 -5px; border-radius: 7px; }"
        "QSlider::sub-page:vertical { background: #31C27C; border-radius: 2px; }"
        "QSlider::add-page:vertical { background: rgba(255,255,255,20); border-radius: 2px; }");
    connect(popupVolumeSlider, &QSlider::valueChanged, [this](int val) {
        sendCommand(QString("volume %1 1").arg(val));
        ui->volumeSlider->setValue(val);
    });
    connect(muteBtn, &QPushButton::clicked, [this, muteBtn, popupVolumeSlider]() {
        isMuted = !isMuted;
        if (isMuted) {
            sendCommand("volume 0 1");
            muteBtn->setText("🔇");
        } else {
            sendCommand(QString("volume %1 1").arg(popupVolumeSlider->value()));
            muteBtn->setText("🔊");
        }
    });

    volPopupLayout->addWidget(muteBtn);
    volPopupLayout->addWidget(popupVolumeSlider, 1);

    // 时间标签
    ui->curTimeLabel->setStyleSheet("color: rgba(255,255,255,120); font-size: 11px;");
    ui->totalTimeLabel->setStyleSheet("color: rgba(255,255,255,120); font-size: 11px;");

    // 歌曲信息
    ui->songTitleLabel->setStyleSheet("color: white; font-weight: bold; font-size: 24px;");
    ui->songArtistLabel->setStyleSheet("color: rgba(255,255,255,130); font-size: 14px;");
    ui->barNameLabel->setStyleSheet("color: rgba(255,255,255,230); font-size: 13px;");
    ui->barArtistLabel->setStyleSheet("color: rgba(255,255,255,100); font-size: 11px;");

    // 封面缩略图
    ui->barThumbLabel->setStyleSheet(
        "QLabel { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #2d6b4f, stop:1 #1a4a3a);"
        "  border-radius: 6px; font-size: 16px; color: rgba(255,255,255,80); }"
    );
    ui->barThumbLabel->setText("乐");

    // 播放列表项
    ui->playlistList->setStyleSheet(
        "QListWidget { background: transparent; border: none; color: rgba(255,255,255,200); }"
        "QListWidget::item { padding: 12px 16px; border-bottom: 1px solid rgba(255,255,255,10); font-size: 13px; }"
        "QListWidget::item:selected { background: rgba(49,194,124,30); color: #31C27C; }"
    );

    // 整体背景
    setStyleSheet(
        "QWidget {"
        "  background: qlineargradient(x1:0, y1:0, x2:0.5, y2:1,"
        "    stop:0 #0a1628, stop:0.3 #0d2137, stop:0.7 #122a3e, stop:1 #0f2530);"
        "}"
    );
}

void Widget::initMplayer()
{
    QString appDir = QStringLiteral("/home/edu/work/Project/project02/music_player");
    QString fifoPath = appDir + "/fifo_cmd";

    qDebug() << "Initializing mplayer...";
    qDebug() << "FIFO path:" << fifoPath;

    // 先删除旧的有名管道，避免权限问题
    if (unlink(fifoPath.toUtf8().constData()) == 0) {
        qDebug() << "Removed old FIFO";
    }

    // 创建新的有名管道
    if (mkfifo(fifoPath.toUtf8().constData(), 0666) == -1) {
        qDebug() << "mkfifo failed:" << strerror(errno);
        return;
    }
    qDebug() << "Created new FIFO";

    mplayerProcess = new QProcess(this);
    mplayerProcess->setProcessChannelMode(QProcess::MergedChannels);

    QStringList arguments;
    arguments << "-slave" << "-quiet" << "-idle"
              << "-input" << ("file=" + fifoPath);
    qDebug() << "Starting mplayer with args:" << arguments;
    mplayerProcess->start("mplayer", arguments);

    if (!mplayerProcess->waitForStarted(3000)) {
        qDebug() << "Failed to start mplayer!";
        ui->songTitleLabel->setText("mplayer启动失败");
        return;
    }

    qDebug() << "mplayer started successfully, PID:" << mplayerProcess->processId();
    connect(mplayerProcess, &QProcess::readyReadStandardOutput,
            this, &Widget::readMplayerOutput);
}

void Widget::readMplayerOutput()
{
    while (mplayerProcess->canReadLine()) {
        QByteArray output = mplayerProcess->readLine();
        QString line = QString::fromUtf8(output).trimmed();

        // 解析播放位置
        if (line.contains("ANS_TIME_POSITION=")) {
            QStringList parts = line.split("=");
            if (parts.size() >= 2) {
                float pos = parts[1].toFloat();
                float delta = pos - lastPosition;

                // 检测歌曲结束：位置回退或停滞
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
                    ui->curTimeLabel->setText(formatTime(currentPosition));
                    if (totalDuration > 0) {
                        int sliderVal = (currentPosition * 1000) / totalDuration;
                        ui->progressSlider->setValue(sliderVal);
                    }
                    updateLyricsHighlight(currentPosition * 1000);
                }
            }
        }

        // 解析总时长
        if (line.contains("ANS_LENGTH=")) {
            QStringList parts = line.split("=");
            if (parts.size() >= 2) {
                totalDuration = static_cast<int>(parts[1].toFloat());
                ui->totalTimeLabel->setText(formatTime(totalDuration));
            }
        }
    }
}

void Widget::onSongFinished()
{
    qDebug() << "Song finished, loopMode=" << loopMode;

    switch (loopMode) {
    case 0: // 列表循环
        playNext();
        break;
    case 1: // 单曲循环
        playSong(currentSongIndex);
        break;
    case 2: // 随机
        if (songList.size() > 1) {
            int newIndex;
            do {
                newIndex = std::rand() % songList.size();
            } while (newIndex == currentSongIndex);
            playSong(newIndex);
        }
        break;
    }
}

void Widget::readSongMeta(const QString &filePath, QString &title, QString &artist)
{
    // 直接使用文件名作为标题，避免复杂的 ID3 解析导致问题
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
        }
    }

    file.close();
}

void Widget::scanSongs()
{
    songList.clear();
    songNames.clear();
    songTitles.clear();
    songArtists.clear();

    QString songDir = QStringLiteral("/home/edu/work/Project/project02/music_player") + "/song";
    QDir dir(songDir);
    qDebug() << "Scanning songs in:" << songDir << "Exists:" << dir.exists();

    if (!dir.exists()) {
        qDebug() << "Song directory not found!";
        return;
    }

    QStringList filters;
    filters << "*.mp3" << "*.wav";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);
    qDebug() << "Found" << fileList.size() << "audio files";

    for (const QFileInfo &fileInfo : fileList) {
        songList.append(fileInfo.absoluteFilePath());
        songNames.append(fileInfo.completeBaseName());

        // 读取 ID3 标签
        QString title, artist;
        readSongMeta(fileInfo.absoluteFilePath(), title, artist);
        songTitles.append(title);
        songArtists.append(artist);

        qDebug() << "Song:" << title << "-" << artist << "(" << fileInfo.fileName() << ")";
    }

    // 更新播放列表显示
    ui->playlistList->clear();
    for (int i = 0; i < songTitles.size(); i++) {
        QString prefix = favorites.contains(songNames[i]) ? "♥ " : "";
        ui->playlistList->addItem(prefix + songTitles[i] + " - " + songArtists[i]);
    }
    ui->plInfoLabel->setText(QString("共 %1 首歌曲").arg(songList.size()));

    // 恢复上次播放
    if (currentSongIndex >= 0 && currentSongIndex < songList.size()) {
        ui->playlistList->setCurrentRow(currentSongIndex);
    }

    qDebug() << "Total songs loaded:" << songList.size();
}

    // 恢复上次播放
    if (currentSongIndex >= 0 && currentSongIndex < songList.size()) {
        ui->playlistList->setCurrentRow(currentSongIndex);
    }

    qDebug() << "Found songs:" << songList.size();
}

void Widget::playSong(int index)
{
    if (index < 0 || index >= songList.size()) return;

    currentSongIndex = index;
    QString songPath = songList[index];
    QString songName = songNames[index];

    sendLoadFile(songPath);

    isPlaying = true;
    ui->playBtn->setText("暂停");

    // 更新界面
    QString title = songTitles[index];
    QString artist = songArtists[index];
    ui->songTitleLabel->setText(title);
    ui->songArtistLabel->setText(artist);
    ui->barNameLabel->setText(title);
    ui->plInfoLabel->setText(QString("共 %1 首 · 正在播放: %2").arg(songList.size()).arg(title));

    // 高亮播放列表
    ui->playlistList->setCurrentRow(index);

    // 更新收藏状态
    isLiked = favorites.contains(songName);
    ui->likeBtn->setStyleSheet(isLiked
        ? "QPushButton { background: transparent; border: none; color: #e74c3c; font-size: 22px; padding: 6px; }"
        : "QPushButton { background: transparent; border: none; color: rgba(255,255,255,80); font-size: 20px; padding: 6px; }");

    // 重置进度
    currentPosition = 0;
    lastPosition = 0;
    totalDuration = 0;
    ui->progressSlider->setValue(0);
    ui->curTimeLabel->setText("00:00");
    ui->totalTimeLabel->setText("00:00");

    // 加载歌词
    loadLyrics(songPath);

    qDebug() << "Playing:" << songPath;
}

void Widget::playNext()
{
    if (songList.isEmpty()) return;

    if (loopMode == 2) { // 随机
        if (songList.size() > 1) {
            int newIndex;
            do {
                newIndex = std::rand() % songList.size();
            } while (newIndex == currentSongIndex);
            playSong(newIndex);
        }
    } else {
        int nextIndex = (currentSongIndex + 1) % songList.size();
        playSong(nextIndex);
    }
}

void Widget::playPrev()
{
    if (songList.isEmpty()) return;

    if (loopMode == 2) { // 随机
        if (songList.size() > 1) {
            int newIndex;
            do {
                newIndex = std::rand() % songList.size();
            } while (newIndex == currentSongIndex);
            playSong(newIndex);
        }
    } else {
        int prevIndex = (currentSongIndex - 1 + songList.size()) % songList.size();
        playSong(prevIndex);
    }
}

void Widget::sendCommand(const QString &cmd)
{
    if (!mplayerProcess || mplayerProcess->state() == QProcess::NotRunning) {
        qDebug() << "Cannot send command - mplayer not running";
        return;
    }

    QString fifoPath = QStringLiteral("/home/edu/work/Project/project02/music_player") + "/fifo_cmd";
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

void Widget::sendLoadFile(const QString &path)
{
    sendCommand(QString("loadfile \"%1\"").arg(path));
}

void Widget::loadLyrics(const QString &songPath)
{
    lyrics.clear();
    filteredLyrics.clear();
    currentLyricIndex = -1;
    ui->lyricsList->clear();

    QFileInfo fi(songPath);
    QString lrcPath = fi.path() + "/" + fi.completeBaseName() + ".lrc";

    QFile file(lrcPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui->lyricsList->addItem("暂无歌词");
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

            int timeMs = min * 60000 + sec * 1000 + ms;

            LyricLine lyric;
            lyric.timeMs = timeMs;
            lyric.text = text.isEmpty() ? "..." : text;
            lyric.isChinese = chineseReg.indexIn(text) >= 0;
            lyrics.append(lyric);

            pos += regex.matchedLength();
        }
    }
    file.close();

    // 按时间排序
    for (int i = 0; i < lyrics.size() - 1; i++) {
        for (int j = i + 1; j < lyrics.size(); j++) {
            if (lyrics[j].timeMs < lyrics[i].timeMs) {
                qSwap(lyrics[i], lyrics[j]);
            }
        }
    }

    // 应用歌词过滤模式
    filterLyricsByMode();
}

void Widget::filterLyricsByMode()
{
    filteredLyrics.clear();
    ui->lyricsList->clear();

    for (const LyricLine &l : lyrics) {
        bool show = false;
        switch (lyricsMode) {
        case 0: show = true; break;                    // 双语
        case 1: show = l.isChinese; break;             // 纯中文
        case 2: show = !l.isChinese; break;            // 纯英文
        case 3: show = false; break;                   // 隐藏
        }

        if (show) {
            filteredLyrics.append(l);
            ui->lyricsList->addItem(l.text);
        }
    }

    if (filteredLyrics.isEmpty() && lyricsMode != 3) {
        ui->lyricsList->addItem("暂无歌词");
    }
}

void Widget::updateLyricsHighlight(int currentTimeMs)
{
    if (filteredLyrics.isEmpty()) return;

    int newIndex = -1;
    for (int i = filteredLyrics.size() - 1; i >= 0; i--) {
        if (currentTimeMs >= filteredLyrics[i].timeMs) {
            newIndex = i;
            break;
        }
    }

    if (newIndex == currentLyricIndex) return;
    currentLyricIndex = newIndex;

    if (currentLyricIndex < 0) return;

    for (int i = 0; i < ui->lyricsList->count(); i++) {
        QListWidgetItem *item = ui->lyricsList->item(i);
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

    ui->lyricsList->setCurrentRow(currentLyricIndex);
    ui->lyricsList->scrollTo(ui->lyricsList->model()->index(currentLyricIndex, 0),
        QAbstractItemView::PositionAtCenter);
}

void Widget::loadFavorites()
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
}

void Widget::saveFavorites()
{
    QString favPath = QStringLiteral("/home/edu/work/Project/project02/music_player") + "/favorites.txt";
    QFile file(favPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    for (const QString &name : favorites) {
        out << name << "\n";
    }
    file.close();
}

void Widget::toggleFavorite(const QString &songName)
{
    if (favorites.contains(songName)) {
        favorites.remove(songName);
        isLiked = false;
    } else {
        favorites.insert(songName);
        isLiked = true;
    }
    saveFavorites();

    // 更新爱心样式
    ui->likeBtn->setStyleSheet(isLiked
        ? "QPushButton { background: transparent; border: none; color: #e74c3c; font-size: 22px; padding: 6px; }"
        : "QPushButton { background: transparent; border: none; color: rgba(255,255,255,80); font-size: 20px; padding: 6px; }");

    // 刷新播放列表
    ui->playlistList->clear();
    for (int i = 0; i < songTitles.size(); i++) {
        QString prefix = favorites.contains(songNames[i]) ? "♥ " : "";
        ui->playlistList->addItem(prefix + songTitles[i] + " - " + songArtists[i]);
    }
    ui->playlistList->setCurrentRow(currentSongIndex);
}

void Widget::loadConfig()
{
    QString configPath = QStringLiteral("/home/edu/work/Project/project02/music_player") + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    currentSongIndex = settings.value("player/songIndex", 0).toInt();
    int vol = settings.value("player/volume", 75).toInt();
    loopMode = settings.value("player/loopMode", 0).toInt();
    themeMode = settings.value("ui/theme", 0).toInt();
    lyricsMode = settings.value("ui/lyricsMode", 0).toInt();
    eqMode = settings.value("player/eqMode", 0).toInt();

    ui->volumeSlider->setValue(vol);
    on_loopBtn_clicked_text_update();
    applyTheme(themeMode);
}

void Widget::saveConfig()
{
    QString configPath = QStringLiteral("/home/edu/work/Project/project02/music_player") + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    settings.setValue("player/songIndex", currentSongIndex);
    settings.setValue("player/volume", ui->volumeSlider->value());
    settings.setValue("player/loopMode", loopMode);
    settings.setValue("ui/theme", themeMode);
    settings.setValue("ui/lyricsMode", lyricsMode);
    settings.setValue("player/eqMode", eqMode);
}

void Widget::applyTheme(int themeId)
{
    switch (themeId) {
    case 0: // 标准
        setStyleSheet(
            "QWidget { background: qlineargradient(x1:0, y1:0, x2:0.5, y2:1,"
            "  stop:0 #0a1628, stop:0.3 #0d2137, stop:0.7 #122a3e, stop:1 #0f2530); }");
        ui->modeBtn->setText("标准");
        break;
    case 1: // 夜间
        setStyleSheet("QWidget { background: #0a0a0a; }");
        ui->modeBtn->setText("夜间");
        break;
    case 2: // 车载(大字体极简)
        setStyleSheet("QWidget { background: #000; }");
        ui->songTitleLabel->setStyleSheet("color: white; font-weight: bold; font-size: 32px;");
        ui->modeBtn->setText("车载");
        break;
    }
}

void Widget::updateCoverAnimation()
{
    if (!isPlaying) return;

    coverAnimStep = (coverAnimStep + 1) % 360;
    // 呼吸灯效果：颜色缓慢变化
    int r = 26 + static_cast<int>(10 * sin(coverAnimStep * 3.14159 / 180));
    int g = 74 + static_cast<int>(20 * sin((coverAnimStep + 120) * 3.14159 / 180));
    int b = 58 + static_cast<int>(15 * sin((coverAnimStep + 240) * 3.14159 / 180));
    ui->coverLabel->setStyleSheet(
        QString("QLabel { background: rgb(%1,%2,%3); border-radius: 16px;"
                "  font-size: 56px; color: rgba(255,255,255,15); }")
        .arg(r).arg(g).arg(b));
}

void Widget::updateProgress()
{
    if (!isPlaying || !mplayerProcess) return;
    sendCommand("get_time_pos");
    sendCommand("get_time_length");
}

// ============ 槽函数 ============

void Widget::on_playBtn_clicked()
{
    if (songList.isEmpty()) return;

    if (!isPlaying) {
        if (totalDuration == 0) {
            playSong(currentSongIndex);
        } else {
            sendCommand("pause");
            isPlaying = true;
            ui->playBtn->setText("暂停");
        }
    } else {
        sendCommand("pause");
        isPlaying = false;
        ui->playBtn->setText("播放");
        ui->coverLabel->setStyleSheet(
            "QLabel { background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
            "  stop:0 #1a4a3a, stop:0.5 #2d6b4f, stop:1 #0f3a28);"
            "  border-radius: 16px; font-size: 56px; color: rgba(255,255,255,30); }");
    }
}

void Widget::on_prevBtn_clicked() { playPrev(); }
void Widget::on_nextBtn_clicked() { playNext(); }

void Widget::on_volumeBtn_clicked()
{
    volumePopupVisible = !volumePopupVisible;
    volumePopup->setVisible(volumePopupVisible);
}

void Widget::on_likeBtn_clicked()
{
    if (currentSongIndex >= 0 && currentSongIndex < songNames.size()) {
        toggleFavorite(songNames[currentSongIndex]);
    }
}

void Widget::on_loopBtn_clicked()
{
    loopMode = (loopMode + 1) % 3;
    on_loopBtn_clicked_text_update();
}

void Widget::on_loopBtn_clicked_text_update()
{
    switch (loopMode) {
    case 0: ui->loopBtn->setText("列表循环"); break;
    case 1: ui->loopBtn->setText("单曲循环"); break;
    case 2: ui->loopBtn->setText("随机播放"); break;
    }
}

void Widget::on_playlistBtn_clicked()
{
    playlistVisible = !playlistVisible;
    ui->playlistPanel->setVisible(playlistVisible);
    ui->playlistBtn->setStyleSheet(playlistVisible
        ? "QPushButton { background: transparent; border: none; color: #31C27C; font-size: 18px; padding: 8px; }"
        : "QPushButton { background: transparent; border: none; color: rgba(255,255,255,160); font-size: 18px; padding: 8px; }");
}

void Widget::on_modeBtn_clicked()
{
    themeMode = (themeMode + 1) % 3;
    applyTheme(themeMode);
}

void Widget::on_lyricsBtn_clicked()
{
    lyricsMode = (lyricsMode + 1) % 4;
    filterLyricsByMode();

    QString modeText;
    switch (lyricsMode) {
    case 0: modeText = "歌词:双语"; break;
    case 1: modeText = "歌词:中文"; break;
    case 2: modeText = "歌词:英文"; break;
    case 3: modeText = "歌词:隐藏"; break;
    }
    ui->lyricsBtn->setText(modeText);
}

void Widget::on_eqBtn_clicked()
{
    eqMode = (eqMode + 1) % 5;

    // 发送mplayer均衡器命令
    switch (eqMode) {
    case 0: sendCommand("af_eq_set_bass=0"); ui->eqBtn->setText("均衡"); break;
    case 1: sendCommand("af_eq_set_bass=8"); ui->eqBtn->setText("摇滚"); break;
    case 2: sendCommand("af_eq_set_bass=4"); ui->eqBtn->setText("流行"); break;
    case 3: sendCommand("af_eq_set_bass=-2"); ui->eqBtn->setText("古典"); break;
    case 4: sendCommand("af_eq_set_bass=12"); ui->eqBtn->setText("低音"); break;
    }
}

void Widget::on_progressSlider_sliderPressed() { isSliderPressed = true; }

void Widget::on_progressSlider_sliderReleased()
{
    isSliderPressed = false;
    int sliderVal = ui->progressSlider->value();
    if (totalDuration > 0) {
        int targetSec = (sliderVal * totalDuration) / 1000;
        sendCommand(QString("seek %1 2").arg(targetSec));
    }
}

void Widget::on_progressSlider_valueChanged(int value) { Q_UNUSED(value); }

void Widget::on_volumeSlider_valueChanged(int value)
{
    sendCommand(QString("volume %1 1").arg(value));
}

void Widget::on_lyricsList_currentRowChanged(int currentRow) { Q_UNUSED(currentRow); }

void Widget::on_playlistList_doubleClicked(const QModelIndex &index)
{
    int row = index.row();
    if (row >= 0 && row < songList.size()) {
        playSong(row);
    }
}

QString Widget::formatTime(int seconds)
{
    int mins = seconds / 60;
    int secs = seconds % 60;
    return QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
}

int Widget::parseLrcTime(const QString &timeStr)
{
    QStringList parts = timeStr.split(":");
    if (parts.size() != 2) return 0;
    int min = parts[0].toInt();
    float sec = parts[1].toFloat();
    return static_cast<int>(min * 60000 + sec * 1000);
}
