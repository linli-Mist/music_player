#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QProcess>
#include <QTimer>
#include <QVector>
#include <QPair>
#include <QSet>
#include <QModelIndex>
#include <QPropertyAnimation>

namespace Ui {
class Widget;
}

// 歌词行结构
struct LyricLine {
    int timeMs;     // 时间戳（毫秒）
    QString text;   // 歌词文本
    bool isChinese; // 是否中文行
};

class Widget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double coverOpacity READ coverOpacity WRITE setCoverOpacity)

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();

    double coverOpacity() const { return m_coverOpacity; }
    void setCoverOpacity(double op) { m_coverOpacity = op; update(); }

private slots:
    // 按钮槽函数
    void on_playBtn_clicked();
    void on_prevBtn_clicked();
    void on_nextBtn_clicked();
    void on_volumeBtn_clicked();
    void on_likeBtn_clicked();
    void on_loopBtn_clicked();
    void on_loopBtn_clicked_text_update();
    void on_playlistBtn_clicked();
    void on_modeBtn_clicked();
    void on_lyricsBtn_clicked();
    void on_eqBtn_clicked();

    // 进度条
    void on_progressSlider_sliderPressed();
    void on_progressSlider_sliderReleased();
    void on_progressSlider_valueChanged(int value);

    // 音量
    void on_volumeSlider_valueChanged(int value);

    // 歌词列表点击
    void on_lyricsList_currentRowChanged(int currentRow);

    // 播放列表双击
    void on_playlistList_doubleClicked(const QModelIndex &index);

    // 定时更新
    void updateProgress();
    void readMplayerOutput();
    void onSongFinished();

private:
    void initUI();
    void initMplayer();
    void scanSongs();
    void readSongMeta(const QString &filePath, QString &title, QString &artist);
    void playSong(int index);
    void playNext();
    void playPrev();

    // mplayer 命令
    void sendCommand(const QString &cmd);
    void sendLoadFile(const QString &path);

    // 歌词
    void loadLyrics(const QString &songPath);
    void updateLyricsHighlight(int currentTimeMs);
    void filterLyricsByMode();

    // 收藏
    void loadFavorites();
    void saveFavorites();
    void toggleFavorite(const QString &songName);

    // 数据持久化
    void loadConfig();
    void saveConfig();

    // 界面模式
    void applyTheme(int themeId);
    void updateCoverAnimation();

    // 工具函数
    QString formatTime(int seconds);
    int parseLrcTime(const QString &timeStr);

    Ui::Widget *ui;

    // mplayer 进程
    QProcess *mplayerProcess;

    // 定时器
    QTimer *progressTimer;
    QTimer *coverAnimTimer;

    // 音量弹出面板
    QWidget *volumePopup;
    bool volumePopupVisible;

    // 歌曲列表
    QVector<QString> songList;
    QVector<QString> songNames;
    QVector<QString> songTitles;   // ID3 标题(或文件名)
    QVector<QString> songArtists;  // ID3 歌手(或"未知歌手")
    int currentSongIndex;

    // 歌词
    QVector<LyricLine> lyrics;
    QVector<LyricLine> filteredLyrics; // 过滤后的歌词
    int currentLyricIndex;
    int lyricsMode; // 0=双语 1=中文 2=英文 3=隐藏

    // 收藏
    QSet<QString> favorites;

    // 播放状态
    bool isPlaying;
    bool isSliderPressed;
    bool playlistVisible;
    bool isMuted;
    bool isLiked;
    int loopMode;    // 0=列表循环 1=单曲循环 2=随机

    // 界面模式
    int themeMode;   // 0=标准 1=夜间 2=车载

    // 均衡器
    int eqMode;      // 0=默认 1=摇滚 2=流行 3=古典 4=低音

    // 进度
    int totalDuration;  // 秒
    int currentPosition; // 秒
    float lastPosition;  // 上次位置（用于检测歌曲结束）

    // 封面动画
    double m_coverOpacity;
    int coverAnimStep;
};

#endif // WIDGET_H
