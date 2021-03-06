#include "../interface/PedestalTask.h"

#include <algorithm>

#include "DataFormats/EcalRawData/interface/EcalDCCHeaderBlock.h"
#include "DataFormats/DetId/interface/DetId.h"
#include "DataFormats/EcalDigi/interface/EcalDataFrame.h"

#include "DQM/EcalCommon/interface/EcalDQMCommonUtils.h"

namespace ecaldqm {

  PedestalTask::PedestalTask(const edm::ParameterSet &_params, const edm::ParameterSet& _paths) :
    DQWorkerTask(_params, _paths, "PedestalTask"),
    MGPAGains_(),
    MGPAGainsPN_()
  {
    using namespace std;

    collectionMask_ = 
      (0x1 << kEBDigi) |
      (0x1 << kEEDigi) |
      (0x1 << kPnDiodeDigi);

    edm::ParameterSet const& commonParams(_params.getUntrackedParameterSet("Common"));
    MGPAGains_ = commonParams.getUntrackedParameter<std::vector<int> >("MGPAGains");
    MGPAGainsPN_ = commonParams.getUntrackedParameter<std::vector<int> >("MGPAGainsPN");

    for(std::vector<int>::iterator gainItr(MGPAGains_.begin()); gainItr != MGPAGains_.end(); ++gainItr)
      if(*gainItr != 1 && *gainItr != 6 && *gainItr != 12) throw cms::Exception("InvalidConfiguration") << "MGPA gain" << std::endl;

    for(std::vector<int>::iterator gainItr(MGPAGainsPN_.begin()); gainItr != MGPAGainsPN_.end(); ++gainItr)
      if(*gainItr != 1 && *gainItr != 16) throw cms::Exception("InvalidConfiguration") << "PN diode gain" << std::endl;	

    map<string, string> replacements;
    stringstream ss;

    for(vector<int>::iterator gainItr(MGPAGains_.begin()); gainItr != MGPAGains_.end(); ++gainItr){
      ss.str("");
      ss << *gainItr;
      replacements["gain"] = ss.str();

      unsigned offset(0);
      switch(*gainItr){
      case 1: offset = 0; break;
      case 6: offset = 1; break;
      case 12: offset = 2; break;
      default: break;
      }

      MEs_[kOccupancy + offset]->name(replacements);
      MEs_[kPedestal + offset]->name(replacements);
    }

    for(vector<int>::iterator gainItr(MGPAGainsPN_.begin()); gainItr != MGPAGainsPN_.end(); ++gainItr){
      ss.str("");
      ss << *gainItr;
      replacements["pngain"] = ss.str();

      unsigned offset(0);
      switch(*gainItr){
      case 1: offset = 0; break;
      case 16: offset = 1; break;
      default: break;
      }

      MEs_[kPNOccupancy + offset]->name(replacements);
      MEs_[kPNPedestal + offset]->name(replacements);
    }
  }

  PedestalTask::~PedestalTask()
  {
  }

  void
  PedestalTask::bookMEs()
  {
    for(std::vector<int>::iterator gainItr(MGPAGains_.begin()); gainItr != MGPAGains_.end(); ++gainItr){
      unsigned offset(0);
      switch(*gainItr){
      case 1: offset = 0; break;
      case 6: offset = 1; break;
      case 12: offset = 2; break;
      default: break;
      }

      MEs_[kOccupancy + offset]->book();
      MEs_[kPedestal + offset]->book();
    }
    for(std::vector<int>::iterator gainItr(MGPAGainsPN_.begin()); gainItr != MGPAGainsPN_.end(); ++gainItr){
      unsigned offset(0);
      switch(*gainItr){
      case 1: offset = 0; break;
      case 16: offset = 1; break;
      default: break;
      }

      MEs_[kPNOccupancy + offset]->book();
      MEs_[kPNPedestal + offset]->book();
    }
  }

  bool
  PedestalTask::filterRunType(const std::vector<short>& _runType)
  {
    bool enable(false);

    for(int iFED(0); iFED < 54; iFED++){
      if(_runType[iFED] == EcalDCCHeaderBlock::PEDESTAL_STD ||
	 _runType[iFED] == EcalDCCHeaderBlock::PEDESTAL_GAP){
	enable = true;
	enable_[iFED] = true;
      }
    }

    return enable;
  }

  void
  PedestalTask::runOnDigis(const EcalDigiCollection &_digis)
  {
    for(EcalDigiCollection::const_iterator digiItr(_digis.begin()); digiItr != _digis.end(); ++digiItr){
      DetId id(digiItr->id());

      int iDCC(dccId(id) - 1);

      if(!enable_[iDCC]) continue;

      // EcalDataFrame is not a derived class of edm::DataFrame, but can take edm::DataFrame in the constructor
      EcalDataFrame dataFrame(*digiItr);

      unsigned offset(0);
      int gain(0);
      switch(dataFrame.sample(0).gainId()){
      case 1: offset = 2; gain = 12; break;
      case 2: offset = 1; gain = 6; break;
      case 3: offset = 0; gain = 1; break;
      default: continue;
      }

      if(std::find(MGPAGains_.begin(), MGPAGains_.end(), gain) == MGPAGains_.end()) continue;

      MEs_[kOccupancy + offset]->fill(id);

      float mean(0.);
      for(int iSample(0); iSample < 10; iSample++)
	mean += dataFrame.sample(iSample).adc();
      mean /= 10.;

      MEs_[kPedestal + offset]->fill(id, mean);
    }
  }

  void
  PedestalTask::runOnPnDigis(const EcalPnDiodeDigiCollection &_digis)
  {
    for(EcalPnDiodeDigiCollection::const_iterator digiItr(_digis.begin()); digiItr != _digis.end(); ++digiItr){
      EcalPnDiodeDetId id(digiItr->id());

      int iDCC(dccId(id) - 1);

      if(!enable_[iDCC]) continue;

      unsigned offset(0);
      int gain(0);
      switch(digiItr->sample(0).gainId()){
      case 0: offset = 0; gain = 1; break;
      case 1: offset = 1; gain = 16; break;
      default: continue;
      }

      if(std::find(MGPAGainsPN_.begin(), MGPAGainsPN_.end(), gain) == MGPAGainsPN_.end()) continue;

      MEs_[kPNOccupancy + offset]->fill(id);

      float mean(0.);
      for(int iSample(0); iSample < 50; iSample++)
	mean += digiItr->sample(iSample).adc();
      mean /= 50.;

      MEs_[kPNPedestal + offset]->fill(id, mean);
    }
  }

  /*static*/
  void
  PedestalTask::setMEData(std::vector<MEData>& _data)
  {
    for(unsigned iGain(0); iGain < nGain; iGain++){
      _data[kOccupancy + iGain] = MEData("Occupancy", BinService::kEcal2P, BinService::kSuperCrystal, MonitorElement::DQM_KIND_TH2F);
      _data[kPedestal + iGain] = MEData("Pedestal", BinService::kSM, BinService::kCrystal, MonitorElement::DQM_KIND_TPROFILE2D);
    }
    for(unsigned iPNGain(0); iPNGain < nPNGain; iPNGain++){
      _data[kPNOccupancy + iPNGain] = MEData("PNOccupancy", BinService::kEcalMEM2P, BinService::kCrystal, MonitorElement::DQM_KIND_TH2F);
      _data[kPNPedestal + iPNGain] = MEData("PNPedestal", BinService::kSMMEM, BinService::kCrystal, MonitorElement::DQM_KIND_TPROFILE);
    }
  }

  DEFINE_ECALDQM_WORKER(PedestalTask);
}
