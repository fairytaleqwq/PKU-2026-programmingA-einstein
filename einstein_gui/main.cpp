#include <QApplication>
#include <QRandomGenerator>
#include <QStringList>

#include "core.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);        // Qt 应用对象：事件循环、全局资源都归它管
    app.setApplicationName(QStringLiteral("爱恩斯坦棋"));

    // ---------------------------------------------------------------
    //  给 core 的随机数发生器播种
    //
    //  core 里的 rng() 初始状态是写死的（88172645463325252），
    //  不播种的话每次运行都会得到同一串骰子和同一种随机布阵。
    //  默认用系统熵播种；想复现某一盘就传：einstein_gui --seed 12345
    // ---------------------------------------------------------------
    quint64 seed = QRandomGenerator::global()->generate64();   // 随机数种子：默认取系统熵
    bool seedFromArg = false;                                  // 这个种子是不是 --seed 指定的

    const QStringList args = QCoreApplication::arguments();    // 命令行参数列表
    const int idx = args.indexOf(QStringLiteral("--seed"));    // --seed 出现的位置，-1 = 没传

    if (idx >= 0 && idx + 1 < args.size()) {
        bool ok = false;                                            // 数字解析是否成功
        const qulonglong given = args.at(idx + 1).toULongLong(&ok); // --seed 后面跟的那个数
        if (ok) {
            seed = given;                                           // 改用命令行给的种子
            seedFromArg = true;
        }
    }

    einstein::rng().seed(seed);         // 播种：必须赶在第一局开局之前

    MainWindow window;                  // 主窗口

    // 指定了种子就把种子显示在标题上，方便"同一盘棋"复现
    if (seedFromArg) {
        window.setWindowTitle(
            QStringLiteral("爱恩斯坦棋 —— Qt 图形版（种子 %1）").arg(seed));
    }

    window.show();                      // 显示窗口（此时窗口才真正出现在屏幕上）

    return app.exec();                  // 进入事件循环，程序停在这里直到关窗口
}
