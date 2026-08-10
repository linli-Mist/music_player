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

namespace Ui {
class MainWindow;
}

struct LyricLine {
    int timeMs;
    QString text;
    bool isChinese;
};

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 公共接口 - 供各页面调用
    void playSong(int index);
    void playNext();
    void playPrev();
    void togglePlay();
    void setVolume(int vol);
    bool isSongPlaying() const { return isPlaying; }
    int getCurrentSongIndex() const { return currentSongIndex; }
    QString getCurrentSongName() const;
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

signals:
    void songChanged(int index);
    void playStateChanged(bool playing);
    void progressUpdated(int current, int total);

private slots:
    void updateProgress();
    void readMplayerOutput();

private:
    void initUI();
    void initMplayer();
    void scanSongs();
    void readSongMeta(const QString &filePath, QString &title, QString &artist);
    void sendCommand(const QString &cmd);
    void sendLoadFile(const QString &path);
    void loadLyrics(const QString &songPath);
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

    // UI 构建
    QWidget* createHomePage();
    QWidget* createSongsPage();
    QWidget* createFavoritesPage();
    QWidget* createPlayerPage();
    QWidget* createSettingsPage();
    QWidget* createTabBar();

    Ui::MainWindow *ui;

    // 页面
    QStackedWidget *stackedWidget;

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
    bool isSliderPressed;
    bool isMuted;
    bool isLiked;
    int loopMode;
    int themeMode;
    int eqMode;
    int totalDuration;
    int currentPosition;
    float lastPosition;

    // 封面动画
    int coverAnimStep;

    // UI 控件指针（各页面的控件）
    // 播放页
    QWidget *playerPage;
    QLabel *coverLabel;
    QLabel *songTitleLabel;
    QLabel *songArtistLabel;
    QListWidget *lyricsList;
    QWidget *playerBar;
    QLabel *barThumbLabel;
    QLabel *barNameLabel;
    QPushButton *playBtn;
    QPushButton *prevBtn;
    QPushButton *nextBtn;
    QPushButton *volumeBtn;
    QPushButton *loopBtn;
    QPushButton *likeBtn;
    QPushButton *playlistBtn;
    QWidget *volumePopup;
    QSlider *progressSlider;
    QSlider *volumeSlider;
    QLabel *curTimeLabel;
    QLabel *totalTimeLabel;
    QWidget *playlistPanel;
    QListWidget *playlistList;
    QLabel *plInfoLabel;

    // 首页
    QWidget *homePage;
    QLabel *homeNowPlaying;

    // 歌曲页
    QWidget *songsPage;
    QListWidget *songsListWidget;
    QLineEdit *searchEdit;

    // 收藏页
    QWidget *favoritesPage;
    QListWidget *favoritesListWidget;

    // 设置页
    QWidget *settingsPage;

    // Tab 按钮
    QVector<QPushButton*> tabButtons;
    int currentTabIndex;
};

#endif // MAINWINDOW_H
