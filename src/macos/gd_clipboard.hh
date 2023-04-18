#ifdef __APPLE__
#pragma once

#include <QClipboard>
#include <QObject>
#include <QString>
#include <QTimer>

/**
A custom clipboard monitor that read keep reading clipboard in background.
QClipboard on macOS only triggers when the app is focused.
Code was inspired by
https://github.com/KDE/kdeconnect-kde/blob/v22.12.2/plugins/clipboard/clipboardlistener.cpp
*/

class gd_clipboard : public QObject {
Q_OBJECT
public:
    explicit gd_clipboard(QObject * parent = nullptr);

    [[nodiscard]] QString text() const;

    void stop();

    void start();

private:

    QClipboard * sysClipboard;
    QString curClipboardContent;

    QTimer m_monitoringTimer;
    QString m_currentContent;
    void updateClipboard();

signals:
    void changed(QClipboard::Mode mode);
};

#endif
