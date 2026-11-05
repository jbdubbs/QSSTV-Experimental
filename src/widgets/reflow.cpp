#include "reflow.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QAbstractScrollArea>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QRadioButton>
#include <QStyle>
#include <QEvent>
#include <algorithm>

namespace
{

//! how many group boxes the layout is inside: each takes its frame and margins from the room
int nesting(const QLayout *l)
{
  int n=0;
  for(const QObject *o=l->parent();o;o=o->parent())
    if(qobject_cast<const QGroupBox *>(o)) n++;
  return n;
}

int depthOf(const QObject *o)
{
  int d=0;
  for(;o;o=o->parent()) d++;
  return d;
}

//! the text broken at spaces into lines that are at most \a limit wide (a button's text does not wrap by itself)
void breakButtonText(QAbstractButton *b,int limit)
{
  QString text=b->text();
  if(text.contains('\n') || text.isEmpty()) return;
  const QFontMetrics fm(b->font());
  const int room=limit-b->style()->pixelMetric(QStyle::PM_IndicatorWidth)-b->style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing)-8;
  if(fm.horizontalAdvance(text)<=room) return;
  QString line,out;
  for(const QString &word:text.split(' ',Qt::SkipEmptyParts))
    {
      const QString tryLine=line.isEmpty() ? word : line+' '+word;
      if(!line.isEmpty() && fm.horizontalAdvance(tryLine)>room)
        {
          out+=(out.isEmpty() ? "" : "\n")+line;
          line=word;
        }
      else line=tryLine;
    }
  out+=(out.isEmpty() ? "" : "\n")+line;
  b->setText(out);
}

//! forget what the layout and its items have measured (a widget item keeps its minimum size until it is told, and the sizes of
//! a form can have changed since: the first measuring was before the font and the style were final)
void forget(QLayout *l)
{
  for(int i=0;i<l->count();i++) l->itemAt(i)->invalidate();
  l->invalidate();
}

void stackGrid(QGridLayout *g)
{
  struct Cell {int r,c;QLayoutItem *item;int minHeight;};
  QList<Cell> cells;
  for(int i=0;i<g->count();i++)
    {
      int r,c,rs,cs;
      g->getItemPosition(i,&r,&c,&rs,&cs);
      cells<<Cell{r,c,g->itemAt(i),g->rowMinimumHeight(r)};   // a row that was made tall keeps its height in the column
    }
  std::stable_sort(cells.begin(),cells.end(),[](const Cell &a,const Cell &b){return a.r!=b.r ? a.r<b.r : a.c<b.c;});
  for(int i=g->count()-1;i>=0;i--) g->takeAt(i);
  for(int c=0;c<g->columnCount();c++)
    {
      g->setColumnStretch(c,0);
      g->setColumnMinimumWidth(c,0);
    }
  for(int r=0;r<g->rowCount();r++)
    {
      g->setRowStretch(r,0);
      g->setRowMinimumHeight(r,0);
    }
  int row=0;
  for(const Cell &cell:cells)
    {
      if(cell.item->spacerItem())
        {
          delete cell.item;
          continue;
        }
      g->setRowMinimumHeight(row,cell.minHeight);
      g->addItem(cell.item,row++,0);
    }
  g->setColumnStretch(0,1);
}

void stackRow(QBoxLayout *b)
{
  b->setDirection(QBoxLayout::TopToBottom);
  for(int i=b->count()-1;i>=0;i--)
    if(b->itemAt(i)->spacerItem()) delete b->takeAt(i);
}

}

void reflowNarrow(QWidget *page,int width)
{
  if(!page || page->property("reflowed").toBool()) return;
  page->setProperty("reflowed",true);

  // the widgets first: what they ask for decides which rows have to be put in columns
  for(QLabel *l:page->findChildren<QLabel *>())
    {
      if(l->pixmap(Qt::ReturnByValue).isNull() && !l->movie() && !l->text().isEmpty() && !l->wordWrap())
        {
          l->setWordWrap(true);
          if(l->alignment() & Qt::AlignRight) l->setAlignment((l->alignment() & ~Qt::AlignHorizontal_Mask) | Qt::AlignLeft);
        }
    }
  // a widget that ignores its height takes what the layout has left over: in a page that scrolls nothing is left over, and it
  // shrinks to nothing (the single-line text boxes of the waterfall texts). A text box that was one line gets a line.
  for(QWidget *w:page->findChildren<QWidget *>())
    {
      QSizePolicy sp=w->sizePolicy();
      if(sp.verticalPolicy()!=QSizePolicy::Ignored) continue;
      if(qobject_cast<QAbstractScrollArea *>(w) && w->minimumHeight()==0 && w->maximumHeight()>=QWIDGETSIZE_MAX) w->setFixedHeight(40);
      else
        {
          sp.setVerticalPolicy(QSizePolicy::Preferred);
          w->setSizePolicy(sp);
        }
    }
  // a container whose policy cannot shrink is laid out at its preferred width, not at the least it could do (that is how the
  // form of a section asked for 466 dp when its parts could be squeezed into 350)
  for(QWidget *w:page->findChildren<QWidget *>())
    {
      if(!w->layout()) continue;
      QSizePolicy sp=w->sizePolicy();
      if(sp.horizontalPolicy()!=QSizePolicy::Ignored && !(sp.horizontalPolicy() & QSizePolicy::ShrinkFlag))
        {
          sp.setHorizontalPolicy(QSizePolicy::Preferred);
          w->setSizePolicy(sp);
        }
    }
  for(QComboBox *c:page->findChildren<QComboBox *>())
    {
      c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
      c->setMinimumContentsLength(8);
      // the combo box keeps the minimum size hint it measured before (the width of its longest entry) through both calls above,
      // and forgets it only for a change of style
      QEvent restyled(QEvent::StyleChange);
      QCoreApplication::sendEvent(c,&restyled);
    }
  for(QAbstractButton *b:page->findChildren<QAbstractButton *>())
    if(qobject_cast<QCheckBox *>(b) || qobject_cast<QRadioButton *>(b)) breakButtonText(b,width-24);

  // then the layouts, the innermost first
  QList<QLayout *> layouts=page->findChildren<QLayout *>();
  if(page->layout() && !layouts.contains(page->layout())) layouts<<page->layout();
  std::stable_sort(layouts.begin(),layouts.end(),[](const QLayout *a,const QLayout *b){return depthOf(a)>depthOf(b);});
  // Twice: what the first round measured was measured on widgets that were changed meanwhile.
  for(int round=0;round<2;round++)
  for(QLayout *l:layouts)
    {
      if(round==1) forget(l);
      const int room=width-16-14*nesting(l);
      if(QFormLayout *f=qobject_cast<QFormLayout *>(l))
        {
          f->setRowWrapPolicy(QFormLayout::WrapAllRows);
          f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
          f->setLabelAlignment(Qt::AlignLeft);
          continue;
        }
      forget(l);
      // by what it would like to be, not by the least it can be squeezed to: a table squeezed to a few characters beside its
      // buttons and a paragraph is no use to a finger
      if(l->sizeHint().width()<=room && l->minimumSize().width()<=room) continue;
      if(QGridLayout *g=qobject_cast<QGridLayout *>(l)) stackGrid(g);
      else if(QBoxLayout *b=qobject_cast<QBoxLayout *>(l))
        {
          if(b->direction()==QBoxLayout::LeftToRight || b->direction()==QBoxLayout::RightToLeft) stackRow(b);
        }
    }
  // everything that was measured on the way was measured before the rows above it were changed: measure the whole page again,
  // the innermost first
  for(QLayout *l:layouts) forget(l);
  // What a page asks for is what its widgets ask for, no more: a widget item of the page's layout was seen to report 116 dp more
  // than the widget's own minimum size hint (466 for 350), and the page then scrolled sideways although everything in it fits.
  // An explicit minimum (any positive width replaces the measured one) says it plainly.
  int need=1;
  if(QLayout *pl=page->layout())
    for(int i=0;i<pl->count();i++)
      if(QWidget *w=pl->itemAt(i)->widget()) need=qMax(need,w->minimumSizeHint().width());
  page->setMinimumWidth(need);
  page->updateGeometry();
}

QStringList reflowOffenders(QWidget *page,int width)
{
  QStringList out;
  for(QWidget *w:page->findChildren<QWidget *>())
    if(w->isVisibleTo(page) && w->minimumSizeHint().width()>width)
    {
      QString extra;
      if(const QComboBox *cb=qobject_cast<const QComboBox *>(w)) extra=QString(" (policy %1, contents %2, items %3)").arg(int(cb->sizeAdjustPolicy())).arg(cb->minimumContentsLength()).arg(cb->count());
      out<<QString("%1 '%2' wants %3%4").arg(w->metaObject()->className(),w->objectName()).arg(w->minimumSizeHint().width()).arg(extra);
    }
  if(qEnvironmentVariableIsSet("QSSTV_REFLOW_DUMP") || page->property("reflowDump").toBool())
    for(QLayout *l:page->findChildren<QLayout *>())
      {
        forget(l);
        out<<QString("   layout %1 of '%2' min %3 hint %4 items %5").arg(l->metaObject()->className(),l->parentWidget() ? l->parentWidget()->objectName() : QString()).arg(l->minimumSize().width()).arg(l->sizeHint().width()).arg(l->count());
      }
  // and the layouts that are still too wide, with the widgets in them (a row that could not be put in a column)
  QList<QLayout *> layouts=page->findChildren<QLayout *>();
  if(page->layout()) layouts<<page->layout();
  for(QLayout *l:layouts)
    {
      forget(l);
      if(l->minimumSize().width()<=width) continue;
      QStringList parts;
      for(int i=0;i<l->count();i++)
        {
          QLayoutItem *it=l->itemAt(i);
          if(it->widget()) parts<<QString("%1 '%2' %3 (hint %4, set %5, policy %6, size hint %7)").arg(it->widget()->metaObject()->className(),it->widget()->objectName()).arg(it->minimumSize().width()).arg(it->widget()->minimumSizeHint().width()).arg(it->widget()->minimumWidth()).arg(int(it->widget()->sizePolicy().horizontalPolicy())).arg(QString("%1, item hint %2, item max %3, widget max %4, min %5x%6, hfw %7, layout min %8").arg(it->widget()->sizeHint().width()).arg(it->sizeHint().width()).arg(it->maximumSize().width()).arg(it->widget()->maximumWidth()).arg(it->widget()->minimumSize().width()).arg(it->widget()->minimumSize().height()).arg(int(it->hasHeightForWidth())).arg(it->widget()->layout() ? it->widget()->layout()->minimumSize().width() : -1));
          else if(it->layout()) parts<<QString("[%1 %2]").arg(it->layout()->metaObject()->className()).arg(it->layout()->minimumSize().width());
          else parts<<"spacer";
        }
      out<<QString("layout %1 of '%2' wants %3: %4").arg(l->metaObject()->className(),l->parentWidget() ? l->parentWidget()->objectName() : QString()).arg(l->minimumSize().width()).arg(parts.join(", "));
    }
  return out;
}
