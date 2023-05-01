#include "mruqmenu.hh"
#include <QKeyEvent>

MRUQMenu::MRUQMenu(const QString title, QWidget *parent):
  QMenu(title,parent)
{}

void MRUQMenu::keyReleaseEvent (QKeyEvent * kev){
  if (kev->key() == Qt::Key_Control && actions().size() > 1){
    QAction *act = activeAction();
    if( act == nullptr ){
      act = actions().at( 1 );
    }
    emit requestTabChange( act->data().toInt() );
    hide();
  }
  else {
    kev->ignore();
  }
};
