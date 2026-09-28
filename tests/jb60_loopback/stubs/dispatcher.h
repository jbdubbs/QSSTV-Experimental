#pragma once
#include <QObject>
#include <QEvent>
class dispatcher : public QObject {};
struct lineDisplayEvent : public QEvent { lineDisplayEvent(unsigned):QEvent(QEvent::User){} };
