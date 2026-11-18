#include "boardwidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {
const int kCell = 96;     // 每格边长（像素）
const int kMargin = 34;   // 棋盘外留白：左边、上边要留出画坐标号的宽度
}

BoardWidget::BoardWidget(QWidget *parent) : QWidget(parent)
{
    setFixedSize(kCell * einstein::kSize + kMargin * 2, kCell * einstein::kSize + kMargin * 2);
    setCursor(Qt::PointingHandCursor);

    // 给屏幕阅读器（以及自动化测试）一个名字，方便定位这块区域
    setAccessibleName(QStringLiteral("棋盘"));
}

QSize BoardWidget::sizeHint() const
{
    return QSize(kCell * einstein::kSize + kMargin * 2, kCell * einstein::kSize + kMargin * 2);
}

QRect BoardWidget::cellRect(int x, int y) const
{
    return QRect(kMargin + x * kCell, kMargin + y * kCell, kCell, kCell);
}

QPoint BoardWidget::cellAt(const QPoint &pixel) const
{
    const int x = (pixel.x() - kMargin) / kCell;   // 列号：减掉左边距再除以格宽
    const int y = (pixel.y() - kMargin) / kCell;   // 行号：减掉上边距再除以格高

    if (x < 0 || x >= einstein::kSize || y < 0 || y >= einstein::kSize) return QPoint(-1, -1);
    return QPoint(x, y);
}

void BoardWidget::setPosition(const einstein::Position &p)
{
    pos_ = p;
    update();
}

void BoardWidget::setSelection(int x, int y)
{
    selX_ = x;
    selY_ = y;
    update();
}

void BoardWidget::setLegalTargets(const QVector<QPoint> &targets)
{
    targets_ = targets;
    update();
}

void BoardWidget::setCandidates(const QVector<QPoint> &pieces)
{
    candidates_ = pieces;
    update();
}

void BoardWidget::setLastMove(const einstein::Move &m, bool has)
{
    last_ = m;
    hasLast_ = has;
    update();
}

void BoardWidget::setManualSetup(bool on, int humanSide, int placed)
{
    manual_ = on;
    manualSide_ = humanSide;
    manualPlaced_ = placed;
    update();
}

void BoardWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor("#f7f4ec"));

    QFont numFont = font();       // 棋子上的数字：粗体 34px
    numFont.setBold(true);
    numFont.setPixelSize(34);

    QFont markFont = font();      // 终点角上的 ★：16px
    markFont.setPixelSize(16);

    // ---- 坐标号：上方列号 1~5、左侧行号 1~5（和走棋记录里的 (x,y) 对得上）----
    QFont coordFont = font();     // 棋盘边上的行列坐标号：15px
    coordFont.setPixelSize(15);
    p.setFont(coordFont);
    p.setPen(QColor("#8a7f6a"));

    for (int i = 0; i < einstein::kSize; ++i) {
        // 列号：这一列的正上方
        p.drawText(QRect(kMargin + i * kCell, 8, kCell, kMargin - 12),
                   Qt::AlignCenter, QString::number(i + 1));

        // 行号：这一行的左侧
        p.drawText(QRect(6, kMargin + i * kCell, kMargin - 12, kCell),
                   Qt::AlignCenter, QString::number(i + 1));
    }

    for (int y = 0; y < einstein::kSize; ++y) {
        for (int x = 0; x < einstein::kSize; ++x) {
            const QRect r = cellRect(x, y);   // 这一格的像素矩形
            const bool redStart = einstein::rules::inStartArea(einstein::kRed, einstein::Pos(x, y));   // 这格是不是红方的出发区
            const bool blueStart = einstein::rules::inStartArea(einstein::kBlue, einstein::Pos(x, y));   // 这格是不是蓝方的出发区

            // ---- 格子底色 ----
            QColor bg = (redStart || blueStart) ? QColor("#efe6d2") : QColor("#fdfbf6");   // 格子底色（后面还会按状态改）

            if (!manual_ && targets_.contains(QPoint(x, y))) bg = QColor("#dff0d8");
            if (!manual_ && x == selX_ && y == selY_) bg = QColor("#fdf3d8");
            if (manual_ && !redStart && !blueStart) bg = QColor("#f2efe6");

            p.fillRect(r, bg);
            p.setPen(QPen(QColor("#d8cfb8"), 1));
            p.drawRect(r);

            // ---- 两只"终点角"打个星号，方便找目标 ----
            const einstein::Pos redGoal = einstein::rules::goalCell(einstein::kRed);   // 红方终点格：右下角
            const einstein::Pos blueGoal = einstein::rules::goalCell(einstein::kBlue);   // 蓝方终点格：左上角

            if (x == redGoal.x && y == redGoal.y) {
                p.setFont(markFont);
                p.setPen(QColor("#d9534f"));
                p.drawText(r.adjusted(6, 2, -6, -2), Qt::AlignTop | Qt::AlignRight, QStringLiteral("★"));
            } else if (x == blueGoal.x && y == blueGoal.y) {
                p.setFont(markFont);
                p.setPen(QColor("#3b6fb6"));
                p.drawText(r.adjusted(6, 2, -6, -2), Qt::AlignTop | Qt::AlignRight, QStringLiteral("★"));
            }
        }
    }

    // ---- 手动布阵：在出发格上画"待放置"的提示 ----
    if (manual_) {
        QFont hint = font();   // 布阵提示的「+」用的字体
        hint.setPixelSize(30);
        hint.setBold(true);

        for (int i = 0; i < 6; ++i) {
            const einstein::Pos cell = einstein::rules::startCell(manualSide_, i);   // 玩家这一方的第 i 个出发格

            if (pos_.cell[cell.y][cell.x] != einstein::kEmpty) continue;

            const QRect r = cellRect(cell.x, cell.y).adjusted(8, 8, -8, -8);   // 虚线框矩形（向内缩 8px）
            QPen pen(QColor("#c9bfa6"), 2, Qt::DashLine);   // 灰色虚线画笔
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r, 8, 8);

            p.setFont(hint);
            p.setPen(QColor("#cfc6b0"));
            p.drawText(r, Qt::AlignCenter, QStringLiteral("+"));
        }
    }

    // ---- 棋子 ----
    for (int y = 0; y < einstein::kSize; ++y) {
        for (int x = 0; x < einstein::kSize; ++x) {
            const int v = pos_.cell[y][x];   // 这一格的棋子编码（0 = 空）

            if (v == einstein::kEmpty) continue;

            const QRect r = cellRect(x, y);   // 这一格的像素矩形
            const QPointF c = r.center();   // 格子中心点（画圆的圆心）
            const bool isRed = (einstein::sideOf(v) == einstein::kRed);   // 是红方还是蓝方，决定颜色

            // 本回合能动的棋子：底下垫一圈蓝光
            if (candidates_.contains(QPoint(x, y))) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(59, 111, 182, 60));
                p.drawEllipse(c, 40, 40);
            }

            p.setPen(Qt::NoPen);
            p.setBrush(isRed ? QColor("#d9534f") : QColor("#3b6fb6"));
            p.drawEllipse(c, 33, 33);

            // 上一手：黄框标出来
            if (hasLast_ && last_.to.x == x && last_.to.y == y) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QColor("#f0b429"), 3, Qt::DotLine));
                p.drawEllipse(c, 40, 40);
            }

            // 选中的棋子：金色实线框
            if (x == selX_ && y == selY_) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QColor("#f0b429"), 4));
                p.drawEllipse(c, 39, 39);
            }

            p.setFont(numFont);
            p.setPen(Qt::white);
            p.drawText(r, Qt::AlignCenter, QString::number(einstein::numOf(v)));
        }
    }

    // ---- 可落点：绿点 ----
    for (const QPoint &t : targets_) {
        const QRect r = cellRect(t.x(), t.y());   // 可落点所在格的矩形
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(76, 175, 80, 170));
        p.drawEllipse(r.center(), 13, 13);
    }
}

void BoardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPoint cell = cellAt(event->pos());   // 把鼠标位置换算成"第几列第几行"

    if (cell.x() >= 0) emit cellClicked(cell.x(), cell.y());
}
