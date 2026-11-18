// ============================================================================
//  core.h —— 爱恩斯坦棋的"核心逻辑"接口
//
//  来源：从 einstein.cpp 的控制台实现里拆出来的算法部分（去掉界面，逻辑未改）。
//    · 常量与棋子编码（piece / sideOf / numOf）
//    · Position / Pos / Move / Record / PieceMap / Options
//    · namespace rules —— 规则：走法、吃子、胜负、布阵（纯函数，无副作用）
//    · class Ai        —— 期望极小极大搜索
//    · class Game      —— 一局棋的状态 + 存档读写
//
//  【这个头文件里没有一行 Qt 代码】——算法和界面彻底分开：
//  界面层换掉（控制台 / 窗口）都不用动这里，改算法也不会碰到界面代码。
// ============================================================================

#ifndef EINSTEIN_CORE_H
#define EINSTEIN_CORE_H

#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace einstein {

constexpr int kSize = 5;          // 棋盘边长
constexpr int kEmpty = 0;         // 空格
constexpr int kRed = 1;           // 红方
constexpr int kBlue = 2;          // 蓝方
constexpr int kNoSide = 0;        // 没有阵营：胜负未定
constexpr int kPhaseRoll = 0;     // 等待掷骰
constexpr int kPhaseMove = 1;     // 已掷骰，等待走棋
constexpr int kPhaseOver = 2;     // 对局结束

constexpr int kWinCorner = 1;     // 到达角点
constexpr int kWinEmpty = 2;      // 对方无子
constexpr int kWinSelf = 3;       // 自己无子（对方赢）

constexpr int kEasy = 1;
constexpr int kNormal = 2;
constexpr int kHard = 3;

constexpr int kSetupRandom = 1;
constexpr int kSetupFixed = 2;
constexpr int kSetupManual = 3;

constexpr int kMaxRecords = 600;

// 棋子编码：红 11~16，蓝 21~26
constexpr int piece(int side, int num) { return side * 10 + num; }
constexpr int sideOf(int v) { return v / 10; }
constexpr int numOf(int v) { return v % 10; }

using Cell = signed char;

// 棋盘局面：cell[y][x] 存棋子编码，turn 是行动方
struct Position {
    Cell cell[kSize][kSize] = {};   // 每格的棋子编码：0 空 / 11~16 红 / 21~26 蓝
    int turn = kRed;                // 该谁走：kRed 或 kBlue

    void clear() { std::memset(cell, 0, sizeof cell); }
};

// 棋盘坐标（0 下标），注意数组是 cell[y][x]
struct Pos {
    int x;   // 列号，0~4（最左是 0）
    int y;   // 行号，0~4（最上是 0）

    constexpr Pos() : x(0), y(0) {}
    constexpr Pos(int ax, int ay) : x(ax), y(ay) {}
};

inline Pos operator+(const Pos& a, const Pos& b) { return Pos(a.x + b.x, a.y + b.y); }
inline Pos operator-(const Pos& a, const Pos& b) { return Pos(a.x - b.x, a.y - b.y); }
inline bool operator==(const Pos& a, const Pos& b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(const Pos& a, const Pos& b) { return !(a == b); }

// 一步走法
struct Move {
    Pos from;   // 从哪一格
    Pos to;     // 走到哪一格
};

// 走棋记录（存档用，坐标是 1 下标）
struct Record {
    int side = 0;      // 谁走的：kRed / kBlue
    int num = 0;       // 走的是几号棋子（1~6）
    int dice = 0;      // 这一手掷出的点数
    Pos from;          // 起点（1 下标，和存档文字一致）
    Pos to;            // 终点（1 下标）
    int captured = 0;  // 被吃掉的棋子编码；0 表示走到空格
};

// 棋子位置表：一次扫描得到双方每枚棋子的位置与在场数量
struct PieceMap {
    bool present[kBlue + 1][7] = {};  // present[方][号]：这枚子还在不在场
    Pos at[kBlue + 1][7] = {};        // at[方][号]：它在哪一格
    int count[kBlue + 1] = {};        // count[方]：这一方还剩几枚
};

// 新局默认设置
struct Options {
    int level = kNormal;          // AI 难度：kEasy / kNormal / kHard
    int setupMode = kSetupRandom; // 布阵方式：随机 / 固定 / 手动
    int humanSide = kRed;         // 玩家执哪一方
    int firstSide = kRed;         // 谁先手
};

// 随机数发生器（xorshift）：同一个 seed 能复现同一盘棋
class Random {
public:
    void seed(unsigned long long s)
    {
        state_ = (s == 0) ? 88172645463325252ULL : s;
    }

    unsigned int next()
    {
        unsigned long long x = state_;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        state_ = x;
        return static_cast<unsigned int>(x >> 16);
    }

    int range(int lo, int hi)
    {
        return lo + static_cast<int>(next() % static_cast<unsigned int>(hi - lo + 1));
    }

    // 存档要保存随机数状态，所以留一个读写口
    unsigned long long state() const { return state_; }
    void setState(unsigned long long s) { state_ = (s == 0) ? 88172645463325252ULL : s; }

private:
    unsigned long long state_ = 88172645463325252ULL;
};

Random& rng();
double nowMs();

// ===== 规则：走法、选子、胜负、布阵，都是无副作用的纯函数 =====
namespace rules {

int other(int side);
const char* sideName(int side);
const char* sideChar(int side);

Pos goalCell(int side);
bool isGoalCell(int side, Pos p);

bool inBoard(Pos p);
bool inStartArea(int side, Pos p);
Pos startCell(int side, int i);
int startX(int side, int i);
int startY(int side, int i);
Pos stepPos(int side, Pos p, int k);

int distToGoal(int side, Pos p);
std::string pieceText(int v);
std::string saveCellText(int v);
int countPieces(const Position& p, int side);

void locateAll(const Position& p, PieceMap& pm);
int pickCandidates(const bool present[7], int dice, int out[2]);
int candidates(const Position& p, int side, int dice, int out[2]);
bool isCandidate(const Position& p, int side, int dice, int num);

int legalMoves(const PieceMap& pm, int side, int dice, std::array<Move, 6>& out);
int legalMoves(const Position& p, int side, int dice, std::array<Move, 6>& out);

bool isStepOk(int side, Pos from, Pos to);
int doMove(Position& p, int side, const Move& m);
int judge(const Position& p, int mover, int* reason);

void setupSide(Position& p, int side, int mode);
void setupBoard(Position& p, int humanSide, int mode);
bool checkSetup(const Position& p, int side, std::string& err);

int rollDice();

}  // namespace rules

// ===== AI：期望极小极大搜索 + 静态评估 =====
class Ai {
public:
    Ai(int depth = 5, double wProg = 10.0, double wCnt = -24.0, double wFlex = 6.0)
        : depth_(depth), wProg_(wProg), wCnt_(wCnt), wFlex_(wFlex)
    {
    }

    int searchDepth() const { return depth_; }
    unsigned long long nodeCount() const { return nodes_; }
    double elapsedMs() const { return lastMs_; }

    bool choose(const Position& p, int dice, int level, Move& best);

    double evalPosition(const Position& p, int side) const;

private:
    double sideValue(const PieceMap& pm, int side) const;
    double evalMap(const PieceMap& pm, int side) const;
    double search(const Position& p, int depth);

    static bool quickWin(const Position& p, const Move& m, int mover, int oppCount);

    int depth_;        // 搜索深度（困难难度用 5；普通难度固定 1）
    double wProg_;     // 权重：推进距离（越大越愿意往前走）
    double wCnt_;      // 权重：子力（吃子/保子的价值，负数表示少子吃亏）
    double wFlex_;     // 权重：走子灵活度（可选走法越少越差）
    unsigned long long nodes_ = 0;   // 上一次搜索访问了多少个节点（调试用）
    double lastMs_ = 0.0;            // 上一次搜索花了多少毫秒（调试用）
};

// ===== 一局棋的完整状态：局面 + 走棋记录 + 阵营/难度/阶段 + 存档读写 =====
class Game {
public:
    Position pos;                 // 当前局面：棋盘上每格是什么、该谁走
    int human = kRed;             // 玩家执哪一方
    int ai = kBlue;               // 电脑执哪一方（永远是对面）
    int level = kNormal;          // 难度：kEasy / kNormal / kHard
    int phase = kPhaseRoll;       // 阶段：等掷骰 / 等走棋 / 已结束
    int dice = 0;                 // 本回合掷出的点数，0 表示还没掷
    int winner = kNoSide;         // 赢家；kNoSide 表示还没分胜负
    int winReason = 0;            // 胜负原因：kWinCorner / kWinEmpty / kWinSelf
    bool active = false;          // 是否有一局正在进行
    std::vector<Record> records;  // 走棋记录，一步一条（存档也靠它）

    void addRecord(int side, int num, int diceValue, const Move& m, int captured)
    {
        if (static_cast<int>(records.size()) >= kMaxRecords) return;

        Record r;

        r.side = side;
        r.num = num;
        r.dice = diceValue;
        r.from.x = m.from.x + 1;
        r.from.y = m.from.y + 1;
        r.to.x = m.to.x + 1;
        r.to.y = m.to.y + 1;
        r.captured = captured;
        records.push_back(r);
    }

    bool save(const std::string& path, std::string& err) const;
    bool load(const std::string& path, std::string& err);
    bool validate(std::string& err) const;

private:
    std::string serialize() const;
};

}  // namespace einstein

#endif  // EINSTEIN_CORE_H
