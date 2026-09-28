#include "calculator.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

Calculator::Calculator(QWidget *parent)
    : QWidget(parent),
      m_display(new QLabel(QStringLiteral("0"), this)),
      m_current(QStringLiteral("0")),
      m_accum(0.0),
      m_newEntry(true)
{
    m_display->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_display->setObjectName(QStringLiteral("calcDisplay"));
    m_display->setMinimumHeight(60);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->addWidget(m_display);

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(8);
    const char *keys[4][4] = {
        { "7", "8", "9", "/" },
        { "4", "5", "6", "*" },
        { "1", "2", "3", "-" },
        { "0", "C", "=", "+" }
    };
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            const QString k = QString::fromLatin1(keys[r][c]);
            QPushButton *btn = new QPushButton(k, this);
            btn->setMinimumSize(60, 52);
            grid->addWidget(btn, r, c);
            if (k == QStringLiteral("C")) {
                connect(btn, &QPushButton::clicked, this, &Calculator::clear);
            } else if (k == QStringLiteral("=")) {
                connect(btn, &QPushButton::clicked, this, &Calculator::equals);
            } else if (k == QStringLiteral("+") || k == QStringLiteral("-") ||
                       k == QStringLiteral("*") || k == QStringLiteral("/")) {
                connect(btn, &QPushButton::clicked, this, [this, k]() { inputOp(k); });
            } else {
                connect(btn, &QPushButton::clicked, this, [this, k]() { inputDigit(k); });
            }
        }
    }
    layout->addLayout(grid);
}

void Calculator::inputDigit(const QString &d)
{
    if (m_newEntry) {
        m_current = d;
        m_newEntry = false;
    } else {
        if (m_current == QStringLiteral("0"))
            m_current = d;
        else
            m_current += d;
    }
    updateDisplay();
}

void Calculator::inputOp(const QString &op)
{
    if (!m_newEntry) {
        m_accum = m_current.toDouble();
        m_newEntry = true;
    }
    m_op = op;
}

void Calculator::equals()
{
    const double rhs = m_current.toDouble();
    double result = m_accum;
    if (m_op == QStringLiteral("+"))
        result += rhs;
    else if (m_op == QStringLiteral("-"))
        result -= rhs;
    else if (m_op == QStringLiteral("*"))
        result *= rhs;
    else if (m_op == QStringLiteral("/"))
        result = (rhs != 0) ? result / rhs : 0;
    m_current = QString::number(result, 'g', 10);
    m_accum = result;
    m_newEntry = true;
    updateDisplay();
}

void Calculator::clear()
{
    m_current = QStringLiteral("0");
    m_accum = 0.0;
    m_op.clear();
    m_newEntry = true;
    updateDisplay();
}

void Calculator::updateDisplay()
{
    m_display->setText(m_current);
}
