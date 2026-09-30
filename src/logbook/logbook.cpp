#include "logbook.h"
#include "appglobal.h"
#include "xmlrpc/xmlinterface.h"
#include "configparams.h"
#include "rigcontrol.h"
#include <QDateTime>
#include <QSettings>



slogParam logParamArray[NUMLOGPARAMS]=
{
    {"program","QSSTV 9"},
    {"version", "1"},
    {"date","" },
    {"time",""  },
    {"endTime",""  },
    {"call","" , },
    {"mhz",""  },
    {"mode",""  },
    {"tx",""  },
    {"rx",""  },
    {"name","" },
    {"qth","" },
    {"state","" },
    {"province","" },
    {"country","" },
    {"locator","" },
    {"serialout","" },
    {"serialin","" },
    {"free1","" },
    {"notes","" },
    {"power","" }
};

// Default port for the ADIF-over-UDP broadcast below (see buildADIFRecord()). There is no
// single fixed standard port -- WSJT-X/JS8Call/N1MM+ etc. all make this configurable in
// their own reporting settings, and CQRLOG's ADIF-remote listener (a program already
// referenced in this project's own docs) works the same way. Whatever a receiving
// logger's UDP listener is set to, point it at this: QSettings key LOGBOOK/udpPort.
#define DEFAULTLOGBOOKUDPPORT 2333

logBook::logBook()
{
  QSettings qSettings;
  qSettings.beginGroup("LOGBOOK");
  udpPort=(quint16)qSettings.value("udpPort",DEFAULTLOGBOOKUDPPORT).toInt();
  qSettings.endGroup();
}


void logBook::logQSO(QString call,QString mode,QString comment)
{
  QDateTime dt(QDateTime::currentDateTimeUtc());
  QString tmp;
  getFrequency();
  if(frequency!=-1) setParam(LFREQ,QString::number(frequency/1000000.,'g',9));
  setParam(LCALL,call);
  setParam(LNOTES,comment);
  tmp=dt.date().toString("yyyyMMdd");
  setParam(LDATE,tmp);
  tmp=dt.time().toString("hhmmss");
  setParam(LTIME,tmp);
  setParam(LENDTIME,tmp);
  setParam(LMODE,mode);

  QByteArray record=buildADIFRecord().toUtf8();
  udpSocket.writeDatagram(record,QHostAddress::LocalHost,udpPort);
}



// A single "headerless" ADIF QSO record -- <field:length>value pairs with no ADIF file
// header, terminated by <eor> -- the same format WSJT-X's "Secondary UDP Server ->
// Enable logged contact ADIF broadcast" option sends, and what CQRLOG's ADIF-remote
// listener recognizes by the record starting with <CALL (hence CALL is emitted first
// below, not just wherever it falls in logParamArray). Fields with no value are omitted
// rather than sent empty, matching normal ADIF practice. LTX/LRX (tx/rx) and
// LPROG/LVER (program/version) have no per-QSO ADIF equivalent worth sending here --
// PROGRAMID/PROGRAMVERSION are ADIF *header* fields, meaningless in a headerless record,
// and tx/rx were never populated by this app to begin with. LPROV (province) has no
// standard core ADIF field (ADIF's STATE already covers Canadian provinces too, and this
// app already logs a separate LSTATE for that), so it goes out as an APP-namespaced
// extension field instead of overloading a core one with the wrong meaning.
QString logBook::buildADIFRecord() const
{
  QString rec;
  auto add=[&](const QString &field,const QString &value)
    {
      if(value.isEmpty()) return;
      rec+=QString("<%1:%2>%3").arg(field).arg(value.toUtf8().size()).arg(value);
    };
  add("CALL",logParamArray[LCALL].val);
  add("QSO_DATE",logParamArray[LDATE].val);
  add("TIME_ON",logParamArray[LTIME].val);
  add("TIME_OFF",logParamArray[LENDTIME].val);
  add("FREQ",logParamArray[LFREQ].val);
  add("MODE",logParamArray[LMODE].val);
  add("NAME",logParamArray[LNAME].val);
  add("QTH",logParamArray[LQTH].val);
  add("STATE",logParamArray[LSTATE].val);
  add("APP_QSSTV_PROVINCE",logParamArray[LPROV].val);
  add("COUNTRY",logParamArray[LCNTRY].val);
  add("GRIDSQUARE",logParamArray[LLOC].val);
  add("STX_STRING",logParamArray[LSO].val);
  add("SRX_STRING",logParamArray[LSI].val);
  add("COMMENT",logParamArray[LFREE].val);
  add("NOTES",logParamArray[LNOTES].val);
  add("TX_PWR",logParamArray[LPWR].val);
  rec+="<eor>";
  return rec;
}


void logBook::getFrequency()
{
  frequency=-1;

  if(rigControllerPtr->params()->enableXMLRPC) // we get the frequency from flrig or alike
    {
      frequency=xmlIntfPtr->getFrequency();
    }
  else if(rigControllerPtr->params()->enableCAT) // we get the frequency from hamlib
    {
      if(!rigControllerPtr->getFrequency(frequency))
        {
          frequency=-1;
        }
    }


}

void logBook::setParam(eIndex tag,QString value)
{
   logParamArray[tag].val=value;
}
