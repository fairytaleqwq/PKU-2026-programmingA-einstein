// ============================================================================
//  boardwidget.h —— 棋盘控件
//
//  这个类只负责"把局面画出来"和"把鼠标点击变成坐标"，
//  它不知道规则、不知道谁该走——那是 MainWindow 的事。
//  这种"只管画"的控件，换一个项目也能直接复用。
// ============================================================================

#ifndef BOARDWIDGET_H
#define BOARDWIDGET_H

#include <QPoint>
#include <QVector>
#include <QWidget>

#include "core.h"

class BoardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BoardWidget(QWidget *parent = nullptr);

    QSize sizeHint() const override;

    // ---- 外面通过这几个函数告诉它"该画成什么样" ----
    void setPosition(const einstein::Position &p);             // 换成新的局面
    void setSelection(int x, int y);                           // 选中哪一格（-1,-1 = 没选中）
    void setLegalTargets(const QVector<QPoint> &targets);      // 可落点（绿色圆点）
    void setCandidates(const QVector<QPoint> &pieces);         // 本回合能动的棋子（蓝色光晕）
    void setLastMove(const einstein::Move &m, bool has);       // 上一手（黄色虚线框）
    void setManualSetup(bool on, int humanSide, int placed);   // 进入/退出"手动布阵"模式

signals:
    void cellClicked(int x, int y);   // 用户点了第 x 列、第 y 行（0 下标）

protected:
    void paintEvent(QPaintEvent *event) override;      // Qt 要求重画时被调用
    void mousePressEvent(QMouseEvent *event) override; // 鼠标按下时被调用

private:
    QRect cellRect(int x, int y) const;        // 逻辑坐标 (x,y) → 屏幕像素矩形
    QPoint cellAt(const QPoint &pixel) const;  // 屏幕像素 → 逻辑坐标（点偏了返回 -1,-1）

    einstein::Position pos_;         // 要画的局面（棋盘 + 行动方），由 setPosition 更新

    int selX_ = -1;                  // 选中棋子的列号，-1 = 没有选中
    int selY_ = -1;                  // 选中棋子的行号，-1 = 没有选中
    QVector<QPoint> targets_;        // 可落点列表（每项是一格的 x,y）
    QVector<QPoint> candidates_;     // 本回合能动的棋子位置（画蓝色光晕用）

    bool hasLast_ = false;           // last_ 是否有意义（开局时还没有上一手）
    einstein::Move last_;            // 上一手走法，用来给落点画黄框

    bool manual_ = false;            // 是否处于手动布阵阶段（只画自己的出发格）
    int manualSide_ = einstein::kRed;  // 手动布阵的是哪一方
    int manualPlaced_ = 0;           // 已经放了几枚（0~6），用于提示"下一枚是几号"
};

#endif  // BOARDWIDGET_H
