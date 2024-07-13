#pragma once

#include "tts/services/azure.hh"
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QWidget>

namespace TTS {
class ConfigWindow: public QWidget
{
  Q_OBJECT

public:
  explicit ConfigWindow( QWidget * parent, const QString & configRootPath );

signals:
  void service_changed();

private:
  QGridLayout * MainLayout;
  QGroupBox * configPane;

  QDialogButtonBox * buttonBox;
  QLineEdit * previewLineEdit;
  QPushButton * previewButton;

  QString currentService;
  QDir configRootDir;

  QComboBox * serviceSelector;

  QScopedPointer< TTS::Service > previewService;
  QScopedPointer< TTS::ServiceConfigWidget > serviceConfigUI;

  void setupUi();

private slots:
  void updateConfigPaneBasedOnCurrentService();
};

} // namespace TTS