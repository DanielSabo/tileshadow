#ifndef ORA_H
#define ORA_H

#include "canvasstack.h"
#include <functional>

void saveStackAs(CanvasStack *stack, QString path,
                 std::function<void(QString const &msg, float percent)> progressCallback = [](QString const &, float) {});
void loadStackFromORA(CanvasStack *stack, QString path);

#endif // ORA_H
