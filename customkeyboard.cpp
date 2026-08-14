#include "customkeyboard.h"
#include <QLineEdit>
#include <QApplication>
#include <QPainter>

CustomKeyboard::CustomKeyboard(QWidget *parent)
    : QWidget(parent)
    , m_targetEdit(nullptr)
    , m_isAlphaMode(true)
    , m_isUpperCase(false)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedHeight(220);
    setMinimumWidth(400);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(8, 8, 8, 8);
    m_mainLayout->setSpacing(4);

    // 背景容器
    QWidget *bgWidget = new QWidget(this);
    bgWidget->setStyleSheet("QWidget { background: rgba(30, 30, 40, 240); border-radius: 12px; }");
    QVBoxLayout *bgLayout = new QVBoxLayout(bgWidget);
    bgLayout->setContentsMargins(6, 6, 6, 6);
    bgLayout->setSpacing(4);

    // 键盘按键区域
    m_keysWidget = new QWidget(bgWidget);
    m_keysLayout = new QGridLayout(m_keysWidget);
    m_keysLayout->setContentsMargins(0, 0, 0, 0);
    m_keysLayout->setSpacing(3);

    bgLayout->addWidget(m_keysWidget);
    m_mainLayout->addWidget(bgWidget);

    setupEnglishLayout();
}

void CustomKeyboard::setTargetLineEdit(QLineEdit *edit)
{
    m_targetEdit = edit;
}

void CustomKeyboard::showKeyboard()
{
    show();
    raise();
}

void CustomKeyboard::hideKeyboard()
{
    hide();
    emit hidden();
}

void CustomKeyboard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    // 半透明背景
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(0, 0, 0, 100));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 12, 12);
}

QPushButton* CustomKeyboard::createKeyButton(const QString &text, const QString &style)
{
    QPushButton *btn = new QPushButton(text, m_keysWidget);
    btn->setMinimumSize(36, 36);
    btn->setFocusPolicy(Qt::NoFocus);

    QString defaultStyle;
    if (text == "删除" || text == "Enter" || text == "大写锁定" || text == "123" || text == "ABC") {
        // 功能键样式
        defaultStyle = "QPushButton { background: rgba(80, 80, 100, 200); border: none;"
            "border-radius: 6px; color: white; font-size: 14px; font-weight: bold; }"
            "QPushButton:pressed { background: rgba(120, 120, 150, 200); }";
    } else if (text == "空格") {
        defaultStyle = "QPushButton { background: rgba(60, 60, 80, 200); border: none;"
            "border-radius: 6px; color: white; font-size: 13px; }"
            "QPushButton:pressed { background: rgba(100, 100, 130, 200); }";
    } else {
        // 普通字母键
        defaultStyle = "QPushButton { background: rgba(50, 50, 65, 220); border: none;"
            "border-radius: 6px; color: white; font-size: 16px; font-weight: bold; }"
            "QPushButton:pressed { background: rgba(49, 194, 124, 180); }";
    }

    if (!style.isEmpty()) defaultStyle = style;
    btn->setStyleSheet(defaultStyle);

    return btn;
}

void CustomKeyboard::setupEnglishLayout()
{
    clearLayout(m_keysLayout);

    // 第一排数字行
    QStringList numRow = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
    for (int i = 0; i < numRow.size(); i++) {
        QPushButton *btn = createKeyButton(numRow[i]);
        btn->setObjectName("key_" + numRow[i]);
        connect(btn, &QPushButton::clicked, this, &CustomKeyboard::onKeyClicked);
        m_keysLayout->addWidget(btn, 0, i);
    }

    // 第二排 QWERTYUIOP
    QStringList row1 = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"};
    for (int i = 0; i < row1.size(); i++) {
        QString ch = m_isUpperCase ? row1[i] : row1[i].toLower();
        QPushButton *btn = createKeyButton(ch);
        btn->setObjectName("key_" + row1[i]);
        connect(btn, &QPushButton::clicked, this, &CustomKeyboard::onKeyClicked);
        m_keysLayout->addWidget(btn, 1, i);
    }

    // 第三排 ASDFGHJKL
    QStringList row2 = {"A", "S", "D", "F", "G", "H", "J", "K", "L"};
    for (int i = 0; i < row2.size(); i++) {
        QString ch = m_isUpperCase ? row2[i] : row2[i].toLower();
        QPushButton *btn = createKeyButton(ch);
        btn->setObjectName("key_" + row2[i]);
        connect(btn, &QPushButton::clicked, this, &CustomKeyboard::onKeyClicked);
        m_keysLayout->addWidget(btn, 2, i + 1);  // 居中偏移
    }

    // 第四排 ZXCVBNM + 退格
    QPushButton *shiftBtn = createKeyButton("大写锁定");
    shiftBtn->setObjectName("key_SHIFT");
    shiftBtn->setMinimumWidth(60);
    connect(shiftBtn, &QPushButton::clicked, this, &CustomKeyboard::onSwitchLang);
    m_keysLayout->addWidget(shiftBtn, 3, 0, 1, 2);

    QStringList row3 = {"Z", "X", "C", "V", "B", "N", "M"};
    for (int i = 0; i < row3.size(); i++) {
        QString ch = m_isUpperCase ? row3[i] : row3[i].toLower();
        QPushButton *btn = createKeyButton(ch);
        btn->setObjectName("key_" + row3[i]);
        connect(btn, &QPushButton::clicked, this, &CustomKeyboard::onKeyClicked);
        m_keysLayout->addWidget(btn, 3, i + 2);
    }

    QPushButton *backBtn = createKeyButton("删除");
    backBtn->setObjectName("key_BACKSPACE");
    backBtn->setMinimumWidth(50);
    connect(backBtn, &QPushButton::clicked, this, &CustomKeyboard::onBackspace);
    m_keysLayout->addWidget(backBtn, 3, 9, 1, 2);

    // 第五排: 123 + 空格 + Enter
    QPushButton *symBtn = createKeyButton("123");
    symBtn->setObjectName("key_SYMBOL");
    connect(symBtn, &QPushButton::clicked, this, &CustomKeyboard::onSwitchSymbol);
    m_keysLayout->addWidget(symBtn, 4, 0, 1, 2);

    QPushButton *spaceBtn = createKeyButton("空格");
    spaceBtn->setObjectName("key_SPACE");
    spaceBtn->setMinimumWidth(120);
    connect(spaceBtn, &QPushButton::clicked, this, &CustomKeyboard::onSpace);
    m_keysLayout->addWidget(spaceBtn, 4, 2, 1, 5);

    QPushButton *enterBtn = createKeyButton("Enter");
    enterBtn->setObjectName("key_ENTER");
    connect(enterBtn, &QPushButton::clicked, this, &CustomKeyboard::onEnter);
    m_keysLayout->addWidget(enterBtn, 4, 7, 1, 3);
}

void CustomKeyboard::setupSymbolLayout()
{
    clearLayout(m_keysLayout);

    // 符号布局
    QStringList symbols1 = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
    QStringList symbols2 = {"@", "#", "$", "%", "&", "-", "+", "(", ")", "!"};
    QStringList symbols3 = {"*", "\"", "'", ":", ";", "/", "?", "~", "`", "="};
    QStringList symbols4 = {".", ",", "…", "—", "【", "】", "{", "}", "<", ">"};

    for (int i = 0; i < symbols1.size(); i++) {
        QPushButton *btn = createKeyButton(symbols1[i]);
        btn->setObjectName("key_" + symbols1[i]);
        connect(btn, &QPushButton::clicked, this, &CustomKeyboard::onKeyClicked);
        m_keysLayout->addWidget(btn, 0, i);
    }

    for (int i = 0; i < symbols2.size(); i++) {
        QPushButton *btn = createKeyButton(symbols2[i]);
        btn->setObjectName("key_SYM_" + QString::number(i));
        connect(btn, &QPushButton::clicked, this, &CustomKeyboard::onKeyClicked);
        m_keysLayout->addWidget(btn, 1, i);
    }

    for (int i = 0; i < symbols3.size(); i++) {
        QPushButton *btn = createKeyButton(symbols3[i]);
        btn->setObjectName("key_SYM2_" + QString::number(i));
        connect(btn, &QPushButton::clicked, this, &CustomKeyboard::onKeyClicked);
        m_keysLayout->addWidget(btn, 2, i);
    }

    for (int i = 0; i < symbols4.size(); i++) {
        QPushButton *btn = createKeyButton(symbols4[i]);
        btn->setObjectName("key_SYM3_" + QString::number(i));
        connect(btn, &QPushButton::clicked, this, &CustomKeyboard::onKeyClicked);
        m_keysLayout->addWidget(btn, 3, i);
    }

    // 底部行: ABC + 空格 + 退格 + Enter
    QPushButton *abcBtn = createKeyButton("ABC");
    abcBtn->setObjectName("key_ABC");
    connect(abcBtn, &QPushButton::clicked, this, &CustomKeyboard::onSwitchSymbol);
    m_keysLayout->addWidget(abcBtn, 4, 0, 1, 2);

    QPushButton *backBtn = createKeyButton("删除");
    backBtn->setObjectName("key_BACKSPACE");
    backBtn->setMinimumWidth(50);
    connect(backBtn, &QPushButton::clicked, this, &CustomKeyboard::onBackspace);
    m_keysLayout->addWidget(backBtn, 4, 2, 1, 2);

    QPushButton *spaceBtn = createKeyButton("空格");
    spaceBtn->setObjectName("key_SPACE");
    spaceBtn->setMinimumWidth(120);
    connect(spaceBtn, &QPushButton::clicked, this, &CustomKeyboard::onSpace);
    m_keysLayout->addWidget(spaceBtn, 4, 4, 1, 3);

    QPushButton *enterBtn = createKeyButton("Enter");
    enterBtn->setObjectName("key_ENTER");
    connect(enterBtn, &QPushButton::clicked, this, &CustomKeyboard::onEnter);
    m_keysLayout->addWidget(enterBtn, 4, 7, 1, 3);
}

void CustomKeyboard::clearLayout(QLayout *layout)
{
    if (!layout) return;
    while (layout->count() > 0) {
        QLayoutItem *item = layout->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

void CustomKeyboard::onKeyClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn || !m_targetEdit) return;

    QString text = btn->text();
    m_targetEdit->insert(text);
    emit keyPressed(text);
}

void CustomKeyboard::onBackspace()
{
    if (!m_targetEdit) return;
    m_targetEdit->backspace();
    emit backspacePressed();
}

void CustomKeyboard::onEnter()
{
    hideKeyboard();
    emit enterPressed();
}

// #8修复: Shift 键只切换大小写，不切换语言
void CustomKeyboard::onSwitchLang()
{
    m_isUpperCase = !m_isUpperCase;
    setupEnglishLayout();
}

void CustomKeyboard::onSwitchSymbol()
{
    // #8修复: 123/ABC 按钮切换字母/符号布局，不影响大小写状态
    m_isAlphaMode = !m_isAlphaMode;
    if (m_isAlphaMode) {
        setupEnglishLayout();
    } else {
        setupSymbolLayout();
    }
}

void CustomKeyboard::onSpace()
{
    if (!m_targetEdit) return;
    m_targetEdit->insert(" ");
    emit keyPressed(" ");
}
