#include "mainwindow.h"

#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <array>

#include "boardwidget.h"

namespace {

QString sideText(int side) { return QString::fromUtf8(einstein::rules::sideName(side)); }

// 棋子编码（11~16 红 / 21~26 蓝）→ "红方 3 号" 这种人话
QString pieceLabel(int code)
{
    return QStringLiteral("%1 %2 号").arg(sideText(einstein::sideOf(code))).arg(einstein::numOf(code));
}

QString reasonText(int reason)
{
    switch (reason) {
    case einstein::kWinCorner:
        return QStringLiteral("有棋子走到了终点角");
    case einstein::kWinEmpty:
        return QStringLiteral("对方的棋子被吃光了");
    case einstein::kWinSelf:
        return QStringLiteral("自己的棋子被吃光了");
    default:
        return QString();
    }
}

}  // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("爱恩斯坦棋 —— Qt 图形版"));
    resize(920, 670);

    // ================= 棋盘 =================
    board_ = new BoardWidget(this);
    connect(board_, &BoardWidget::cellClicked, this, &MainWindow::onCellClicked);

    // ================= 右侧面板 =================
    auto *panel = new QWidget(this);   // 右栏容器：把下面十几个控件打包成一块
    panel->setFixedWidth(310);

    auto *title = new QLabel(QStringLiteral("爱恩斯坦棋"), panel);   // 大标题「爱恩斯坦棋」
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #3b3226;");

    diceLabel_ = new QLabel(QStringLiteral("–"), panel);
    diceLabel_->setAlignment(Qt::AlignCenter);
    diceLabel_->setFixedSize(76, 76);
    diceLabel_->setStyleSheet("background: white; border: 2px solid #d8cfb8; border-radius: 12px;"
                              "font-size: 40px; font-weight: bold; color: #3b3226;");

    auto *diceCaption = new QLabel(QStringLiteral("骰子"), panel);   // 「骰子」两个小字
    diceCaption->setStyleSheet("color: #8a7f6a; font-size: 12px;");

    turnLabel_ = new QLabel(panel);
    turnLabel_->setStyleSheet("font-size: 14px; font-weight: bold; color: #3b3226;");

    countLabel_ = new QLabel(panel);
    countLabel_->setStyleSheet("font-size: 13px; color: #5a5040;");

    statusLabel_ = new QLabel(panel);
    statusLabel_->setWordWrap(true);
    statusLabel_->setMinimumHeight(80);
    statusLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    statusLabel_->setStyleSheet("background: #fffbef; border: 1px solid #e8dfc8; border-radius: 8px;"
                                "padding: 8px; font-size: 13px; color: #5a5040;");

    diceTimer_ = new QTimer(this);
    diceTimer_->setInterval(55);
    connect(diceTimer_, &QTimer::timeout, this, &MainWindow::onDiceTick);

    auto *newBtn = new QPushButton(QStringLiteral("新对局"), panel);   // 「新对局」按钮
    auto *saveBtn = new QPushButton(QStringLiteral("保存"), panel);   // 「保存」按钮
    auto *loadBtn = new QPushButton(QStringLiteral("读取"), panel);   // 「读取」按钮
    undoBtn_ = new QPushButton(QStringLiteral("悔棋"), panel);        // 「悔棋」按钮
    newBtn->setMinimumHeight(34);
    saveBtn->setMinimumHeight(34);
    loadBtn->setMinimumHeight(34);
    undoBtn_->setMinimumHeight(34);
    undoBtn_->setEnabled(false);                                     // 还没走过棋，不能悔
    connect(newBtn, &QPushButton::clicked, this, &MainWindow::newGame);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveGame);
    connect(loadBtn, &QPushButton::clicked, this, &MainWindow::loadGame);
    connect(undoBtn_, &QPushButton::clicked, this, &MainWindow::undoMove);

    auto *btnRow = new QHBoxLayout;   // 保存 / 读取 / 悔棋 并排的一行
    btnRow->addWidget(saveBtn);
    btnRow->addWidget(loadBtn);
    btnRow->addWidget(undoBtn_);

    auto *recordCaption = new QLabel(QStringLiteral("走棋记录"), panel);   // 「走棋记录」小标题
    recordCaption->setStyleSheet("color: #8a7f6a; font-size: 12px;");

    recordList_ = new QListWidget(panel);
    recordList_->setMaximumHeight(150);
    recordList_->setStyleSheet("font-size: 12px; border: 1px solid #e8dfc8; border-radius: 6px;");

    auto *diceRow = new QHBoxLayout;   // 骰子框 + 右边三行小字，横向排
    diceRow->addWidget(diceLabel_, 0, Qt::AlignTop);

    auto *infoCol = new QVBoxLayout;   // 骰子右边的三行：骰子 / 轮到你 / 子数
    infoCol->addWidget(diceCaption);
    infoCol->addWidget(turnLabel_);
    infoCol->addWidget(countLabel_);
    infoCol->addStretch();
    diceRow->addLayout(infoCol);

    auto *panelLay = new QVBoxLayout(panel);   // 右栏内部的竖向布局
    panelLay->addWidget(title);
    panelLay->addLayout(diceRow);
    panelLay->addSpacing(4);
    panelLay->addWidget(statusLabel_);
    panelLay->addWidget(newBtn);
    panelLay->addLayout(btnRow);
    panelLay->addSpacing(6);
    panelLay->addWidget(recordCaption);
    panelLay->addWidget(recordList_);
    panelLay->addStretch();

    // ================= 整体布局 =================
    auto *central = new QWidget(this);   // 中央区域：棋盘和右栏都放这里
    central->setStyleSheet("background: #f7f4ec;");

    auto *mainLay = new QHBoxLayout(central);   // 左右两栏
    mainLay->setContentsMargins(16, 16, 16, 16);
    mainLay->addWidget(board_, 0, Qt::AlignTop);
    mainLay->addSpacing(12);
    mainLay->addWidget(panel, 0, Qt::AlignTop);
    mainLay->addStretch();

    setCentralWidget(central);

    // ================= 菜单 =================
    auto *gameMenu = menuBar()->addMenu(QStringLiteral("游戏(&G)"));   // 「游戏」菜单

    QAction *actNew = gameMenu->addAction(QStringLiteral("新对局(&N)…"));
    actNew->setShortcut(QKeySequence::New);
    connect(actNew, &QAction::triggered, this, &MainWindow::newGame);

    gameMenu->addSeparator();

    QAction *actSave = gameMenu->addAction(QStringLiteral("保存(&S)…"));
    actSave->setShortcut(QKeySequence::Save);
    connect(actSave, &QAction::triggered, this, &MainWindow::saveGame);

    QAction *actLoad = gameMenu->addAction(QStringLiteral("读取(&O)…"));
    actLoad->setShortcut(QKeySequence::Open);
    connect(actLoad, &QAction::triggered, this, &MainWindow::loadGame);

    QAction *actUndo = gameMenu->addAction(QStringLiteral("悔棋(&Z)"));
    actUndo->setShortcut(QKeySequence::Undo);          // Ctrl+Z
    connect(actUndo, &QAction::triggered, this, &MainWindow::undoMove);

    gameMenu->addSeparator();

    QAction *actQuit = gameMenu->addAction(QStringLiteral("退出(&X)"));
    actQuit->setShortcut(QKeySequence::Quit);
    connect(actQuit, &QAction::triggered, this, &QWidget::close);

    QAction *actFolder = gameMenu->addAction(QStringLiteral("打开存档文件夹"));
    connect(actFolder, &QAction::triggered, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(savesDir()));
    });

    auto *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));   // 「帮助」菜单
    QAction *actRules = helpMenu->addAction(QStringLiteral("规则说明"));
    connect(actRules, &QAction::triggered, this, &MainWindow::showRules);

    // ================= 开局 =================
    // 启动时先把界面刷成"空盘"的样子，再弹"新对局"让玩家选设置。
    // （用 singleShot 推迟到事件循环里，保证主窗口先显示出来，对话框再盖上去）
    refreshUi();
    setStatus(QStringLiteral("选好阵营、难度、布阵方式和先手，然后点「开始」。"));
    QTimer::singleShot(0, this, &MainWindow::newGame);
}

// ---------------------------------------------------------------------------
//  新对局：弹设置框 → 布阵 → 开始
// ---------------------------------------------------------------------------
void MainWindow::newGame()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("新对局"));

    auto *sideBox = new QComboBox(&dlg);   // 阵营下拉框（我执红 / 我执蓝）
    sideBox->addItem(QStringLiteral("我执红（左上出发，往右下走）"), einstein::kRed);
    sideBox->addItem(QStringLiteral("我执蓝（右下出发，往左上走）"), einstein::kBlue);

    auto *levelBox = new QComboBox(&dlg);   // 难度下拉框（简单 / 普通 / 困难）
    levelBox->addItem(QStringLiteral("简单（随机挑一步）"), einstein::kEasy);
    levelBox->addItem(QStringLiteral("普通（算 1 层）"), einstein::kNormal);
    levelBox->addItem(QStringLiteral("困难（算 5 层）"), einstein::kHard);
    levelBox->setCurrentIndex(1);        // 默认"普通"，和以前的一致

    auto *setupBox = new QComboBox(&dlg);   // 布阵方式下拉框（随机 / 固定 / 手动）
    setupBox->addItem(QStringLiteral("随机布阵"), einstein::kSetupRandom);
    setupBox->addItem(QStringLiteral("固定布阵"), einstein::kSetupFixed);
    setupBox->addItem(QStringLiteral("手动布阵（自己摆 6 枚）"), einstein::kSetupManual);

    auto *firstBox = new QComboBox(&dlg);   // 先手下拉框（红先 / 蓝先）
    firstBox->addItem(QStringLiteral("红方先手"), einstein::kRed);
    firstBox->addItem(QStringLiteral("蓝方先手"), einstein::kBlue);

    auto *form = new QFormLayout;   // 四行「标签 + 下拉框」
    form->addRow(QStringLiteral("阵营"), sideBox);
    form->addRow(QStringLiteral("难度"), levelBox);
    form->addRow(QStringLiteral("布阵"), setupBox);
    form->addRow(QStringLiteral("先手"), firstBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);   // 「开始」「取消」按钮
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("开始"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto *lay = new QVBoxLayout(&dlg);   // 对话框的竖向布局
    lay->addLayout(form);
    lay->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) {
        // 第一次启动就把对话框取消了：按默认设置开一局，免得窗口空着没法玩
        if (!game_.active) {
            startNewGame(einstein::kRed, einstein::kNormal, einstein::kSetupRandom, einstein::kRed);
        }
        return;
    }

    startNewGame(sideBox->currentData().toInt(), levelBox->currentData().toInt(),
                 setupBox->currentData().toInt(), firstBox->currentData().toInt());
}

void MainWindow::startNewGame(int humanSide, int level, int setupMode, int firstSide)
{
    diceTimer_->stop();
    game_ = einstein::Game();
    firstSide_ = firstSide;
    game_.human = humanSide;
    game_.ai = einstein::rules::other(humanSide);
    game_.level = level;
    game_.active = true;
    game_.winner = einstein::kNoSide;
    game_.winReason = 0;
    game_.dice = 0;
    game_.phase = einstein::kPhaseRoll;
    game_.pos.turn = firstSide;
    game_.records.clear();

    selectedX_ = selectedY_ = -1;
    legalMoves_.clear();
    hasLastMove_ = false;
    recordList_->clear();
    history_.clear();                 // 新局：悔棋栈清空
    board_->setSelection(-1, -1);
    board_->setLegalTargets(QVector<QPoint>());
    board_->setCandidates(QVector<QPoint>());
    board_->setLastMove(lastMove_, false);

    if (setupMode == einstein::kSetupManual) {
        manualSetup_ = true;
        manualPlaced_ = 0;
        game_.pos.clear();
        game_.pos.turn = firstSide;
        board_->setManualSetup(true, humanSide, 0);
        refreshUi();
        setStatus(QStringLiteral("手动布阵：按顺序点你自己的 6 个出发格，会依次放入 1~6 号棋子。"));
        return;
    }

    manualSetup_ = false;
    board_->setManualSetup(false, humanSide, 0);
    einstein::rules::setupBoard(game_.pos, humanSide, setupMode);

    beginTurn();
}

// ---------------------------------------------------------------------------
//  回合流程
// ---------------------------------------------------------------------------
void MainWindow::beginTurn()
{
    if (game_.winner != einstein::kNoSide) return;

    diceTimer_->stop();          // 万一下一轮来得太快，先把上一轮的动画停掉
    game_.phase = einstein::kPhaseRoll;
    game_.dice = 0;
    selectedX_ = selectedY_ = -1;
    legalMoves_.clear();
    board_->setSelection(-1, -1);
    board_->setLegalTargets(QVector<QPoint>());
    board_->setCandidates(QVector<QPoint>());
    refreshUi();

    setStatus(isHumanTurn() ? QStringLiteral("轮到你了，掷骰子…")
                            : QStringLiteral("电脑正在掷骰子…"));

    // 不管是玩家还是电脑，都自动掷。先停一下制造节奏，再放出动画
    QTimer::singleShot(isHumanTurn() ? 180 : 420, this, &MainWindow::startRollAnimation);
}

// 摇骰子动画：让点数快速跳 5 下，再落定
void MainWindow::startRollAnimation()
{
    if (game_.winner != einstein::kNoSide || game_.phase != einstein::kPhaseRoll) return;

    rollTicks_ = 0;
    diceTimer_->start();
}

void MainWindow::onDiceTick()
{
    ++rollTicks_;

    if (rollTicks_ <= 5) {
        // 纯视觉效果：随机闪几个点数
        diceLabel_->setText(QString::number(QRandomGenerator::global()->bounded(1, 7)));
        return;
    }

    diceTimer_->stop();
    finishRoll();
}

void MainWindow::finishRoll()
{
    game_.dice = einstein::rules::rollDice();
    game_.phase = einstein::kPhaseMove;
    diceLabel_->setText(QString::number(game_.dice));

    selectedX_ = selectedY_ = -1;
    board_->setSelection(-1, -1);
    collectLegalMoves();

    QVector<QPoint> candidates;
    for (const auto &m : legalMoves_) candidates.push_back(QPoint(m.from.x, m.from.y));
    board_->setCandidates(candidates);
    refreshUi();

    if (!isHumanTurn()) {
        setStatus(QStringLiteral("电脑掷出 %1 点，正在思考…").arg(game_.dice));
        QTimer::singleShot(240, this, &MainWindow::aiChoose);
        return;
    }

    setStatus(QStringLiteral("你掷出 %1 点。发光的棋子能走，点它，再点绿格落子。").arg(game_.dice));
}

void MainWindow::aiChoose()
{
    if (game_.winner != einstein::kNoSide) return;

    einstein::Move best;   // AI 算出来的最佳走法（输出参数）

    if (!engine_.choose(game_.pos, game_.dice, game_.level, best)) {
        game_.pos.turn = einstein::rules::other(game_.pos.turn);
        beginTurn();
        return;
    }

    applyMove(best);
}

void MainWindow::applyMove(const einstein::Move &m)
{
    pushSnapshot();                    // 走之前先把现场存下来，悔棋才有东西可退

    const int side = game_.pos.turn;   // 这一步是谁走的（1 红 / 2 蓝）
    const int num = einstein::numOf(game_.pos.cell[m.from.y][m.from.x]);   // 走的是几号棋子
    const int captured = einstein::rules::doMove(game_.pos, side, m);   // 被吃掉的棋子编码（0 = 走到空格）

    game_.addRecord(side, num, game_.dice, m, captured);
    lastMove_ = m;
    hasLastMove_ = true;
    board_->setLastMove(m, true);

    selectedX_ = selectedY_ = -1;
    legalMoves_.clear();
    board_->setSelection(-1, -1);
    board_->setLegalTargets(QVector<QPoint>());
    board_->setCandidates(QVector<QPoint>());

    QString msg = QStringLiteral("%1 %2 号 (%3,%4) → (%5,%6)")   // 给玩家看的一句话：谁、几号、从哪到哪
                      .arg(sideText(side))
                      .arg(num)
                      .arg(m.from.x + 1)
                      .arg(m.from.y + 1)
                      .arg(m.to.x + 1)
                      .arg(m.to.y + 1);
    if (captured != einstein::kEmpty) msg += QStringLiteral("，吃掉 %1").arg(pieceLabel(captured));
    setStatus(msg);

    if (!game_.records.empty()) appendRecordItem(game_.records.back());

    int reason = 0;   // 胜负原因（到达角点 / 对方无子 / 自己无子）
    const int winner = einstein::rules::judge(game_.pos, side, &reason);   // 赢家；kNoSide 表示还没分出胜负

    if (winner != einstein::kNoSide) {
        finishGame(winner, reason);
        return;
    }

    beginTurn();
}

void MainWindow::finishGame(int winner, int reason)
{
    game_.winner = winner;
    game_.winReason = reason;
    game_.phase = einstein::kPhaseOver;
    game_.active = false;

    selectedX_ = selectedY_ = -1;
    legalMoves_.clear();
    board_->setSelection(-1, -1);
    board_->setLegalTargets(QVector<QPoint>());
    board_->setCandidates(QVector<QPoint>());
    refreshUi();

    const bool humanWon = (winner == game_.human);   // 是不是玩家赢了（决定弹框图标和措辞）

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("对局结束"));
    box.setIcon(humanWon ? QMessageBox::Information : QMessageBox::Warning);
    box.setText(QStringLiteral("%1获胜！").arg(sideText(winner)));
    box.setInformativeText(QStringLiteral("%1。\n本局共走了 %2 步。")
                               .arg(reasonText(reason))
                               .arg(game_.records.size()));
    box.exec();

    setStatus(QStringLiteral("对局结束：%1获胜（%2）。点「新对局」再来一盘。")
                  .arg(sideText(winner), reasonText(reason)));
}

// ---------------------------------------------------------------------------
//  悔棋：每走一步存一个"现场"，撤销时原样放回去
// ---------------------------------------------------------------------------

void MainWindow::pushSnapshot()
{
    Snapshot s;

    s.pos = game_.pos;                 // 棋盘 + 该谁走
    s.phase = game_.phase;             // 阶段
    s.dice = game_.dice;               // 骰子点数
    s.winner = game_.winner;           // 胜负（可能是"还没结束"）
    s.winReason = game_.winReason;
    s.recordCount = game_.records.size();   // 记录条数，悔棋时截断回这里
    s.rngState = einstein::rng().state();   // 随机数状态，重走会遇到同样的骰子
    s.lastMove = lastMove_;            // 上一手高亮
    s.hasLastMove = hasLastMove_;

    history_.push_back(s);
}

void MainWindow::restoreSnapshot(const Snapshot &s)
{
    game_.pos = s.pos;
    game_.phase = s.phase;
    game_.dice = s.dice;
    game_.winner = s.winner;
    game_.winReason = s.winReason;
    game_.active = true;
    game_.records.resize(s.recordCount);    // 走棋记录截断
    einstein::rng().setState(s.rngState);   // 随机数也退回去
    lastMove_ = s.lastMove;
    hasLastMove_ = s.hasLastMove;
}

void MainWindow::undoMove()
{
    if (history_.empty()) {
        setStatus(QStringLiteral("还没走过棋，不能悔棋。"));
        return;
    }

    diceTimer_->stop();     // 万一正在摇骰子，先停掉动画

    // 一次"悔棋"= 退回到自己上一次做决定之前：
    // 如果电脑已经应过手，就连它的那一步一起退，直到又轮到玩家。
    while (!history_.empty()) {
        const Snapshot s = history_.back();

        history_.pop_back();
        restoreSnapshot(s);
        if (game_.pos.turn == game_.human) break;
    }

    // 界面回到"该你选棋子"的状态
    selectedX_ = selectedY_ = -1;
    board_->setSelection(-1, -1);
    board_->setLegalTargets(QVector<QPoint>());
    board_->setLastMove(lastMove_, hasLastMove_);

    collectLegalMoves();

    QVector<QPoint> candidates;
    for (const auto &m : legalMoves_) candidates.push_back(QPoint(m.from.x, m.from.y));
    board_->setCandidates(candidates);

    rebuildRecordList();
    refreshUi();

    setStatus(QStringLiteral("已悔棋：回到第 %1 手之前，骰子仍是 %2 点。")
                  .arg(static_cast<int>(game_.records.size()) + 1)
                  .arg(game_.dice));
}

// ---------------------------------------------------------------------------
//  玩家点棋盘
// ---------------------------------------------------------------------------
void MainWindow::onCellClicked(int x, int y)
{
    // 手动布阵阶段
    if (manualSetup_) {
        const einstein::Pos p(x, y);   // 点到的那一格（逻辑坐标）

        if (!einstein::rules::inStartArea(game_.human, p)) {
            setStatus(QStringLiteral("请点你自己颜色的出发格（带虚线框和 + 的格子）。"));
            return;
        }
        if (game_.pos.cell[y][x] != einstein::kEmpty) {
            setStatus(QStringLiteral("这一格已经放过了，换一个空的。"));
            return;
        }

        game_.pos.cell[y][x] = static_cast<einstein::Cell>(einstein::piece(game_.human, manualPlaced_ + 1));
        ++manualPlaced_;

        board_->setManualSetup(true, game_.human, manualPlaced_);
        board_->setPosition(game_.pos);
        refreshUi();

        if (manualPlaced_ < 6) {
            setStatus(QStringLiteral("已放好 %1 枚，下一枚是 %2 号。").arg(manualPlaced_).arg(manualPlaced_ + 1));
            return;
        }

        // 六枚放完：电脑按固定顺序布阵，然后开打
        einstein::rules::setupSide(game_.pos, game_.ai, einstein::kSetupFixed);
        manualSetup_ = false;
        board_->setManualSetup(false, game_.human, 0);
        game_.pos.turn = firstSide_;
        setStatus(QStringLiteral("布阵完成。"));
        beginTurn();
        return;
    }

    if (game_.winner != einstein::kNoSide) return;
    if (!isHumanTurn() || game_.phase != einstein::kPhaseMove) return;

    const int v = game_.pos.cell[y][x];   // 这一格的棋子编码（0 = 空）

    // ① 已经选中了棋子，而且这一格是它的合法落点 → 直接落子。
    //    注意：爱恩斯坦棋允许吃掉自己的棋子（用来让路），所以这一条要在"选中"之前判断。
    if (selectedX_ >= 0) {
        if(x == selectedX_ && y == selectedY_){
            selectedX_ = -1;
            selectedY_ = -1;
            board_->setSelection(selectedX_,selectedY_);
            QVector<QPoint>targets;
            board_->setLegalTargets(targets);
            setStatus(QStringLiteral("你掷出 %1 点。发光的棋子能走，点它，再点绿格落子。").arg(game_.dice));
            return;
        }
        for (const auto &m : legalMoves_) {
            if (m.from.x == selectedX_ && m.from.y == selectedY_ && m.to.x == x && m.to.y == y) {
                applyMove(m);
                return;
            }
        }

    }

    // ② 点自己的棋子 → 选中
    if (v != einstein::kEmpty && einstein::sideOf(v) == game_.human) {
        if (!pieceCanMove(x, y)) {
            setStatus(QStringLiteral("这枚棋子本回合不能动（骰子点数决定能动哪几枚）。"));
            return;
        }

        selectedX_ = x;
        selectedY_ = y;

        QVector<QPoint> targets;
        for (const auto &m : legalMoves_)
            if (m.from.x == x && m.from.y == y) targets.push_back(QPoint(m.to.x, m.to.y));

        board_->setSelection(x, y);
        board_->setLegalTargets(targets);
        setStatus(QStringLiteral("已选中 %1 号棋子。点绿格落子——落点上有棋子就被吃掉（自己的也行）。")
                      .arg(einstein::numOf(v)));
        return;
    }

}

bool MainWindow::pieceCanMove(int x, int y) const
{
    for (const auto &m : legalMoves_)
        if (m.from.x == x && m.from.y == y) return true;
    return false;
}

void MainWindow::collectLegalMoves()
{
    legalMoves_.clear();

    std::array<einstein::Move, 6> arr;   // 临时数组：rules 要的是定长 std::array<Move,6>
    const int n = einstein::rules::legalMoves(game_.pos, game_.pos.turn, game_.dice, arr);   // 实际有多少条合法走法

    for (int i = 0; i < n; ++i) legalMoves_.push_back(arr[static_cast<std::size_t>(i)]);
}

// ---------------------------------------------------------------------------
//  界面刷新
// ---------------------------------------------------------------------------
void MainWindow::refreshUi()
{
    diceLabel_->setText(game_.dice > 0 ? QString::number(game_.dice) : QStringLiteral("–"));

    const int red = einstein::rules::countPieces(game_.pos, einstein::kRed);   // 红方还剩几枚子
    const int blue = einstein::rules::countPieces(game_.pos, einstein::kBlue);   // 蓝方还剩几枚子
    countLabel_->setText(QStringLiteral("红 %1 子 · 蓝 %2 子").arg(red).arg(blue));

    if (game_.winner != einstein::kNoSide)
        turnLabel_->setText(QStringLiteral("对局结束"));
    else if (!game_.active)
        turnLabel_->setText(QStringLiteral("等待开局"));
    else if (manualSetup_)
        turnLabel_->setText(QStringLiteral("布阵中"));
    else
        turnLabel_->setText(isHumanTurn() ? QStringLiteral("轮到你") : QStringLiteral("轮到电脑"));

    // 悔棋按钮：走过棋、且现在轮到你（或对局已结束）才能点
    undoBtn_->setEnabled(!manualSetup_ && !history_.empty() &&
                         (game_.winner != einstein::kNoSide || isHumanTurn()));

    board_->setPosition(game_.pos);
}

void MainWindow::setStatus(const QString &text)
{
    statusLabel_->setText(text);
    statusBar()->showMessage(text, 4000);
}

void MainWindow::appendRecordItem(const einstein::Record &r)
{
    QString text = QStringLiteral("%1. %2 %3号 (%4,%5)→(%6,%7)")   // 这一行记录的文字
                       .arg(recordList_->count() + 1)
                       .arg(sideText(r.side))
                       .arg(r.num)
                       .arg(r.from.x)
                       .arg(r.from.y)
                       .arg(r.to.x)
                       .arg(r.to.y);

    if (r.captured != 0) text += QStringLiteral("  吃 %1").arg(pieceLabel(r.captured));

    recordList_->addItem(text);
    recordList_->scrollToBottom();
}

// 悔棋之后整表重刷：清空列表，按 game_.records 重新填一遍
void MainWindow::rebuildRecordList()
{
    recordList_->clear();

    for (const auto &r : game_.records) appendRecordItem(r);
}

// ---------------------------------------------------------------------------
//  存档 / 读档 / 规则
// ---------------------------------------------------------------------------

// 存档统一放在程序目录下的 saves/ 里；建不出来（比如程序装在只读目录）就退到用户数据目录
QString MainWindow::savesDir()
{
    QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/saves");   // 优先放在程序目录下的 saves/

    if (!QDir().mkpath(path)) {
        path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/saves");
        QDir().mkpath(path);
    }
    return path;
}

void MainWindow::saveGame()
{
    const QString suggested = QDir(savesDir()).filePath(   // 默认文件名：game_年月日_时分秒.sav
        QStringLiteral("game_%1.sav").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"))));

    const QString path = QFileDialog::getSaveFileName(   // 用户在对话框里最终选的保存路径
        this, QStringLiteral("保存对局"), suggested,
        QStringLiteral("存档 (*.sav *.txt);;所有文件 (*)"));

    if (path.isEmpty()) return;

    std::string err;   // core 失败时给出的中文原因

    // core 里用的是 std::ofstream，Windows 上它按本地编码（GBK）打开文件，
    // 所以这里要 toLocal8Bit 而不是 toUtf8，否则中文目录下会存不进去。
    if (game_.save(path.toLocal8Bit().toStdString(), err)) {
        setStatus(QStringLiteral("已保存到 %1").arg(path));
    } else {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QString::fromUtf8(err.c_str()));
    }
}

void MainWindow::loadGame()
{
    const QString path = QFileDialog::getOpenFileName(   // 用户选中的存档路径
        this, QStringLiteral("读取对局"), savesDir(),
        QStringLiteral("存档 (*.sav *.txt);;所有文件 (*)"));

    if (path.isEmpty()) return;

    einstein::Game loaded;   // 先读进临时对象，全部校验通过才替换当前对局
    std::string err;   // core 失败时给出的中文原因

    if (!loaded.load(path.toLocal8Bit().toStdString(), err)) {
        QMessageBox::warning(this, QStringLiteral("读取失败"), QString::fromUtf8(err.c_str()));
        return;
    }

    game_ = loaded;

    // 随机数状态由 core 的 Game::load() 从存档的 RNG 字段恢复，
    // 所以这里不用再做别的——读档后摇出的骰子和存档那一刻完全接得上。

    manualSetup_ = false;
    manualPlaced_ = 0;
    board_->setManualSetup(false, game_.human, 0);

    selectedX_ = selectedY_ = -1;
    legalMoves_.clear();
    hasLastMove_ = false;
    board_->setSelection(-1, -1);
    board_->setLegalTargets(QVector<QPoint>());
    board_->setCandidates(QVector<QPoint>());
    board_->setLastMove(lastMove_, false);

    recordList_->clear();
    for (const auto &r : game_.records) appendRecordItem(r);
    history_.clear();                 // 读档：悔棋栈也清空（撤不回存档之前）

    refreshUi();

    if (game_.winner != einstein::kNoSide) {
        setStatus(QStringLiteral("这局已经结束：%1获胜。").arg(sideText(game_.winner)));
        return;
    }

    if (game_.phase == einstein::kPhaseMove) {
        if (isHumanTurn()) {
            collectLegalMoves();
            QVector<QPoint> candidates;
            for (const auto &m : legalMoves_) candidates.push_back(QPoint(m.from.x, m.from.y));
            board_->setCandidates(candidates);
            setStatus(QStringLiteral("读档成功。你掷出 %1 点，该你走了。").arg(game_.dice));
        } else {
            setStatus(QStringLiteral("读档成功，轮到电脑。"));
            QTimer::singleShot(500, this, &MainWindow::aiChoose);
        }
    } else {
        setStatus(QStringLiteral("读档成功，掷骰子…"));
        QTimer::singleShot(300, this, &MainWindow::startRollAnimation);
    }
}

void MainWindow::showRules()
{
    QMessageBox::information(
        this, QStringLiteral("规则说明"),
        QStringLiteral(
            "· 5×5 棋盘，红方从左上出发，目标是右下角；蓝方从右下出发，目标是左上角。\n"
            "· 每回合先掷骰子，只有对应编号的棋子能走；如果那枚棋子已被吃，则取最接近的编号。\n"
            "· 棋子只能向右、向下、向右下（蓝方相反方向）走一格。\n"
            "· 目标格上有对方棋子时可以吃掉它；吃光对方棋子也算赢。\n"
            "· 谁的棋子先到达对角终点，谁就赢。"));
}
