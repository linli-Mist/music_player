#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QProcess>
#include <QTimer>
#include <QVector>
#include <QSet>
#include <QModelIndex>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QListWidget>
#include <QLineEdit>
#include <QMap>

#include <QPixmap>
#include <QTransform>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QDir>
#include <QFileInfo>

namespace Ui {
class MainWindow;
}

class CustomKeyboard;  // 前向声明
class QComboBox;       // 前向声明（设置页ComboBox）

struct LyricLine {
    int timeMs;
    QString text;
    bool isChinese;
};

// ========== 旋转封面控件 ==========
class RotatingCover : public QWidget {
    Q_OBJECT
public:
    explicit RotatingCover(QWidget *parent = nullptr);
    void setAngle(double angle);
    double angle() const { return m_angle; }
    void setAccentColor(const QColor &color);
    void setCoverImage(const QPixmap &pixmap);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void generateBasePixmap();
    QPixmap m_basePixmap;
    QPixmap m_coverImage;
    double m_angle;
    QColor m_accentColor;
    int m_size;
};

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    bool eventFilter(QObject *obj, QEvent *event) override;

    // 公共接口 - 供各页面调用
    void playSong(int index, bool addToHistory = true);
    void playNext();
    void playPrev();
    void togglePlay();
    void setVolume(int vol);
    bool isSongPlaying() const { return isPlaying; }
    int getCurrentSongIndex() const { return currentSongIndex; }
    QVector<QString> getSongList() const { return songNames; }
    QSet<QString> getFavorites() const { return favorites; }
    void toggleFavorite(int index);
    int getTotalDuration() const { return totalDuration; }
    int getCurrentPosition() const { return currentPosition; }
    int getLoopMode() const { return loopMode; }
    void setLoopMode(int mode);
    int getThemeMode() const { return themeMode; }
    void setThemeMode(int mode);
    int getLyricsMode() const { return lyricsMode; }
    void setLyricsMode(int mode);
    int getEqMode() const { return eqMode; }
    void setEqMode(int mode);
    void setPlaybackSpeed(double speed);

signals:
    void songChanged(int index);
    void playStateChanged(bool playing);
    void progressUpdated(int current, int total);

private slots:
    void updateProgress();
    void readMplayerOutput();

private:
    void setupConnections();
    void initDynamicUI();
    void initMplayer();
    void scanSongs();
    QString findSongDir();
    void sendCommand(const QString &cmd);
    void sendLoadFile(const QString &path);
    void loadLyrics(const QString &songPath);
    void updateLyricsHighlight(int currentTimeMs);
    void updateCoverRotation();
    void loadFavorites();
    void saveFavorites();
    void loadConfig();
    void saveConfig();
    void onSongFinished();
    void on_playBtn_clicked();
    void on_volumeBtn_clicked();
    void on_likeBtn_clicked();
    void on_loopBtn_clicked();
    void updateTabHighlight(int idx);
    QString formatTime(int seconds);

    // 新增功能函数
    void loadRecentPlayed();
    void saveRecentPlayed();
    void addToRecent(int index);
    QColor extractDominantColor(const QString &imagePath);
    void applyAccentColor(const QColor &color);
    void showLongPressMenu(QListWidget *list, const QPoint &pos);
    QString getPinyinInitials(const QString &text);
    void sortSongs(int mode);
    void loadPlayCounts();
    void savePlayCounts();
    void updateSleepDisplay();
    void refreshHomePage();  // #5修复: 刷新首页每日推荐和最近播放
    void updateGreeting();   // 更新问候语和时间日期
    static QPixmap makeRoundedPixmap(const QPixmap &src, int size, int radius = 8);

    // UI（由 .ui 文件生成）
    Ui::MainWindow *ui;

    // 自定义软键盘（浮动窗口，不在 .ui 中）
    CustomKeyboard *m_keyboard;

    // mplayer
    QProcess *mplayerProcess;

    // 定时器
    QTimer *progressTimer;
    QTimer *coverAnimTimer;

    // 歌曲数据
    QVector<QString> songList;     // 完整路径
    QVector<QString> songNames;    // 文件名(不含扩展名)
    QVector<QString> songTitles;   // ID3 标题(或文件名)
    QVector<QString> songArtists;  // ID3 歌手(或"未知歌手")
    int currentSongIndex;

    // 歌词
    QVector<LyricLine> lyrics;
    QVector<LyricLine> filteredLyrics;
    int currentLyricIndex;
    int lyricsMode;

    // 收藏
    QSet<QString> favorites;

    // 播放状态
    bool isPlaying;
    bool songEnded;  // 防止歌曲结束重复触发
    bool isSliderPressed;
    bool isMuted;
    bool isLiked;
    int loopMode;
    int themeMode;
    int eqMode;
    int totalDuration;
    int currentPosition;
    float lastPosition;
    double playbackSpeed;  // 播放速度 0.5~2.0

    // 睡眠定时器
    QTimer *sleepTimer;
    int sleepRemaining;  // 剩余秒数
    int sleepDuration;   // 设定的分钟数（0=关闭）

    // 封面动画
    double coverAngle;
    QColor accentColor;

    // Tab 按钮（通过 ui->tabBtn_N 访问，这里只存指针数组方便遍历）
    QVector<QPushButton*> tabButtons;
    int currentTabIndex;

    QString fifoPath;        // FIFO路径（单一来源，#2修复）

    // 歌曲排序
    int sortMode;  // 0=默认 1=名称 2=歌手 3=最近播放 4=播放次数
    QMap<QString, int> playCounts;  // 播放次数统计

    // 播放历史（随机模式下支持「上一首」回到历史记录，#15修复）
    QVector<int> playHistory;
    int playHistoryIdx;

    // 设置页ComboBox指针（双向同步用，#17修复）
    QComboBox *settingsLoopCombo;
    QComboBox *settingsEqCombo;
    QComboBox *settingsSpeedCombo;
    QComboBox *settingsLyricsCombo;
    QComboBox *settingsThemeCombo;

    // 长按菜单
    QTimer *longPressTimer;
    QPoint longPressStartPos;
    bool longPressTriggered;

    // 最近播放
    QVector<QString> recentPlayed;

    // 音量弹出面板（由代码定位，不在 .ui 中）
    QWidget *volumePopup;
    QSlider *volumeSlider;
    QLabel *volumeLabel;

    // 播放列表面板（由代码定位，不在 .ui 中）
    QWidget *playlistPanel;
    QLabel *plInfoLabel;
    QListWidget *playlistList;
};

#endif // MAINWINDOW_H
