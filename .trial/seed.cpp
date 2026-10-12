// Writes the audio device settings with the app's own organisation/application names so the
// native macOS preferences domain is guaranteed to match what qsstv reads.
#include <QCoreApplication>
#include <QSettings>
int main(int argc,char**argv)
{
  QCoreApplication app(argc,argv);
  QCoreApplication::setOrganizationName("ON4QZ");
  QCoreApplication::setApplicationName("qsstv_9.0");
  QSettings s;
  s.beginGroup("SOUND");
  s.setValue("inputAudioDevice",QString::fromUtf8(argv[1]));
  s.setValue("outputAudioDevice",QString::fromUtf8(argv[2]));
  s.endGroup();
  s.sync();
  return 0;
}
