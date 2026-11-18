// ============================================================================
//  mainwindow.h —— 主窗口：把 core 的规则接到一堆 Qt 控件上
//
//  分工：
//    Game   —— 记着"当前局面、骰子、该谁走、胜负"
//    Ai     —— 算一步棋
//    本类    —— 管"什么时候能点、点了之后调谁、界面上显示什么"
//    Board  —— 只负责画
// ============================================================================

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <vector>

#include "core.h"

class BoardWidget;
class QLabel;
class QListWidget;
class QPushButton;
class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void newGame();                       // 弹出"新对局"设置对话框
    void saveGame();                      // 存到 saves/ 目录
    void loadGame();                      // 从 saves/ 目录读档
    void onCellClicked(int x, int y);     // 玩家点了棋盘上的第 x 列、第 y 行
    void showRules();                      // 帮助 → 规则说明
    void onDiceTick();                    // 骰子滚动动画的一帧

private:
    // 一步棋之前的完整现场：悔棋就是把这里的东西原样放回去
    struct Snapshot {
        einstein::Position pos;              // 当时的棋盘 + 该谁走
        int phase = 0;                       // 当时的阶段（等掷骰 / 等走棋）
        int dice = 0;                        // 当时的骰子点数
        int winner = 0;                      // 当时的胜负（悔棋要能撤销"已结束"）
        int winReason = 0;
        std::size_t recordCount = 0;         // 当时的走棋记录条数，悔棋时截断回这里
        unsigned long long rngState = 0;     // 当时的随机数状态，这样重走能遇到同样的骰子
        einstein::Move lastMove;             // 当时的"上一手"高亮
        bool hasLastMove = false;
    };

    void startNewGame(int humanSide, int level, int setupMode, int firstSide);
    void beginTurn();                     // 换手后：自动掷骰 / 让电脑走
    void startRollAnimation();            // 开始"摇骰子"动画
    void finishRoll();                    // 动画结束，落定真正的点数
    void aiChoose();                      // 电脑选一步棋并走掉
    void applyMove(const einstein::Move &m);          // 把一步棋落到局面和界面上
    void finishGame(int winner, int reason);          // 结算：弹框 + 更新界面
    void refreshUi();                     // 把 game_ 的状态刷到各个控件上
    void setStatus(const QString &text);  // 更新右侧提示框 + 底部状态栏
    void collectLegalMoves();             // 重新计算本回合所有合法走法
    void appendRecordItem(const einstein::Record &r);  // 往走棋记录列表追加一行
    void rebuildRecordList();             // 清空记录列表并按 game_.records 重填
    QString savesDir();                   // 存档目录（程序旁边的 saves/）

    void undoMove();                      // 悔棋：回退到自己上一次决策前
    void pushSnapshot();                  // 每走一步之前，把现场压栈
    void restoreSnapshot(const Snapshot &s);  // 把现场放回去

    bool isHumanTurn() const { return game_.pos.turn == game_.human; }
    bool pieceCanMove(int x, int y) const;

    // ---- 游戏状态（真正的数据都在 core 里，这里只是持有一份）----
    einstein::Game game_;                 // 当前这盘棋：局面、骰子、回合、记录、胜负
    einstein::Ai engine_;                 // AI 搜索引擎（深度/权重都在它内部）

    // ---- 界面控件（指针：由 Qt 的对象树负责销毁）----
    BoardWidget *board_ = nullptr;        // 左边那块自绘的棋盘
    QLabel *diceLabel_ = nullptr;         // 右侧的骰子数字框
    QLabel *turnLabel_ = nullptr;         // "轮到你 / 轮到电脑"
    QLabel *countLabel_ = nullptr;        // "红 6 子 · 蓝 6 子"
    QLabel *statusLabel_ = nullptr;       // 右侧米黄色提示框
    QListWidget *recordList_ = nullptr;   // 走棋记录列表
    QPushButton *undoBtn_ = nullptr;      // 「悔棋」按钮
    QTimer *diceTimer_ = nullptr;         // 摇骰子动画用的定时器（55ms 一帧）
    int rollTicks_ = 0;                   // 动画已经走了几帧，数到 5 就落定

    // ---- 手动布阵的临时状态 ----
    bool manualSetup_ = false;            // 是否正处于"手动布阵"阶段
    int manualPlaced_ = 0;                // 手动布阵已经放下了几枚子（0~6）
    int firstSide_ = einstein::kRed;      // 新局设置里选的先手（读档时也要恢复这个）

    // ---- 本回合的交互状态 ----
    int selectedX_ = -1;                  // 当前选中棋子的列号，-1 表示没选中
    int selectedY_ = -1;                  // 当前选中棋子的行号，-1 表示没选中
    std::vector<einstein::Move> legalMoves_;  // 本回合所有合法走法（骰子落定时算一次）
    einstein::Move lastMove_;             // 上一手走法（棋盘上画黄色虚线框）
    bool hasLastMove_ = false;            // lastMove_ 是否有效（开局时还没有）
    std::vector<Snapshot> history_;       // 悔棋栈：每走一步之前压一个现场
};

#endif  // MAINWINDOW_H
