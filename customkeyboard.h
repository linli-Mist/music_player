#ifndef CUSTOMKEYBOARD_H
#define CUSTOMKEYBOARD_H

#include <QWidget>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVector>

class QLineEdit;  // 前向声明

class CustomKeyboard : public QWidget
{
    Q_OBJECT

public:
    explicit CustomKeyboard(QWidget *parent = nullptr);

    void setTargetLineEdit(QLineEdit *edit);
    void showKeyboard();
    void hideKeyboard();

signals:
    void keyPressed(const QString &text);
    void backspacePressed();
    void enterPressed();
    void hidden();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onKeyClicked();
    void onBackspace();
    void onEnter();
    void onSwitchLang();
    void onSwitchSymbol();
    void onSpace();

private:
    void setupEnglishLayout();
    void setupSymbolLayout();
    void clearLayout(QLayout *layout);
    QPushButton* createKeyButton(const QString &text, const QString &style = "");

    QLineEdit *m_targetEdit;
    QWidget *m_keysWidget;
    QVBoxLayout *m_mainLayout;
    QGridLayout *m_keysLayout;

    bool m_isAlphaMode;   // #16修复: true=字母/数字布局, false=符号布局（原 m_isEnglish 命名误导）
    bool m_isUpperCase;    // 大写状态

    // 当前布局行
    QVector<QVector<QString>> m_currentRows;
};

#endif // CUSTOMKEYBOARD_H
