#ifndef CALCULATOR_H
#define CALCULATOR_H

#include <QString>
#include <QWidget>

class QLabel;

class Calculator : public QWidget
{
    Q_OBJECT
public:
    explicit Calculator(QWidget *parent = nullptr);

private:
    QLabel *m_display;
    QString m_current;
    double m_accum;
    QString m_op;
    bool m_newEntry;

    void inputDigit(const QString &d);
    void inputOp(const QString &op);
    void equals();
    void clear();
    void updateDisplay();
};

#endif
