// ============================================================================
//  core.cpp —— 从控制台版 einstein.cpp 里原样抽出的"规则 + AI + 对局状态"
//
//  这个文件里【没有任何 Qt 代码】，它就是一个普通的 C++ 程序：
//    · namespace rules  —— 走法生成、吃子、胜负判定、布阵
//    · class Ai          —— 期望极小极大搜索
//    · class Game        —— 一局棋的状态 + 存档读写
//
//  界面层（boardwidget / mainwindow）只调用这里的东西，不关心算法细节；
//  反过来说，这些算法也不需要知道"界面是控制台还是窗口"。
// ============================================================================

#include "core.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace einstein {

Random& rng()
{
    static Random instance;
    return instance;
}

// 当前时间（毫秒），用于计时与动画节奏
double nowMs()
{
    using Clock = std::chrono::steady_clock;
    static const Clock::time_point start = Clock::now();

    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// 把 32 位整数格式化成 8 位十六进制，存档校验和用
std::string hex8(unsigned int v)
{
    char buf[16];

    std::snprintf(buf, sizeof buf, "%08X", v);
    return std::string(buf);
}


// ===== 规则：走法、选子、胜负、布阵，都是无副作用的纯函数 =====
namespace rules {

// 取对方阵营：阵营是 1/2，而 1+2=3，所以“3 减自己”就是对方
int other(int side) { return 3 - side; }

// 阵营的中文名
const char* sideName(int side) { return side == kRed ? "红方" : "蓝方"; }
// 阵营的单字母 “R”/“B”，棋盘显示与存档用
const char* sideChar(int side) { return side == kRed ? "R" : "B"; }

// 双方的目标角点：红方在右下 (5,5)，蓝方在左上 (1,1)。用阵营当小标，第 0 项空着
const Pos kGoalCell[kBlue + 1] = {
    Pos(-1, -1), Pos(kSize - 1, kSize - 1), Pos(0, 0)
};

Pos goalCell(int side) { return kGoalCell[side]; }

bool isGoalCell(int side, Pos p) { return p == goalCell(side); }
// 这个位置是否还在棋盘内
bool inBoard(Pos p) { return p.x >= 0 && p.x < kSize && p.y >= 0 && p.y < kSize; }

// 这个位置是否属于该方的出发区（左上 / 右下的 6 格三角形）
bool inStartArea(int side, Pos p)
{
    if (side == kRed) return p.x + p.y <= 2;
    return (kSize - 1 - p.x) + (kSize - 1 - p.y) <= 2;
}

// 红方 6 个出发格的坐标（顺序固定，蓝方按棋盘中心镜像）
const Pos kStartCells[6] = {
    Pos(0, 0), Pos(1, 0), Pos(2, 0), Pos(0, 1), Pos(1, 1), Pos(0, 2)
};

// 第 i 个出发格的坐标（i = 0~5，红蓝镜像）
Pos startCell(int side, int i)
{
    const Pos p = kStartCells[i];

    return (side == kRed) ? p : Pos(kSize - 1 - p.x, kSize - 1 - p.y);
}

// 为了可读性保留的取列 / 取行小工具
int startX(int side, int i) { return startCell(side, i).x; }
int startY(int side, int i) { return startCell(side, i).y; }

// 红方的三个走子方向（位移矢量）：右、下、右下；蓝方取相反数
const Pos kMoveDir[3] = {Pos(1, 0), Pos(0, 1), Pos(1, 1)};

// 沿第 k 个方向走一格，返回新位置（不检查越界，调用方自己用 inBoard 判断）
Pos stepPos(int side, Pos p, int k)
{
    // 红方往右下走，蓝方方向相反——正好是矢量相加 / 相减
    return (side == kRed) ? p + kMoveDir[k] : p - kMoveDir[k];
}

// 到目标角点的最少步数：红方只能右/下/右下，所以取行列差的最大值
int distToGoal(int side, Pos p)
{
    const Pos goal = goalCell(side);

    int dx = std::abs(goal.x - p.x);
    int dy = std::abs(goal.y - p.y);

    return std::max(dx, dy);
}

// 棋子编码 → “R1” / “B3”，空格返回 “.”
std::string pieceText(int v)
{
    if (v == kEmpty) return ".";

    std::string s;

    s += (sideOf(v) == kRed) ? 'R' : 'B';
    s += static_cast<char>('0' + numOf(v));
    return s;
}

// 存档用的固定两字符文本：“R1” / “B3” / “..”
std::string saveCellText(int v) { return (v == kEmpty) ? std::string("..") : pieceText(v); }

// 数一数某方还有几枚棋子在场上（判胜负、界面显示都要用）
int countPieces(const Position& p, int side)
{
    int x, y, n = 0;

    for (y = 0; y < kSize; ++y)
        for (x = 0; x < kSize; ++x)
            if (p.cell[y][x] != kEmpty && sideOf(p.cell[y][x]) == side) ++n;
    return n;
}

// 扫描一次棋盘，建立双方「编号 → 位置」表，之后查询都是 O(1)
void locateAll(const Position& p, PieceMap& pm)
{
    std::memset(&pm, 0, sizeof pm);
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const int v = p.cell[y][x];
            if (v == kEmpty) continue;

            const int s = sideOf(v);
            const int num = numOf(v);
            if (s < kRed || s > kBlue || num < 1 || num > 6) continue;

            pm.present[s][num] = true;
            pm.at[s][num].x = x;
            pm.at[s][num].y = y;
            ++pm.count[s];
        }
    }
}

// 按骰子点数选出本回合能动的编号：同号优先，否则取最接近的两侧编号
int pickCandidates(const bool present[7], int dice, int out[2])
{
    int n = 0;

    if (dice < 1 || dice > 6) return 0;
    if (present[dice]) {
        out[0] = dice;
        return 1;
    }
    for (int d = dice - 1; d >= 1; --d) {
        if (present[d]) {
            out[n++] = d;
            break;
        }
    }
    for (int d = dice + 1; d <= 6; ++d) {
        if (present[d]) {
            out[n++] = d;
            break;
        }
    }
    return n;
}

// candidates 的棋盘版本：先扫描棋盘，再调用 pickCandidates
int candidates(const Position& p, int side, int dice, int out[2])
{
    bool present[7] = {false, false, false, false, false, false, false};

    if (dice < 1 || dice > 6) return 0;

    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            const int v = p.cell[y][x];
            if (v != kEmpty && sideOf(v) == side) present[numOf(v)] = true;
        }
    return pickCandidates(present, dice, out);
}

// 某个编号的棋子本回合能不能动
bool isCandidate(const Position& p, int side, int dice, int num)
{
    int cand[2];

    const int n = candidates(p, side, dice, cand);
    for (int i = 0; i < n; ++i)
        if (cand[i] == num) return true;
    return false;
}

// 用现成的位置表生成全部合法走法，返回条数
int legalMoves(const PieceMap& pm, int side, int dice, std::array<Move, 6>& out)
{
    int cand[2];
    const int nc = pickCandidates(pm.present[side], dice, cand);
    int n = 0;

    for (int i = 0; i < nc && n < 6; ++i) {
        const int num = cand[i];
        const Pos from = pm.at[side][num];

        for (int k = 0; k < 3 && n < 6; ++k) {
            const Pos to = stepPos(side, from, k);

            if (!inBoard(to)) continue;

            out[n].from = from;
            out[n].to = to;
            ++n;
        }
    }
    return n;
}

// 按局面生成全部合法走法（内部先建位置表）
int legalMoves(const Position& p, int side, int dice, std::array<Move, 6>& out)
{
    PieceMap pm;

    locateAll(p, pm);
    return legalMoves(pm, side, dice, out);
}

// 这一步是不是合法的“走一格”：方向正确、且没走出棋盘
bool isStepOk(int side, Pos from, Pos to)
{
    for (int k = 0; k < 3; ++k)
        if (stepPos(side, from, k) == to) return true;
    return false;
}

// 执行走法，返回被吃掉的棋子（0 表示走到空格）
int doMove(Position& p, int side, const Move& m)
{
    const int cap = p.cell[m.to.y][m.to.x];
    const int num = numOf(p.cell[m.from.y][m.from.x]);

    p.cell[m.from.y][m.from.x] = kEmpty;
    p.cell[m.to.y][m.to.x] = static_cast<Cell>(piece(side, num));
    p.turn = other(side);
    return cap;
}

// 判定胜负：到达角点 / 对方无子 / 自己吃光；未分胜负返回 kNoSide
int judge(const Position& p, int mover, int* reason)
{
    const Pos goal = goalCell(mover);

    if (p.cell[goal.y][goal.x] != kEmpty && sideOf(p.cell[goal.y][goal.x]) == mover) {
        if (reason) *reason = kWinCorner;
        return mover;
    }
    if (countPieces(p, other(mover)) == 0) {
        if (reason) *reason = kWinEmpty;
        return mover;
    }
    if (countPieces(p, mover) == 0) {
        if (reason) *reason = kWinSelf;
        return other(mover);
    }
    return kNoSide;
}

// 布阵：按固定顺序或随机顺序，把 1~6 号放进出发区
void setupSide(Position& p, int side, int mode)
{
    std::array<int, 6> nums = {{1, 2, 3, 4, 5, 6}};

    if (mode == kSetupRandom) {
        for (int i = 5; i > 0; --i) {
            const int j = rng().range(0, i);

            std::swap(nums[i], nums[j]);
        }
    }
    for (int i = 0; i < 6; ++i)
        p.cell[startY(side, i)][startX(side, i)] = static_cast<Cell>(piece(side, nums[i]));
}

// 整盘布阵：清空棋盘，人员方按所选方式布阵，计算机方固定布阵
void setupBoard(Position& p, int humanSide, int mode)
{
    p.clear();
    setupSide(p, humanSide, (mode == kSetupRandom) ? kSetupRandom : kSetupFixed);
    setupSide(p, other(humanSide), kSetupFixed);
}

// 检查布阵是否合法：出发区 6 格恰好是 1~6 号各一枚
bool checkSetup(const Position& p, int side, std::string& err)
{
    bool seen[7] = {false, false, false, false, false, false, false};

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const int v = p.cell[y][x];
            if (v == kEmpty) continue;
            if (sideOf(v) == side && !inStartArea(side, Pos{x, y})) {
                err = std::string(sideName(side)) + "的 " + std::to_string(numOf(v)) + " 号棋子不在出发区内";
                return false;
            }
        }
    }
    for (int i = 0; i < 6; ++i) {
        const int cx = startX(side, i);
        const int cy = startY(side, i);
        const int v = p.cell[cy][cx];

        if (v == kEmpty) {
            err = std::string(sideName(side)) + "出发格 (" + std::to_string(cx + 1) + "," +
                std::to_string(cy + 1) + ") 是空的";
            return false;
        }
        if (sideOf(v) != side) {
            err = std::string(sideName(side)) + "出发格 (" + std::to_string(cx + 1) + "," +
                std::to_string(cy + 1) + ") 上不是本方棋子";
            return false;
        }
        if (seen[numOf(v)]) {
            err = std::string(sideName(side)) + "出发区里有重复的 " + std::to_string(numOf(v)) + " 号棋子";
            return false;
        }
        seen[numOf(v)] = true;
    }
    for (int i = 1; i <= 6; ++i) {
        if (!seen[i]) {
            err = std::string(sideName(side)) + "出发区缺少 " + std::to_string(i) + " 号棋子";
            return false;
        }
    }
    return true;
}

// 掷骰子，返回 1~6
int rollDice() { return rng().range(1, 6); }

}  // namespace rules


// 单方评估：推进距离 + 子力 + 走子灵活度
double Ai::sideValue(const PieceMap& pm, int side) const
{
    double v = 0.0;
    double flex = 0.0;

    for (int num = 1; num <= 6; ++num) {
        if (!pm.present[side][num]) continue;
        v += static_cast<double>(kSize - rules::distToGoal(side, pm.at[side][num])) * wProg_;
    }
    v += static_cast<double>(pm.count[side]) * wCnt_;

    for (int d = 1; d <= 6; ++d) {
        int cand[2];
        const int nc = rules::pickCandidates(pm.present[side], d, cand);
        int best = kSize * 2;

        for (int i = 0; i < nc; ++i) {
            const int dd = rules::distToGoal(side, pm.at[side][cand[i]]);

            if (dd < best) best = dd;
        }
        flex += static_cast<double>(best);
    }
    return v - flex * wFlex_;
}

// 局面评估（从 side 的视角）：双方单方评估相减
double Ai::evalMap(const PieceMap& pm, int side) const
{
    return sideValue(pm, side) - sideValue(pm, rules::other(side));
}

// 同上，但输入是局面（内部建一次位置表）
double Ai::evalPosition(const Position& p, int side) const
{
    PieceMap pm;

    rules::locateAll(p, pm);
    return evalMap(pm, side);
}

// 常数时间判断这一步是不是立即获胜（搜索内层用，避免全盘数子）
bool Ai::quickWin(const Position& p, const Move& m, int mover, int oppCount)
{
    if (m.to == rules::goalCell(mover)) return true;

    const int capNum = p.cell[m.to.y][m.to.x];
    if (capNum != kEmpty && sideOf(capNum) == rules::other(mover) && oppCount == 1) return true;
    return false;
}

// 期望极小极大搜索：对 6 个骰子求平均，depth 是剩余层数
double Ai::search(const Position& p, int depth)
{
    const double kWinScore = 1000000.0;
    const int mover = p.turn;
    const int opp = rules::other(mover);
    PieceMap pm;
    double total = 0.0;
    int diceCount = 0;

    ++nodes_;

    rules::locateAll(p, pm);
    const int oppCount = pm.count[opp];
    if (pm.count[mover] == 0) return -kWinScore;
    if (oppCount == 0) return kWinScore;

    for (int d = 1; d <= 6; ++d) {
        std::array<Move, 6> moves;
        const int n = rules::legalMoves(pm, mover, d, moves);
        double best = -1e18;

        if (n == 0) continue;
        ++diceCount;

        for (int i = 0; i < n; ++i) {
            double v;

            if (quickWin(p, moves[i], mover, oppCount)) {
                v = kWinScore;
            } else {
                Position c = p;

                rules::doMove(c, mover, moves[i]);
                if (depth <= 1) {
                    v = evalPosition(c, mover);
                } else {
                    v = -search(c, depth - 1);
                }
            }
            if (v > best) best = v;
        }
        total += best;
    }
    if (diceCount == 0) return evalPosition(p, mover);
    return total / static_cast<double>(diceCount);
}

// 对外接口：按难度选一步棋（简单=随机、普通=2 层、困难=5 层）
bool Ai::choose(const Position& p, int dice, int level, Move& best)
{
    PieceMap pm;
    std::array<Move, 6> moves;
    const int mover = p.turn;

    rules::locateAll(p, pm);
    const int n = rules::legalMoves(pm, mover, dice, moves);
    if (n <= 0) return false;

    const int oppCount = pm.count[rules::other(mover)];

    if (level <= kEasy) {
        best = moves[static_cast<std::size_t>(rng().range(0, n - 1))];
        return true;
    }

    const int depth = (level == kNormal) ? 1 : depth_;
    const double kWinScore = 1000000.0;
    const double t0 = nowMs();
    double bestv = -1e18;
    int ties[6];
    int nt = 0;

    nodes_ = 0;

    for (int i = 0; i < n; ++i) {
        double v;

        if (quickWin(p, moves[i], mover, oppCount)) {
            v = kWinScore;
        } else {
            Position c = p;

            rules::doMove(c, mover, moves[i]);
            if (depth <= 1) {
                v = evalPosition(c, mover);
            } else {
                v = -search(c, depth - 1);
            }
        }

        if (v > bestv + 1e-9) {
            bestv = v;
            nt = 0;
            ties[nt++] = i;
        } else if (v > bestv - 1e-9) {
            ties[nt++] = i;
        }
    }
    lastMs_ = nowMs() - t0;

    best = moves[static_cast<std::size_t>(ties[rng().range(0, nt - 1)])];
    return true;
}




// 存档解析用的小工具（只在本文件可见）
namespace {

// FNV-1a 哈希，用作存档校验和
unsigned int fnv1a(const std::string& s)
{
    unsigned int h = 2166136261u;

    for (std::size_t i = 0; i < s.size(); ++i) {
        h ^= static_cast<unsigned char>(s[i]);
        h *= 16777619u;
    }
    return h;
}

// 存档里的两字符文本 → 棋子编码
int parsePiece(const std::string& s)
{
    if (s == ".." || s == ".") return 0;
    if (s.size() != 2) return -1;

    int side;

    if (s[0] == 'R') side = kRed;
    else if (s[0] == 'B') side = kBlue;
    else return -1;

    const int num = s[1] - '0';

    if (num < 1 || num > 6) return -1;
    return piece(side, num);
}

}  // namespace

// 把对局写成固定格式的文本（不含最后的校验行）
std::string Game::serialize() const
{
    std::ostringstream os;

    os << "EINSTEIN-SAVE 1\n";
    os << "TURN " << rules::sideChar(pos.turn) << "\n";
    os << "HUMAN " << rules::sideChar(human) << "\n";
    os << "PHASE " << (phase == kPhaseRoll ? 'R' : (phase == kPhaseMove ? 'M' : 'O')) << "\n";
    os << "DICE " << dice << "\n";
    os << "RNG " << rng().state() << "\n";   // 随机数状态：读档后骰子序列才能完全复现
    os << "LEVEL " << level << "\n";
    os << "WINNER " << (winner == kNoSide ? "-" : rules::sideChar(winner)) << "\n";
    os << "MOVES " << records.size() << "\n";

    os << "BOARD\n";
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            if (x) os << ' ';
            os << rules::saveCellText(pos.cell[y][x]);
        }
        os << "\n";
    }

    os << "RECORD\n";
    for (std::size_t i = 0; i < records.size(); ++i) {
        const Record& r = records[i];

        os << rules::sideChar(r.side) << ' ' << r.dice << ' ' << r.num << ' '
            << r.from.x << ' ' << r.from.y << ' ' << r.to.x << ' ' << r.to.y << ' ' << r.captured << "\n";
    }
    os << "END\n";
    return os.str();
}

// 保存到文件：正文 + 校验行
bool Game::save(const std::string& path, std::string& err) const
{
    const std::string text = serialize();
    const unsigned int sum = fnv1a(text);
    std::ofstream file(path.c_str(), std::ios::binary);

    if (!file) {
        err = "无法创建存档文件";
        return false;
    }
    file << text << "SUM " << hex8(sum) << "\n";
    if (!file) {
        err = "写入存档文件失败";
        return false;
    }
    return true;
}

// 读档后的自检：阵营、阶段、骰子、胜负、棋盘、走棋记录是否自洽
bool Game::validate(std::string& err) const
{
    if (human != kRed && human != kBlue) {
        err = "阵营不合法";
        return false;
    }
    if (pos.turn != kRed && pos.turn != kBlue) {
        err = "当前行动方不合法";
        return false;
    }
    if (level < kEasy || level > kHard) {
        err = "AI 难度不合法";
        return false;
    }
    if (phase != kPhaseRoll && phase != kPhaseMove && phase != kPhaseOver) {
        err = "对局阶段不合法";
        return false;
    }
    if (dice < 0 || dice > 6) {
        err = "棋子编码不合法";
        return false;
    }
    if (phase == kPhaseMove && dice == 0) {
        err = "阶段为等待掷骰，却没有骰子点数";
        return false;
    }
    if (phase != kPhaseMove && dice != 0) {
        err = "未掷骰的等待阶段却已有骰子点数";
        return false;
    }
    if (winner != kNoSide && winner != kRed && winner != kBlue) {
        err = "获胜方不合法";
        return false;
    }
    if ((phase == kPhaseOver) != (winner != kNoSide)) {
        err = "对局阶段与获胜方不一致";
        return false;
    }
    if (records.size() > static_cast<std::size_t>(kMaxRecords)) {
        err = "走棋记录条数不合法";
        return false;
    }

    bool seen[kBlue + 1][7] = {{false}};

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const int v = pos.cell[y][x];

            if (v == kEmpty) continue;
            if (v < 0 || sideOf(v) < kRed || sideOf(v) > kBlue ||
                numOf(v) < 1 || numOf(v) > 6) {
                err = "棋盘上出现非法棋子";
                return false;
            }
            if (seen[sideOf(v)][numOf(v)]) {
                err = std::string(rules::sideName(sideOf(v))) + std::to_string(numOf(v)) +
                      " 号棋子重复出现";
                return false;
            }
            seen[sideOf(v)][numOf(v)] = true;
        }
    }

    const int cntRed = rules::countPieces(pos, kRed);
    const int cntBlue = rules::countPieces(pos, kBlue);

    if (phase != kPhaseOver) {
        if (cntRed == 0 || cntBlue == 0) {
            err = "对局尚未结束却有一方没有棋子";
            return false;
        }
    } else {
        const Pos goal = rules::goalCell(winner);

        if (!((pos.cell[goal.y][goal.x] != kEmpty &&
               sideOf(pos.cell[goal.y][goal.x]) == winner) ||
              rules::countPieces(pos, rules::other(winner)) == 0)) {
            err = "获胜方与最终局面不一致";
            return false;
        }
    }

    for (std::size_t i = 0; i < records.size(); ++i) {
        const Record& r = records[i];

        if ((r.side != kRed && r.side != kBlue) || r.num < 1 || r.num > 6 ||
            r.dice < 1 || r.dice > 6 ||
            r.from.x < 1 || r.from.x > kSize || r.from.y < 1 || r.from.y > kSize ||
            r.to.x < 1 || r.to.x > kSize || r.to.y < 1 || r.to.y > kSize) {
            err = "走棋记录第 " + std::to_string(i + 1) + " 条数据不合法";
            return false;
        }
    }
    return true;
}

// 从文件读取对局：先校验再解析，失败时不改动当前对局
bool Game::load(const std::string& path, std::string& err)
{
    std::ifstream file(path.c_str(), std::ios::binary);

    if (!file) {
        err = "存档文件不存在或无法打开";
        return false;
    }

    std::ostringstream all;
    all << file.rdbuf();
    std::string text = all.str();

    // 去掉末尾的所有换行（Windows 存档可能是 "\r\n"，Unix 是 "\n"）
    while (!text.empty() && (text[text.size() - 1] == '\n' || text[text.size() - 1] == '\r'))
        text.erase(text.size() - 1);

    // 空文件、或者只有空行的文件，到这里都是 empty
    if (text.empty()) {
        err = "存档文件为空";
        return false;
    }

    const std::size_t sumPos = text.rfind('\n');
    std::string sumLine;

    if (sumPos == std::string::npos) {
        err = "存档文件缺少校验行";
        return false;
    }
    sumLine = text.substr(sumPos + 1);
    if (sumLine.compare(0, 4, "SUM ") != 0) {
        err = "存档文件缺少校验行";
        return false;
    }

    unsigned int expect = 0;
    {
        std::istringstream iss(sumLine);
        std::string tag;

        iss >> tag >> std::hex >> expect;
        if (!iss) {
            err = "存档文件校验行格式错误";
            return false;
        }
    }
    if (fnv1a(text.substr(0, sumPos + 1)) != expect) {
        err = "存档内容校验失败（文件可能被修改或损坏）";
        return false;
    }

    std::vector<std::string> lines;
    {
        std::istringstream iss(text.substr(0, sumPos + 1));
        std::string line;

        while (std::getline(iss, line)) {
            if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
            lines.push_back(line);
        }
    }

    std::size_t i = 0;

// 前进一行，并确认这一行存在（越界就报"存档内容不完整"并返回）。
// 每读完一个字段 / 一行都调一次，保证接下来访问 lines[i] 是安全的。
#define NEXT_LINE()                                        \
    do {                                                   \
        ++i;                                               \
        if (i >= lines.size()) {                           \
            err = "存档内容不完整";                        \
            return false;                                  \
        }                                                  \
    } while (0)

    Game tmp;
    unsigned long long rngState = 0;       // 存档里记下的随机数状态

    if (i >= lines.size() || lines[i] != "EINSTEIN-SAVE 1") {
        err = "这不是本程序的存档文件";
        return false;
    }
    NEXT_LINE();
    if (lines[i].compare(0, 5, "TURN ") != 0 ||
        (lines[i][5] != 'R' && lines[i][5] != 'B') || lines[i].size() != 6) {
        err = "字段 TURN 格式错误";
        return false;
    }
    tmp.pos.turn = (lines[i][5] == 'R') ? kRed : kBlue;
    NEXT_LINE();
    if (lines[i].compare(0, 6, "HUMAN ") != 0 ||
        (lines[i][6] != 'R' && lines[i][6] != 'B') || lines[i].size() != 7) {
        err = "字段 HUMAN 格式错误";
        return false;
    }
    tmp.human = (lines[i][6] == 'R') ? kRed : kBlue;
    NEXT_LINE();
    if (lines[i].compare(0, 6, "PHASE ") != 0 || lines[i].size() != 7) {
        err = "字段 PHASE 格式错误";
        return false;
    }
    switch (lines[i][6]) {
    case 'R': tmp.phase = kPhaseRoll; break;
    case 'M': tmp.phase = kPhaseMove; break;
    case 'O': tmp.phase = kPhaseOver; break;
    default:
        err = "字段 PHASE 取值不合法";
        return false;
    }
    NEXT_LINE();
    {
        std::istringstream iss(lines[i]);
        std::string key;

        iss >> key >> tmp.dice;
        if (key != "DICE" || !iss) {
            err = "字段 DICE 格式错误";
            return false;
        }
    }
    // 必填字段 RNG <状态>：读档后骰子序列靠它复现
    NEXT_LINE();
    {
        std::istringstream iss(lines[i]);
        std::string key;

        iss >> key >> rngState;
        if (key != "RNG" || !iss) {
            err = "字段 RNG 缺失或格式错误";
            return false;
        }
    }
    NEXT_LINE();
    {
        std::istringstream iss(lines[i]);
        std::string key;

        iss >> key >> tmp.level;
        if (key != "LEVEL" || !iss) {
            err = "字段 LEVEL 格式错误";
            return false;
        }
    }
    NEXT_LINE();
    {
        const std::string& s = lines[i];

        if (s.compare(0, 7, "WINNER ") != 0) {
            err = "字段 WINNER 格式错误";
            return false;
        }
        const std::string v = s.substr(7);

        if (v == "-") tmp.winner = kNoSide;
        else if (v == "R") tmp.winner = kRed;
        else if (v == "B") tmp.winner = kBlue;
        else {
            err = "字段 WINNER 取值不合法";
            return false;
        }
    }
    NEXT_LINE();
    int movesDeclared = -1;
    {
        std::istringstream iss(lines[i]);
        std::string key;

        iss >> key >> movesDeclared;
        if (key != "MOVES" || !iss || movesDeclared < 0 || movesDeclared > kMaxRecords) {
            err = "字段 MOVES 格式错误";
            return false;
        }
        tmp.records.reserve(static_cast<std::size_t>(movesDeclared));
    }
    NEXT_LINE();
    if (lines[i] != "BOARD") {
        err = "存档缺少 BOARD 段";
        return false;
    }
    NEXT_LINE();          // 越过 "BOARD"，指向棋盘第 1 行

    for (int y = 0; y < kSize; ++y) {
        std::istringstream iss(lines[i]);
        std::vector<std::string> tokens;
        std::string tok;

        while (iss >> tok) tokens.push_back(tok);
        if (static_cast<int>(tokens.size()) != kSize) {
            err = "棋盘的 " + std::to_string(y + 1) + " 行格式错误";
            return false;
        }
        for (int x = 0; x < kSize; ++x) {
            const int v = parsePiece(tokens[static_cast<std::size_t>(x)]);

            if (v < 0) {
                err = "棋盘的 " + std::to_string(y + 1) + " 行出现非法棋子";
                return false;
            }
            tmp.pos.cell[y][x] = static_cast<Cell>(v);
        }
        NEXT_LINE();      // 末尾前进：最后一行走完，正好落到 "RECORD" 上
    }

    // 棋盘循环最后一次 NEXT_LINE 已经保证 i 有效，这里直接比
    if (lines[i] != "RECORD") {
        err = "存档缺少 RECORD 段";
        return false;
    }
    NEXT_LINE();          // 越过 "RECORD"，指向第一条记录（或者 "END"）

    {
        std::size_t recorded = 0;

        for (;;) {
            if (lines[i] == "END") break;

            std::istringstream iss(lines[i]);
            std::string sc;
            Record r;

            if (!(iss >> sc >> r.dice >> r.num >> r.from.x >> r.from.y >> r.to.x >> r.to.y >> r.captured)) {
                err = "走棋记录第 " + std::to_string(recorded + 1) + " 行格式错误";
                return false;
            }
            if (sc == "R") r.side = kRed;
            else if (sc == "B") r.side = kBlue;
            else {
                err = "走棋记录第 " + std::to_string(recorded + 1) + " 行的阵营不合法";
                return false;
            }
            tmp.records.push_back(r);
            ++recorded;
            NEXT_LINE();      // 前进到下一条记录，越界会在这里报错
        }
    }

    if (tmp.records.size() > static_cast<std::size_t>(kMaxRecords)) {
        err = "走棋记录条数超出范围";
        return false;
    }
    if (static_cast<int>(tmp.records.size()) != movesDeclared) {
        err = "走棋记录条数与 MOVES 字段不一致";
        return false;
    }

#undef NEXT_LINE

    if (!tmp.validate(err)) return false;

    tmp.ai = rules::other(tmp.human);
    tmp.active = true;
    tmp.winReason = 0;
    if (tmp.winner != kNoSide) {
        const Pos goal = rules::goalCell(tmp.winner);

        if (tmp.pos.cell[goal.y][goal.x] != kEmpty &&
            sideOf(tmp.pos.cell[goal.y][goal.x]) == tmp.winner)
            tmp.winReason = kWinCorner;
        else
            tmp.winReason = kWinEmpty;
    }
    // 恢复随机数状态：读档后摇出的骰子和存档那一刻完全接得上
    rng().setState(rngState);

    *this = tmp;
    return true;
}


}  // namespace einstein
