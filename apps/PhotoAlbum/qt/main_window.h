#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QHash>
#include <QImage>
#include <QMainWindow>

class QLabel;
class QPushButton;
class PhotoView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &photoDirectory, QWidget *parent = nullptr);

private slots:
    void showPrevious();
    void showNext();
    void startCrop();
    void applyCrop();
    void cancelCrop();
    void resetPhoto();
    void savePhoto();

private:
    void buildUi();
    void loadPhotos(const QString &directory);
    void showPhoto();
    void updateActions();
    void updateStatus();
    QImage imageForPath(const QString &path) const;
    static QList<QImage> createDemoPhotos();

    PhotoView *photoView;
    QLabel *titleLabel;
    QLabel *statusLabel;
    QPushButton *previousButton;
    QPushButton *nextButton;
    QPushButton *cropButton;
    QPushButton *applyButton;
    QPushButton *cancelButton;
    QPushButton *resetButton;
    QPushButton *saveButton;

    QStringList photoPaths;
    QHash<QString, QImage> editedImages;
    int currentIndex;
};
#endif
