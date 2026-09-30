/***************************************************************************
 *   mmsstv-linux-port: mode-specific RX options (wide video filter,       *
 *   JB60 chroma sharpening), moved off the main Receive > SSTV tab and    *
 *   into their own Options > Configuration tab.                          *
 ***************************************************************************/
#include "modeoptionsconfig.h"
#include "ui_modeoptionsconfig.h"
#include "videofilterselection.h"
#include "chromadeconvolution.h"
#include "supportfunctions.h"

modeOptionsConfig::modeOptionsConfig(QWidget *parent) : baseConfig(parent), ui(new Ui::modeOptionsConfig)
{
  ui->setupUi(this);
  // Gray out when Wide Video Filter is on -- the deconvolution kernel is matched only to the
  // narrow filter, so it's inert whenever Cr/Cb come from the wide track instead (modejb60.cpp
  // has the matching behavioral guard). toggled fires on both user clicks and setChecked() during
  // settings load, so this stays correct on dialog open too (setParams() below also sets it
  // explicitly, since relying on signal timing alone at construction is fragile).
  connect(ui->wideFilterCheckBox,SIGNAL(toggled(bool)),ui->chromaDeconvolutionCheckBox,SLOT(setDisabled(bool)));
}

modeOptionsConfig::~modeOptionsConfig()
{
  delete ui;
}

void modeOptionsConfig::readSettings()
{
  setParams();
}

void modeOptionsConfig::writeSettings()
{
  getParams();
}

void modeOptionsConfig::getParams()
{
  bool wideFilter;
  getValue(wideFilter,ui->wideFilterCheckBox);
  setWideVideoFilterEnabled(wideFilter);
  bool chromaDeconv;
  getValue(chromaDeconv,ui->chromaDeconvolutionCheckBox);
  setChromaDeconvolutionEnabled(chromaDeconv);
}

void modeOptionsConfig::setParams()
{
  setValue(wideVideoFilterEnabled(),ui->wideFilterCheckBox);
  setValue(chromaDeconvolutionEnabled(),ui->chromaDeconvolutionCheckBox);
  ui->chromaDeconvolutionCheckBox->setDisabled(wideVideoFilterEnabled());
}
