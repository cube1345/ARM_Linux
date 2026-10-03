#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

#include <QAtomicInt>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QPair>
#include <QPixmap>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QStringList>
#include <QThread>
#include <QWidget>

class AudioDecodeThread : public QThread
{
    Q_OBJECT
public:
    explicit AudioDecodeThread(QObject *parent = nullptr);
    ~AudioDecodeThread() override;
    void play(const QString &path);
    void stop();
    void setPaused(bool paused);
    void setVolume(int volume);

signals:
    void playbackFinished();

protected:
    void run() override;

private:
    QString m_path;
    QAtomicInt m_running;
    QAtomicInt m_paused;
    QAtomicInt m_volume;
};

class MusicPlayer : public QWidget
{
    Q_OBJECT
public:
    explicit MusicPlayer(QWidget *parent = nullptr);
    ~MusicPlayer() override;

    void start();
    void stopAll();
    void togglePlay();
    void next();
    void prev();
    void setVolume(int volume);
    void playAt(int index);
    int songCount() const { return m_songs.size(); }
    QString songFileName(int i) const
    { return (i >= 0 && i < m_songs.size()) ? QFileInfo(m_songs.at(i)).fileName() : QString(); }
    bool isPlaying() const { return m_playing; }

signals:
    void songChanged(const QString &name);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QStringList m_songs;
    int m_current;
    bool m_playing;
    AudioDecodeThread *m_thread;
    QElapsedTimer m_songTimer;   // 当前曲开始计时（检测秒退）
    int m_quickStops = 0;        // 连续秒退计数，防坏曲自动 next 死循环
    QVector<QPair<QString, qint64>> m_lyrics;   // (歌词行, 时间ms)
    int m_lrcLine = -1;
    QTimer *m_lrcTimer;
    QPixmap m_bgCache;   // 封面/背景缓存，播放中仅重绘文字降低 UI 负担

    void loadSongs();
    void playCurrent();
    void loadLyrics(const QString &songPath);   // 读取同名 .lrc 解析
    void updateLyricLine();                     // 按播放进度找当前歌词行
};

#endif
