/*
 * \file EcalSelectiveReadoutValidation.cc
 *
 * $Date: 2009/08/03 12:49:21 $
 * $Revision: 1.23 $
 *
 */

#include "Validation/EcalDigis/interface/EcalSelectiveReadoutValidation.h"
#include "Validation/EcalDigis/src/EcalSRPCompat.h"

#include "Validation/EcalDigis/src/ecalDccMap.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

#include "Geometry/EcalMapping/interface/EcalElectronicsMapping.h"
#include "Geometry/EcalMapping/interface/EcalMappingRcd.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloCellGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloGeometry.h"

#include "DQMServices/Core/interface/DQMStore.h"

#include "CalibCalorimetry/EcalTPGTools/interface/EcalTPGScale.h"

#include <string.h>
#include "DQMServices/Core/interface/MonitorElement.h"

#if (CMSSW_COMPAT_VERSION>=210)
#   include "Geometry/Records/interface/CaloGeometryRecord.h"
/**/typedef CaloGeometryRecord MyCaloGeometryRecord;
#else
#   include "Geometry/Records/interface/IdealGeometryRecord.h"
/**/typedef IdealGeometryRecord MyCaloGeometryRecord;
#endif

#define ML_DEBUG

using namespace cms;
using namespace edm;
using namespace std;

const double EcalSelectiveReadoutValidation::rad2deg = 45./atan(1.);

const int EcalSelectiveReadoutValidation::nDccRus_[nDccs_] ={
  //EE- DCCs:
  //   1  2   3   4   5   6   7   8   9
  /**/34, 32, 33, 33, 32, 34, 33, 34, 33,
  //EB- DCCs:
  //  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  25  26  27
  /**/68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68,
  //EB+ DCCs:
  //  28  29  30  31  32  33  34  35  36  37  38  39  40  41  42  43  44  45
  /**/68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68,
  //EE+ DCCs:
  //  46  47  48  49  50  51  52  53  54
  /**/32, 33, 33, 32, 34, 33, 34, 33, 34
};

//#define DO_TIMING

#ifdef DO_TIMING
#  include <sys/time.h>
struct PgTiming{
  PgTiming(const std::string& mess): mess_(mess), stopped_(false){
    gettimeofday(&start_, 0);
  }

  ~PgTiming(){
    if(!stopped_) stop();
  }
  void stop(){
    timeval t;
    gettimeofday(&t, 0);
    std::cout << "[PgTiming] " << mess_ << " "
	      << ((t.tv_sec-start_.tv_sec)*1.e3
		  + (t.tv_usec-start_.tv_usec)*1.e-3)
	      << " ms.\n";
    stopped_ = true;
  }
  timeval start_;
  std::string mess_;
  bool stopped_;
};
#else //DO_TIMING is not defined
struct PgTiming{
  PgTiming(const std::string& mess){}
  void stop(){}
};
#endif //DO_TIMING defined condition

EcalSelectiveReadoutValidation::EcalSelectiveReadoutValidation(const ParameterSet& ps):
  collNotFoundWarn_(ps.getUntrackedParameter<bool>("warnIfCollectionNotFound", true)),
  ebDigis_(ps.getParameter<edm::InputTag>("EbDigiCollection"), false,
	   collNotFoundWarn_),
  eeDigis_(ps.getParameter<edm::InputTag>("EeDigiCollection"), false,
	   collNotFoundWarn_),
  ebNoZsDigis_(ps.getParameter<edm::InputTag>("EbUnsuppressedDigiCollection"),
	       false, collNotFoundWarn_),
  eeNoZsDigis_(ps.getParameter<edm::InputTag>("EeUnsuppressedDigiCollection"),
	       false, collNotFoundWarn_),
  ebSrFlags_(ps.getParameter<edm::InputTag>("EbSrFlagCollection"), false,
	     collNotFoundWarn_),
  eeSrFlags_(ps.getParameter<edm::InputTag>("EeSrFlagCollection"), false,
	     collNotFoundWarn_),
  ebComputedSrFlags_(ps.getParameter<edm::InputTag>("EbSrFlagFromTTCollection"), false,
		   collNotFoundWarn_),
  eeComputedSrFlags_(ps.getParameter<edm::InputTag>("EeSrFlagFromTTCollection"), false,
		   collNotFoundWarn_),
  ebSimHits_(ps.getParameter<edm::InputTag>("EbSimHitCollection"), false,
	     collNotFoundWarn_),
  eeSimHits_(ps.getParameter<edm::InputTag>("EeSimHitCollection"), false,
	     collNotFoundWarn_),
  tps_(ps.getParameter<edm::InputTag>("TrigPrimCollection"), false,
       collNotFoundWarn_),
  ebRecHits_(ps.getParameter<edm::InputTag>("EbRecHitCollection"), false,
	     collNotFoundWarn_),
  eeRecHits_(ps.getParameter<edm::InputTag>("EeRecHitCollection"), false,
	     collNotFoundWarn_),
  fedRaw_(ps.getParameter<edm::InputTag>("FEDRawCollection"), false,
	  collNotFoundWarn_),
  tmax(0),
  tmin(numeric_limits<int64_t>::max()),
  l1aOfTmin(0),
  l1aOfTmax(0),
  triggerTowerMap_(0),
  localReco_(ps.getParameter<bool>("LocalReco")),
  weights_(ps.getParameter<vector<double> >("weights")),
  tpInGeV_(ps.getParameter<bool>("tpInGeV")),
  firstFIRSample_(ps.getParameter<int>("ecalDccZs1stSample")),
  useEventRate_(ps.getParameter<bool>("useEventRate")),
  logErrForDccs_(nDccs_, false),
  ievt_(0),
  allHists_(false),
  histDir_(ps.getParameter<string>("histDir")),
  withEeSimHit_(false),
  withEbSimHit_(false){

  PgTiming t("EcalSelectiveReadoutValidation ctor");

//   std::vector<int> excludedFeds =
//     ps.getParameter<vector<int> >("excludedFeds");

//   for(size_t i = 0; i < excludedFeds.size(); ++i){
//     int iDcc = excludedFeds[i] % 600;
//     if(iDcc < minDccId_ || iDcc > maxDccId_){
//       throw cms::Exception("config") << "Error in parameter excludedFeds of "
//	"EcalSelectiveReadoutValidation module. Values must be between "
//				     << 600 + minDccId_
//				     << " and " << 600 + maxDccId_ << "\n";
//     } else{
//       logErrForDccs_.at(iDcc-minDccId_) = false;
//     }
//   }

  double zsThr = ps.getParameter<double>("zsThrADCCount");

  zsThr_ = lround(zsThr*4);

  //File to log SRP algorithem inconsistency
  srpAlgoErrorLogFileName_
    = ps.getUntrackedParameter<string>("srpAlgoErrorLogFile","");
  logSrpAlgoErrors_ = (srpAlgoErrorLogFileName_.size()!=0);

  //File to log SRP decision application inconsistency
  srApplicationErrorLogFileName_
    = ps.getUntrackedParameter<string>("srApplicationErrorLogFile","");
  logSrApplicationErrors_ = (srApplicationErrorLogFileName_.size()!=0);

  //FIR ZS weights
  configFirWeights(ps.getParameter<vector<double> >("dccWeights"));

  // DQM ROOT output
  outputFile_ = ps.getUntrackedParameter<string>("outputFile", "");

  if(outputFile_.size() != 0){
    LogInfo("OutputInfo") << " Ecal Digi Task histograms will be saved to '"
			  << outputFile_.c_str() << "'";
  } else{
    LogInfo("OutputInfo") << " Ecal Digi Task histograms will NOT be saved";
  }

  // verbosity switch
  verbose_ = ps.getUntrackedParameter<bool>("verbose", false);

  // get hold of back-end interface
  dbe_ = Service<DQMStore>().operator->();

  if(verbose_){
    dbe_->setVerbose(1);
  } else{
    dbe_->setVerbose(0);
  }

  if(verbose_) dbe_->showDirStructure();

  dbe_->setCurrentFolder(histDir_);

  vector<string>
    hists(ps.getUntrackedParameter<vector<string> >("histograms",
						    vector<string>(1, "all")));

  for(vector<string>::iterator it = hists.begin();
      it!=hists.end(); ++it) histList_.insert(*it);
  if(histList_.find("all") != histList_.end()) allHists_ = true;

  //Data volume
  meEbFixedPayload_ = bookFloat("ebFixedVol");
  double ebFixed = getDccOverhead(EB)*nEbDccs;
  double eeFixed =  getDccOverhead(EE)*nEeDccs;
  meEbFixedPayload_->Fill(ebFixed);
  meEeFixedPayload_ = bookFloat("eeFixedVol");
  meEbFixedPayload_->Fill(eeFixed);
  meFixedPayload_ = bookFloat("fixedVol");
  meFixedPayload_->Fill(ebFixed+eeFixed);

  meL1aRate_ = bookFloat("l1aRate_"); 

  meDccVol_ = bookProfile("hDccVol", //"EcalDccEventSizeComputed",
			  "ECAL DCC event fragment size;Dcc id; "
			  "<Event size> (kB)", nDccs_, .5, .5+nDccs_);

  meDccLiVol_ = bookProfile("hDccLiVol",
                            "LI channel payload per DCC;Dcc id; "
                            "<Event size> (kB)", nDccs_, .5, .5+nDccs_);

  meDccHiVol_ = bookProfile("hDccHiVol",
                            "HI channel payload per DCC;Dcc id; "
                            "<Event size> (kB)", nDccs_, .5, .5+nDccs_);

  meDccVolFromData_ = bookProfile("hDccVolFromData", //"EcalDccEventSize",
				  "ECAL DCC event fragment size;Dcc id; "
				  "<Event size> (kB)", nDccs_, .5, .5+nDccs_);

  meVolBLI_ = book1D("hVolBLI",// "EBLowInterestPayload",
		     "ECAL Barrel low interest crystal data payload;"
		     "Event size (kB);Nevts",
		     100, 0., 200.);

  meVolELI_ = book1D("hVolELI", //"EELowInterestPayload",
		     "Endcap low interest crystal data payload;"
		     "Event size (kB);Nevts",
		     100, 0., 200.);

  meVolLI_ = book1D("hVolLI", //"EcalLowInterestPayload",
		    "ECAL low interest crystal data payload;"
		    "Event size (kB);Nevts",
		    100, 0., 200.);

  meVolBHI_ = book1D("hVolBHI", //"EBHighInterestPayload",
		     "Barrel high interest crystal data payload;"
		     "Event size (kB);Nevts",
		     100, 0., 200.);

  meVolEHI_ = book1D("hVolEHI", //"EEHighInterestPayload",
		     "Endcap high interest crystal data payload;"
		     "Event size (kB);Nevts",
		     100, 0., 200.);

  meVolHI_ = book1D("hVolHI", //"EcalHighInterestPayload",
		    "ECAL high interest crystal data payload;"
		    "Event size (kB);Nevts",
		    100, 0., 200.);

  meVolB_ = book1D("hVolB", //"EBEventSize",
		   "Barrel data volume;Event size (kB);Nevts",
		   100, 0., 200.);

  meVolE_ = book1D("hVolE", //"EEEventSize",
		   "Endcap data volume;Event size (kB);Nevts",
		   100, 0., 200.);

  meVol_ = book1D("hVol", //"EcalEventSize",
		  "ECAL data volume;Event size (kB);Nevts",
		  100, 0., 200.);

  meChOcc_ = book2D("h2ChOcc", //"EcalChannelOccupancy",
		    "ECAL crystal channel occupancy after zero suppression;"
		    "iX -200 / iEta / iX + 100;"
		    "iY / iPhi (starting from -10^{o}!);"
		    "Event count",
		    401, -200.5, 200.5,
		    360, .5, 360.5);

  //TP
  string tpUnit;
  if(tpInGeV_) tpUnit = string("GeV"); else tpUnit = string("TP hw unit");
  string title;
  title = string("Trigger primitive TT E_{T};E_{T} ")
    + tpUnit + string(";Event Count");
  meTp_ = book1D("hTp", //"EcalTriggerPrimitiveEt",
		 title.c_str(),
		 (tpInGeV_?100:40), 0., (tpInGeV_?10.:40.));

  meTtf_ = book1D("hTtf", //"EcalTriggerTowerFlag",
		  "Trigger primitive TT flag;Flag number;Event count",
		  8, -.5, 7.5);

  title = string("Trigger tower flag vs TP;E_{T}(TT) (")
    + tpUnit + string(");Flag number");
  meTtfVsTp_ = book2D("h2TtfVsTp",
		      title.c_str(),
		      100, 0., (tpInGeV_?10.:40.),
		      8, -.5, 7.5);

  meTtfVsEtSum_ = book2D("h2TtfVsEtSum",
			 "Trigger tower flag vs #sumE_{T};"
			 "E_{T}(TT) (GeV);"
			 "TTF",
			 100, 0., 10.,
			 8, -.5, 7.5);
  title = string("Trigger primitive Et (TP) vs #sumE_{T};"
		 "E_{T} (sum) (GeV);"
		 "E_{T} (TP) (") + tpUnit + string (")");

  meTpVsEtSum_ = book2D("h2TpVsEtSum",
			title.c_str(),
			100, 0., 10.,
			100, 0., (tpInGeV_?10.:40.));

  title = string("Trigger primitive E_{T};"
                 "iEta;"
                 "iPhi;"
                 "E_{T} (TP) (") + tpUnit + string (")");
  meTpMap_ = bookProfile2D("h2Tp",
                           title.c_str(),
                           57, -28.5, 28.5,
			   72, .5, 72.5);

  //SRF
  meFullRoRu_ = book2D("h2FRORu", //"EcalFullReadoutSRFlagMap",
		       "Full Read-out readout unit;"
		       "iX - 40 / iEta / iX + 20;"
		       "iY / iPhi (iPhi = 1 at phi = 0 rad);"
		       "Event count",
                       80, -39.5, 40.5,
		       72, .5, 72.5);

  meFullRoCnt_ = book1D("hFROCnt",
                        "Number of Full-readout-flagged readout units;"
                        "FRO RU count;Event count",
                        300, -.5, 299.5);

  meEbFullRoCnt_ = book1D("hEbFROCnt",
                          "Number of EB Full-readout-flagged readout units;"
                          "FRO RU count;Event count",
                          200, -.5, 199.5);

  meEeFullRoCnt_ = book1D("hEeFROCnt",
                          "Number of EE Full-readout-flagged readout units;"
                          "FRO RU count;Event count",
                          200, -.5, 199.5);

  meZs1Ru_ = book2D("h2Zs1Ru", //"EbZeroSupp1SRFlagMap",
		    "Readout unit with ZS-thr-1 flag;"
		    "iX - 40 / iEta / iX + 20;"
		    "iY0 / iPhi0 (iPhi = 1 at phi = 0 rad);"
		    "Event count",
                    80, -39.5, 40.5,
		    72, .5, 72.5);

  meForcedRu_ = book2D("h2ForcedRu", //"EcalReadoutUnitForcedBitMap",
		       "ECAL readout unit with forced bit of SR flag on;"
		       "iX - 40 / iEta / iX + 20;"
		       "iY / iPhi (iPhi = 1 at phi = 0 rad);"
		       "Event count",
                       80, -39.5, 40.5,
		       72, .5, 72.5);

  meLiTtf_ = book2D("h2LtTtf", //"EcalLowInterestTriggerTowerFlagMap",
		    "Low interest trigger tower flags;"
		    "iEta;"
		    "iPhi;"
		    "Event count",
                    57, -28.5, 28.5,
		    72, .5, 72.5);

  meMiTtf_ = book2D("h2MiTtf", //"EcalMidInterestTriggerTowerFlagMap",
		    "Mid interest trigger tower flags;"
		    "iEta;"
		    "iPhi;"
		    "Event count",
		    57, -28.5, 28.5,
                    72, .5, 72.5);

  meHiTtf_ = book2D("h2HiTtf", //"EcalHighInterestTriggerTowerFlagMap",
		    "High interest trigger tower flags;"
		    "iEta;"
		    "iPhi;"
		    "Event count",
                    57, -28.5, 28.5,
		    72, .5, 72.5);

  meForcedTtf_ = book2D("h2ForcedTtf", //"EcalTtfForcedBitMap",
                        "Trigger tower flags with forced bit set;"
                        "iEta;"
                        "iPhi;"
                        "Event count",
                        57, -28.5, 28.5,
                        72, .5, 72.5);


  const float ebMinNoise = -1.;
  const float ebMaxNoise = 1.;

  const float eeMinNoise = -1.;
  const float eeMaxNoise = 1.;

#if 0
  const float ebMinE = 0.;
  const float ebMaxE = 120.;

  const float eeMinE = 0.;
  const float eeMaxE = 120.;
#else
  const float ebMinE = ebMinNoise;
  const float ebMaxE = ebMaxNoise;

  const float eeMinE = eeMinNoise;
  const float eeMaxE = ebMaxNoise;
#endif


  const int evtMax = 500;

  meEbRecE_ = book1D("hEbRecE",
		     "Crystal reconstructed energy;E (GeV);Event count",
		     100, ebMinE, ebMaxE);

  meEbEMean_ = bookProfile("hEbEMean",
			   "EE <E_hit>;event #;<E_hit> (GeV)",
			   evtMax, .5, evtMax + .5);

  meEbNoise_ = book1D("hEbNoise",
		      "Crystal noise "
		      "(rec E of crystal without deposited energy)"
                      ";Rec E (GeV);Event count",
		      100, ebMinNoise, ebMaxNoise);

  meEbLiZsFir_   = book1D("zsEbLiFIRemu",
			  "Emulated ouput of ZS FIR filter for EB "
			  "low interest crystals;"
			  "ADC count*4;"
			  "Event count",
			  60, -30, 30);

  meEbHiZsFir_ = book1D("zsEbHiFIRemu",
                        "Emulated ouput of ZS FIR filter for EB "
                        "high interest crystals;"
                        "ADC count*4;"
                        "Event count",
                        60, -30, 30);

  //TODO: Fill this histogram...
//   meEbIncompleteRUZsFir_ = book1D("zsEbIncompleteRUFIRemu",
//                                   "Emulated ouput of ZS FIR filter for EB "
//                                   "incomplete FRO-flagged RU;"
//                                   "ADC count*4;"
//                                   "Event count",
//                                   60, -30, 30);

  meEbSimE_ = book1D("hEbSimE", "EB hit crystal simulated energy",
		     100, ebMinE, ebMaxE);

  meEbRecEHitXtal_ = book1D("hEbRecEHitXtal",
			    "EB rec energy of hit crystals",
			    100, ebMinE, ebMaxE);

  meEbRecVsSimE_ = book2D("hEbRecVsSimE",
			  "Crystal simulated vs reconstructed energy;"
			  "Esim (GeV);Erec GeV);Event count",
			  100, ebMinE, ebMaxE,
			  100, ebMinE, ebMaxE);

  meEbNoZsRecVsSimE_ = book2D("hEbNoZsRecVsSimE",
			      "Crystal no-zs simulated vs reconstructed "
			      "energy;"
			      "Esim (GeV);Erec GeV);Event count",
			      100, ebMinE, ebMaxE,
			      100, ebMinE, ebMaxE);

  meEeRecE_ = book1D("hEeRecE",
		     "EE crystal reconstructed energy;E (GeV);"
		     "Event count",
		     100, eeMinE, eeMaxE);

  meEeEMean_ = bookProfile("hEeEMean",
			   "<E_{EE hit}>;event #;<E_{hit}> (GeV)",
			   evtMax, .5, evtMax + .5);


  meEeNoise_ = book1D("hEeNoise",
		      "EE crystal noise "
		      "(rec E of crystal without deposited energy);"
		      "E (GeV); Event count",
		      200, eeMinNoise, eeMaxNoise);

  meEeLiZsFir_   = book1D("zsEeLiFIRemu",
			  "Emulated ouput of ZS FIR filter for EE "
			  "low interest crystals;"
			  "ADC count*4;"
			  "Event count",
			  60, -30, 30);

  meEeHiZsFir_   = book1D("zsEeHiFIRemu",
			  "Emulated ouput of ZS FIR filter for EE "
			  "high interest crystals;"
			  "ADC count*4;"
			  "Event count",
			  60, -30, 30);

  //TODO: Fill this histogram...
//   meEeIncompleteRUZsFir_ = book1D("zsEeIncompleteRUFIRemu",
//                                     "Emulated ouput of ZS FIR filter for EE "
//                                     "incomplete FRO-flagged RU;"
//                                     "ADC count*4;"
//                                     "Event count",
//                                     60, -30, 30);


  meEeSimE_ = book1D("hEeSimE", "EE hit crystal simulated energy",
		     100, eeMinE, eeMaxE);

  meEeRecEHitXtal_ = book1D("hEeRecEHitXtal",
			    "EE rec energy of hit crystals",
			    100, eeMinE, eeMaxE);

  meEeRecVsSimE_ = book2D("hEeRecVsSimE",
			  "EE crystal simulated vs reconstructed energy;"
			  "Esim (GeV);Erec GeV);Event count",
			  100, eeMinE, eeMaxE,
			  100, eeMinE, eeMaxE);

  meEeNoZsRecVsSimE_ = book2D("hEeNoZsRecVsSimE",
			      "EE crystal no-zs simulated vs "
			      "reconstructed "
			      "energy;Esim (GeV);Erec GeV);Event count",
			      100, eeMinE, eeMaxE,
			      100, eeMinE, eeMaxE);

  meSRFlagsConsistency_ = book2D("hSRAlgoErrorMap",
				 "TTFlags and SR Flags mismatch;"
				 "iX - 40 / iEta / iX + 20;"
				 "iY / iPhi (iPhi = 1 at phi = 0 rad);"
				 "Event count",
				 80, -39.5, 40.5,
				 72, .5, 72.5);

  //Readout Units histos (interest/Ncrystals)
  meIncompleteFROMap_ = book2D("hIncompleteFROMap",
                               "Incomplete full-readout-flagged readout units;"
                               "iX - 40 / iEta / iX + 20;"
                               "iY / iPhi (iPhi = 1 at phi = 0 rad);"
                               "Event count",
                               80, -39.5, 40.5,
                               72, .5, 72.5);

  meIncompleteFROCnt_ = book1D("hIncompleteFROCnt",
                               "Number of incomplete full-readout-flagged "
                               "readout units;"
                               "Number of RUs;Event count;",
                               200, -.5, 199.5);

  meIncompleteFRORateMap_
    = bookProfile2D("hIncompleteFRORateMap",
		    "Incomplete full-readout-flagged readout units;"
		    "iX - 40 / iEta / iX + 20;"
		    "iY / iPhi (iPhi = 1 at phi = 0 rad);"
		    "Incomplete error rate",
                    80, -39.5, 40.5,
		    72, .5, 72.5);


  meDroppedFROMap_ = book2D("hDroppedFROMap",
                            "Dropped full-readout-flagged readout units;"
                            "iX - 40 / iEta / iX + 20;"
                            "iY / iPhi (iPhi = 1 at phi = 0 rad);"
                            "Event count",
                            80, -39.5, 40.5,
                            72, .5, 72.5);

  meDroppedFROCnt_ = book1D("hDroppedFROCnt",
                            "Number of dropped full-readout-flagged "
                            "RU count;RU count;Event count",
                            200, -.5, 199.5);

  meCompleteZSCnt_ = book1D("hCompleteZsCnt",
                            "Number of zero-suppressed-flagged RU fully "
                            "readout;"
                            "RU count;Event count",
                            200, -.5, 199.5);


  meEbZsErrCnt_ = book1D("hEbZsErrCnt",
                         "Number of LI EB channels below the ZS threshold;"
                         "Channel count;Event count",
                         200, -.5, 199.5);

  meEeZsErrCnt_ = book1D("hEeZsErrCnt",
                         "Number of LI EE channels below the ZS threshold;"
                         "Channel count;Event count",
                         200, -.5, 199.5);

  meZsErrCnt_ = book1D("hZsErrCnt",
                       "Number of LI channels below the ZS threshold;"
                       "Channel count;Event count",
                       200, -.5, 199.5);

  meEbZsErrType1Cnt_ = book1D("hEbZsErrType1Cnt",
                              "Number of EB channels below the ZS "
                              "threshold in a LI but fully readout RU;"
                              "Channel count;Event count;",
                              200, -.5, 199.5);

  meEeZsErrType1Cnt_ = book1D("hEeZsErrType1Cnt",
                              "Number EE channels below the ZS threshold"
                              " in a LI but fully readout RU;"
                              "Channel count;Event count",
                              200, -.5, 199.5);

  meZsErrType1Cnt_ = book1D("hZsErrType1Cnt",
                            "Number of LI channels below the ZS threshold "
                            "in a LI but fully readout RU;"
                            "Channel count;Event count",
                            200, -.5, 199.5);


  meDroppedFRORateMap_
    = bookProfile2D("hDroppedFRORateMap",
		    "Dropped full-readout-flagged readout units"
                    "iX - 40 / iEta / iX + 20;"
                    "iY / iPhi (iPhi = 1 at phi = 0 rad);"
                    "Dropping rate",
                    80, -39.5, 40.5,
                    72, .5, 72.5);

  meCompleteZSMap_ = book2D("hCompleteZSMap",
			 "Complete zero-suppressed-flagged readout units;"
			 "iX - 40 / iEta / iX + 20;"
			 "iY / iPhi (iPhi = 1 at phi = 0 rad);"
			 "Event count",
			 80, -39.5, 40.5,
			 72, .5, 72.5);

  meCompleteZSRateMap_
    = bookProfile2D("hCompleteZSRate",
		    "Complete zero-suppressed-flagged readout units;"
		    "iX - 40 / iEta / iX + 20;"
		    "iY / iPhi (iPhi = 1 at phi = 0 rad);"
		    "Completeness rate",
		    80, -39.5, 40.5,
		    72, .5, 72.5);

  //print list of available histograms (must be called after
  //the bookXX methods):
  printAvailableHists();

  //check the histList parameter:
  stringstream s;
  for(set<string>::iterator it = histList_.begin();
      it!=histList_.end();
      ++it){
    if(*it!=string("all")
       && availableHistList_.find(*it)==availableHistList_.end()){
      s << (s.str().size()==0?"":", ") << *it;
    }
  }
  if(s.str().size()!=0){
    LogWarning("Configuration")
      << "Parameter 'histList' contains some unknown histogram(s). "
      "Check spelling. Following name were not found: "
      << s.str();
  }
}


void EcalSelectiveReadoutValidation::updateL1aRate(const edm::Event& event){
  const int32_t bx = event.bunchCrossing();
  if(bx<1 || bx > 3564) return;//throw cms::Exception("EcalSelectiveReadoutValidation")
                       //  << "bx value, " << bx << " is out of range\n";
  
  int64_t t = event.bunchCrossing() + (event.orbitNumber()-1)*3564;
  
  if(t<tmin){
    tmin = t;
    l1aOfTmin = event.id().event();
  }

  if(t>tmax){
    tmax = t;
    l1aOfTmax = event.id().event();
  }
}

double EcalSelectiveReadoutValidation::getL1aRate() const{
  cout << __FILE__ << ":" << __LINE__ << ": "
       <<  "Tmax = " << tmax << " x 25ns; Tmin = " << tmin
       << " x 25ns; L1A(Tmax) = " << l1aOfTmax << "; L1A(Tmin) = "
       << l1aOfTmin << "\n";
  return (double)(l1aOfTmax - l1aOfTmin) / ((tmax-tmin) * 25e-9);
}

void EcalSelectiveReadoutValidation::analyze(const Event& event,
					     const EventSetup& es){

  updateL1aRate(event);
  
  {
   PgTiming t("collection readout");

   //retrieves event products:
   readAllCollections(event);

  }

  withEeSimHit_ = (eeSimHits_->size()!=0);
  withEbSimHit_ = (ebSimHits_->size()!=0);

  if(ievt_<10){

  cout << "Size of TP collection: " << tps_->size() << "\n";
  cout << "Size of EB SRF collection read from data: "
       << ebSrFlags_->size() << "\n";
  cout << "Size of EB SRF collection computed from data TTFs: "
       << ebComputedSrFlags_->size() << "\n";
  cout << "Size of EE SRF collection read from data: "
       << eeSrFlags_->size() << "\n";
  cout << "Size of EE SRF collection computed from data TTFs: "
       << eeComputedSrFlags_->size() << "\n";
  }
  if(ievt_==0){
    selectFedsForLog(); //note: must be called after readAllCollection
  }

  //computes Et sum trigger tower crystals:
  setTtEtSums(es, *ebNoZsDigis_, *eeNoZsDigis_);

  {
    PgTiming t("data volume analysis");

    //Data Volume
    analyzeDataVolume(event, es);
  }

  {
    PgTiming t("EB analysis");
    //EB digis
    //must be called after analyzeDataVolume because it uses
    //isRuComplete_ array that this method fills
    analyzeEB(event, es);
  }

  {
    PgTiming t("EE analysis");
    //EE digis
    //must be called after analyzeDataVolume because it uses
    //isRuComplete_ array that this method fills
    analyzeEE(event, es);
  }

  fill(meFullRoCnt_, nEeFROCnt_+nEbFROCnt_);
  fill(meEbFullRoCnt_, nEbFROCnt_);
  fill(meEeFullRoCnt_, nEeFROCnt_);

  fill(meEbZsErrCnt_, nEbZsErrors_);
  fill(meEeZsErrCnt_, nEeZsErrors_);
  fill(meZsErrCnt_, nEbZsErrors_ + nEeZsErrors_);

  fill(meEbZsErrType1Cnt_, nEbZsErrorsType1_);
  fill(meEeZsErrType1Cnt_, nEeZsErrorsType1_);
  fill(meZsErrType1Cnt_, nEbZsErrorsType1_ + nEeZsErrorsType1_);

  {
    PgTiming t("TP analysis");
    //TP
    analyzeTP(event, es);
  }

  //SR Consistency and validation
  //SRFlagValidation(event,es);
  if(ebComputedSrFlags_->size()){
    compareSrfColl(event, *ebSrFlags_, *ebComputedSrFlags_);
  }
  if(eeComputedSrFlags_->size()){
    compareSrfColl(event, *eeSrFlags_, *eeComputedSrFlags_);
  }
  nDroppedFRO_ = 0;
  nIncompleteFRO_ = 0;
  nCompleteZS_ = 0;
  checkSrApplication(event, *ebSrFlags_);
  checkSrApplication(event, *eeSrFlags_);
  fill(meDroppedFROCnt_, nDroppedFRO_);
  fill(meIncompleteFROCnt_, nIncompleteFRO_);
  fill(meCompleteZSCnt_, nCompleteZS_);
  ++ievt_;
}


void EcalSelectiveReadoutValidation::analyzeEE(const edm::Event& event,
					       const edm::EventSetup& es){
  bool eventError = false;
  nEeZsErrors_ = 0;

  {
    PgTiming t("analyzeEE: init");
    for(int iZ0=0; iZ0<nEndcaps; ++iZ0){
      for(int iX0=0; iX0<nEeX; ++iX0){
        for(int iY0=0; iY0<nEeY; ++iY0){
          eeEnergies[iZ0][iX0][iY0].noZsRecE = -numeric_limits<double>::max();
          eeEnergies[iZ0][iX0][iY0].recE = -numeric_limits<double>::max();
          eeEnergies[iZ0][iX0][iY0].simE = 0; //must be set to zero.
          eeEnergies[iZ0][iX0][iY0].simHit = 0;
          eeEnergies[iZ0][iX0][iY0].gain12   = false;
        }
      }
    }
  }

  // gets the endcap geometry:
  edm::ESHandle<CaloGeometry> geoHandle;
  es.get<MyCaloGeometryRecord>().get(geoHandle);
  const CaloSubdetectorGeometry *geometry_p
    = (*geoHandle).getSubdetectorGeometry(DetId::Ecal, EcalEndcap);
  CaloSubdetectorGeometry const& geometry = *geometry_p;

  {
    PgTiming t("analyzeEE: unsupressed digis");
      //EE unsupressed digis:
      for (unsigned int digis=0; digis<eeNoZsDigis_->size(); ++digis){

        EEDataFrame frame = (*eeNoZsDigis_)[digis];
        int iX0 = iXY2cIndex(frame.id().ix());
        int iY0 = iXY2cIndex(frame.id().iy());
        int iZ0 = frame.id().zside()>0?1:0;

        if(iX0<0 || iX0>=nEeX){
          cout << "iX0 (= " << iX0 << ") is out of range ("
               << "[0," << nEeX -1 << "]\n";
        }
        if(iY0<0 || iY0>=nEeY){
          cout << "iY0 (= " << iY0 << ") is out of range ("
               << "[0," << nEeY -1 << "]\n";
        }
        //    cout << "EE no ZS energy computation..." ;
        eeEnergies[iZ0][iX0][iY0].noZsRecE = frame2Energy(frame);

        eeEnergies[iZ0][iX0][iY0].gain12 = true;
        for(int i = 0; i< frame.size(); ++i){
          const int gain12Code = 0x1;
          if(frame[i].gainId()!=gain12Code) eeEnergies[iZ0][iX0][iY0].gain12 =  false;
        }

        const GlobalPoint xtalPos
          = geometry.getGeometry(frame.id())->getPosition();

        eeEnergies[iZ0][iX0][iY0].phi = rad2deg*((double)xtalPos.phi());
        eeEnergies[iZ0][iX0][iY0].eta = xtalPos.eta();
      }
  }

  {
    PgTiming t("analyzeEE:rec hits");
    //EE rec hits:
    if(!localReco_){
      for(RecHitCollection::const_iterator it
            = eeRecHits_->begin();
          it != eeRecHits_->end(); ++it){
        const RecHit& hit = *it;
        int iX0 = iXY2cIndex(static_cast<const EEDetId&>(hit.id()).ix());
        int iY0 = iXY2cIndex(static_cast<const EEDetId&>(hit.id()).iy());
        int iZ0 = static_cast<const EEDetId&>(hit.id()).zside()>0?1:0;

        if(iX0<0 || iX0>=nEeX){
          cout << "iX0 (= " << iX0 << ") is out of range ("
               << "[0," << nEeX -1 << "]\n";
        }
        if(iY0<0 || iY0>=nEeY){
          cout << "iY0 (= " << iY0 << ") is out of range ("
               << "[0," << nEeY -1 << "]\n";
        }
        //    cout << "EE no ZS energy computation..." ;
        eeEnergies[iZ0][iX0][iY0].recE = hit.energy();
      }
    }
  }

  {
    PgTiming t("analyzeEE:sim hits");
    //EE sim hits:
    for(vector<PCaloHit>::const_iterator it = eeSimHits_->begin();
        it != eeSimHits_->end(); ++it){
      const PCaloHit& simHit = *it;
      EEDetId detId(simHit.id());
      int iX = detId.ix();
      int iX0 =iXY2cIndex(iX);
      int iY = detId.iy();
      int iY0 = iXY2cIndex(iY);
      int iZ0 = detId.zside()>0?1:0;
      eeEnergies[iZ0][iX0][iY0].simE += simHit.energy();
      ++eeEnergies[iZ0][iX0][iY0].simHit;
    }
  }

  {
    PgTiming t("analyzeEE: suppressed digis");

    //EE suppressed digis
    for(EEDigiCollection::const_iterator it = eeDigis_->begin();
        it != eeDigis_->end(); ++it){
      const EEDataFrame& frame = *it;
      int iX0 = iXY2cIndex(static_cast<const EEDetId&>(frame.id()).ix());
      int iY0 = iXY2cIndex(static_cast<const EEDetId&>(frame.id()).iy());
      int iZ0 = static_cast<const EEDetId&>(frame.id()).zside()>0?1:0;
      if(iX0<0 || iX0>=nEeX){
        cout << "iX0 (= " << iX0 << ") is out of range ("
             << "[0," << nEeX -1 << "]\n";
      }
      if(iY0<0 || iY0>=nEeY){
        cout << "iY0 (= " << iY0 << ") is out of range ("
             << "[0," << nEeY -1 << "]\n";
      }

      if(localReco_){
        eeEnergies[iZ0][iX0][iY0].recE = frame2Energy(frame);
      }

      eeEnergies[iZ0][iX0][iY0].gain12 = true;
      for(int i = 0; i< frame.size(); ++i){
        const int gain12Code = 0x1;
        if(frame[i].gainId()!=gain12Code){
          eeEnergies[iZ0][iX0][iY0].gain12 =  false;
        }
      }

      fill(meChOcc_, xtalGraphX(frame.id()), xtalGraphY(frame.id()));

      EESrFlagCollection::const_iterator srf
        = eeSrFlags_->find(readOutUnitOf(frame.id()));

      bool highInterest = false;


      if(srf==eeSrFlags_->end()) continue;

      if(srf!=eeSrFlags_->end()){
        highInterest = ((srf->value() & ~EcalSrFlag::SRF_FORCED_MASK)
                        == EcalSrFlag::SRF_FULL);
      }

      if(highInterest){
        fill(meEeHiZsFir_, dccZsFIR(frame, firWeights_, firstFIRSample_, 0));
      } else{
        int v = dccZsFIR(frame, firWeights_, firstFIRSample_, 0);
        fill(meEeLiZsFir_, v);
        if(v < zsThr_){
          eventError = true;
          ++nEeZsErrors_;
          pair<int,int> ru = dccCh(frame.id());
          if(isRuComplete_[ru.first][ru.second-1]) ++nEeZsErrorsType1_;
          if(nEeZsErrors_ < 3){
            srApplicationErrorLog_ << event.id() << ", "
                                   << "RU " << frame.id() << ", "
                                   << "DCC " << ru.first
                                   << " Ch : " << ru.second << ": "
                                   << "LI channel under ZS threshold.\n";
          }
          if(nEeZsErrors_==3){
            srApplicationErrorLog_ << event.id() << ": "
                                   << "more ZS errors for this event...\n";
          }
        }
      }
    } //next ZS digi.
  }

  {
    PgTiming t("analyzeEE: energies");

    for(int iZ0=0; iZ0<nEndcaps; ++iZ0){
      for(int iX0=0; iX0<nEeX; ++iX0){
        for(int iY0=0; iY0<nEeY; ++iY0){
          double recE = eeEnergies[iZ0][iX0][iY0].recE;
          if(recE==-numeric_limits<double>::max()) continue; //not a crystal or ZS
          fill(meEeRecE_, eeEnergies[iZ0][iX0][iY0].recE);

          fill(meEeEMean_, ievt_+1,
               eeEnergies[iZ0][iX0][iY0].recE);

          if(withEeSimHit_){
            if(!eeEnergies[iZ0][iX0][iY0].simHit){//noise only crystal channel
              fill(meEeNoise_, eeEnergies[iZ0][iX0][iY0].noZsRecE);
            } else{
              fill(meEeSimE_, eeEnergies[iZ0][iX0][iY0].simE);
              fill(meEeRecEHitXtal_, eeEnergies[iZ0][iX0][iY0].recE);
            }
            fill(meEeRecVsSimE_, eeEnergies[iZ0][iX0][iY0].simE,
                 eeEnergies[iZ0][iX0][iY0].recE);
            fill(meEeNoZsRecVsSimE_, eeEnergies[iZ0][iX0][iY0].simE,
                 eeEnergies[iZ0][iX0][iY0].noZsRecE);
          }
        }
      }
    }
  }

  {
    PgTiming t("analyzeEE: RU");

    nEeFROCnt_ = 0;
    char eeSrfMark[2][100][100];
    bzero(eeSrfMark, sizeof(eeSrfMark));
    //Filling RU histo
    for(EESrFlagCollection::const_iterator it = eeSrFlags_->begin();
        it != eeSrFlags_->end(); ++it){
      const EESrFlag& srf = *it;
      int iX = srf.id().ix();
      int iY = srf.id().iy();
      int iZ = srf.id().zside(); //-1 for EE-, +1 for EE+
      if(iX<1 || iY > 100) throw cms::Exception("EcalSelectiveReadoutValidation")
	<< "Found an endcap SRF with an invalid det ID: " << srf.id() << ".\n";
      ++eeSrfMark[iZ>0?1:0][iX-1][iY-1];
      if(eeSrfMark[iZ>0?1:0][iX-1][iY-1] > 1) throw cms::Exception("EcalSelectiveReadoutValidation")
	<< "Duplicate SRF for supercrystal " << srf.id() << ".\n";
      int flag = srf.value() & ~EcalSrFlag::SRF_FORCED_MASK;
      if(flag == EcalSrFlag::SRF_ZS1){
        fill(meZs1Ru_, ruGraphX(srf.id()), ruGraphY(srf.id()));
      }

      if(flag == EcalSrFlag::SRF_FULL){
	fill(meFullRoRu_, ruGraphX(srf.id()), ruGraphY(srf.id()));
	++nEeFROCnt_;
      }
      
      if(srf.value() & EcalSrFlag::SRF_FORCED_MASK){
	fill(meForcedRu_, ruGraphX(srf.id()), ruGraphY(srf.id()));
      }
    }
  }
  
  {
    PgTiming t("analyzeEE: SR appli error log");

    if(eventError) srApplicationErrorLog_ << event.id()
                                          << ": " << nEeZsErrors_
                                          << " ZS-flagged EE channels under "
                     "the ZS threshold, whose " << nEeZsErrorsType1_
                                          << " in a complete RU.\n";
  }
} //end of analyzeEE

void
EcalSelectiveReadoutValidation::analyzeEB(const edm::Event& event,
					  const edm::EventSetup& es){

    bool eventError = false;
    nEbZsErrors_ = 0;
    vector<pair<int,int> > xtalEtaPhi;

  {
    PgTiming t("analyzeEB: init");

    xtalEtaPhi.reserve(nEbPhi*nEbEta);
    for(int iEta0=0; iEta0<nEbEta; ++iEta0){
      for(int iPhi0=0; iPhi0<nEbPhi; ++iPhi0){
        ebEnergies[iEta0][iPhi0].noZsRecE = -numeric_limits<double>::max();
        ebEnergies[iEta0][iPhi0].recE = -numeric_limits<double>::max();
        ebEnergies[iEta0][iPhi0].simE = 0; //must be zero.
        ebEnergies[iEta0][iPhi0].simHit = 0;
        ebEnergies[iEta0][iPhi0].gain12 = false;
        xtalEtaPhi.push_back(pair<int,int>(iEta0, iPhi0));
      }
    }
  }

    // get the barrel geometry:
  edm::ESHandle<CaloGeometry> geoHandle;

  PgTiming t1("analyzeEB: geomRetrieval");
  es.get<MyCaloGeometryRecord>().get(geoHandle);
  const CaloSubdetectorGeometry *geometry_p
    = (*geoHandle).getSubdetectorGeometry(DetId::Ecal, EcalBarrel);
  CaloSubdetectorGeometry const& geometry = *geometry_p;
  t1.stop();


  {
    PgTiming t("analyzeEB: unsuppressed digi loop");
    //EB unsuppressed digis:
    for(EBDigiCollection::const_iterator it = ebNoZsDigis_->begin();
        it != ebNoZsDigis_->end(); ++it){
      const EBDataFrame& frame = *it;
      int iEta0 = iEta2cIndex(static_cast<const EBDetId&>(frame.id()).ieta());
      int iPhi0 = iPhi2cIndex(static_cast<const EBDetId&>(frame.id()).iphi());
      if(iEta0<0 || iEta0>=nEbEta){
        stringstream s;
        s << "EcalSelectiveReadoutValidation: "
          << "iEta0 (= " << iEta0 << ") is out of range ("
          << "[0," << nEbEta -1 << "]\n";
        throw cms::Exception(s.str());
      }
      if(iPhi0<0 || iPhi0>=nEbPhi){
        stringstream s;
        s << "EcalSelectiveReadoutValidation: "
          << "iPhi0 (= " << iPhi0 << ") is out of range ("
          << "[0," << nEbPhi -1 << "]\n";
        throw cms::Exception(s.str());
      }

      ebEnergies[iEta0][iPhi0].noZsRecE = frame2Energy(frame);
      ebEnergies[iEta0][iPhi0].gain12 = true;
      for(int i = 0; i< frame.size(); ++i){
        const int gain12Code = 0x1;
        if(frame[i].gainId()!=gain12Code) ebEnergies[iEta0][iPhi0].gain12 =  false;
      }

      const GlobalPoint xtalPos
        = geometry.getGeometry(frame.id())->getPosition();

      ebEnergies[iEta0][iPhi0].phi = rad2deg*((double)xtalPos.phi());
      ebEnergies[iEta0][iPhi0].eta = xtalPos.eta();
    } //next non-zs digi
  }


  {
    PgTiming t("analyzeEB: simHit loop");
    //EB sim hits
    for(vector<PCaloHit>::const_iterator it = ebSimHits_->begin();
        it != ebSimHits_->end(); ++it){
      const PCaloHit& simHit = *it;
      EBDetId detId(simHit.id());
      int iEta = detId.ieta();
      int iEta0 =iEta2cIndex(iEta);
      int iPhi = detId.iphi();
      int iPhi0 = iPhi2cIndex(iPhi);
      ebEnergies[iEta0][iPhi0].simE += simHit.energy();
      ++ebEnergies[iEta0][iPhi0].simHit;
    }
  }

    bool crystalShot[nEbEta][nEbPhi];
  {
    PgTiming t("analyzeEB: suppressed digi loop init");

    for(int iEta0=0; iEta0<nEbEta; ++iEta0){
      for(int iPhi0=0; iPhi0<nEbPhi; ++iPhi0){
        crystalShot[iEta0][iPhi0] = false;
      }
    }
  }

  int nEbDigi = 0;

  {
    PgTiming t("analyzeEB: suppressed digi loop");

    for(EBDigiCollection::const_iterator it = ebDigis_->begin();
        it != ebDigis_->end(); ++it){
        ++nEbDigi;
        const EBDataFrame& frame = *it;
        int iEta = static_cast<const EBDetId&>(frame.id()).ieta();
        int iPhi = static_cast<const EBDetId&>(frame.id()).iphi();
        int iEta0 = iEta2cIndex(iEta);
        int iPhi0 = iPhi2cIndex(iPhi);
        if(iEta0<0 || iEta0>=nEbEta){
          throw (cms::Exception("EcalSelectiveReadoutValidation")
                 << "iEta0 (= " << iEta0 << ") is out of range ("
                 << "[0," << nEbEta -1 << "]");
        }
        if(iPhi0<0 || iPhi0>=nEbPhi){
          throw (cms::Exception("EcalSelectiveReadoutValidation")
                 << "iPhi0 (= " << iPhi0 << ") is out of range ("
                 << "[0," << nEbPhi -1 << "]");
        }
        assert(iEta0>=0 && iEta0<nEbEta);
        assert(iPhi0>=0 && iPhi0<nEbPhi);
        if(!crystalShot[iEta0][iPhi0]){
          crystalShot[iEta0][iPhi0] = true;
        } else{
          cout << "Error: several digi for same crystal!";
          abort();
        }
        if(localReco_){
          ebEnergies[iEta0][iPhi0].recE = frame2Energy(frame);
        }

        ebEnergies[iEta0][iPhi0].gain12 = true;
        for(int i = 0; i< frame.size(); ++i){
          const int gain12Code = 0x1;
          if(frame[i].gainId()!=gain12Code){
            ebEnergies[iEta0][iPhi0].gain12 =  false;
          }
        }

        fill(meChOcc_, xtalGraphX(frame.id()), xtalGraphY(frame.id()));
        EBSrFlagCollection::const_iterator srf
          = ebSrFlags_->find(readOutUnitOf(frame.id()));

        bool highInterest = false;

        // if(srf == ebSrFlags_->end()){
        //       throw cms::Exception("EcalSelectiveReadoutValidation")
        //	<< __FILE__ << ":" << __LINE__ << ": SR flag not found";
        //}

        if(srf != ebSrFlags_->end()){
          highInterest = ((srf->value() & ~EcalSrFlag::SRF_FORCED_MASK)
                          == EcalSrFlag::SRF_FULL);
        }


        if(highInterest){
          fill(meEbHiZsFir_, dccZsFIR(frame, firWeights_, firstFIRSample_, 0));
        } else{
          int v = dccZsFIR(frame, firWeights_, firstFIRSample_, 0);
          fill(meEbLiZsFir_, v);
          if(v < zsThr_){
            eventError = true;
            ++nEbZsErrors_;
            pair<int,int> ru = dccCh(frame.id());
            if(isRuComplete_[ru.first][ru.second-1]) ++nEbZsErrorsType1_;
            if(nEbZsErrors_ < 3){
              srApplicationErrorLog_ << event.id() << ", "
                                     << "RU " << frame.id() << ", "
                                     << "DCC " << ru.first
                                     << " Ch : " << ru.second << ": "
                                     << "LI channel under ZS threshold.\n";
            }
            if(nEbZsErrors_==3){
              srApplicationErrorLog_ << event.id() << ": "
                                     << "more ZS errors for this event...\n";
            }
          }
        }
      } //next EB digi
    }


    {
      PgTiming t("analyzeEB: rec hit loop");

      if(!localReco_){
        for(RecHitCollection::const_iterator it
              = ebRecHits_->begin();
            it != ebRecHits_->end(); ++it){
          ++nEbDigi;
          const RecHit& hit = *it;
          int iEta = static_cast<const EBDetId&>(hit.id()).ieta();
          int iPhi = static_cast<const EBDetId&>(hit.id()).iphi();
          int iEta0 = iEta2cIndex(iEta);
          int iPhi0 = iPhi2cIndex(iPhi);
          if(iEta0<0 || iEta0>=nEbEta){
            cout << "iEta0 (= " << iEta0 << ") is out of range ("
                 << "[0," << nEbEta -1 << "]\n";
          }
          if(iPhi0<0 || iPhi0>=nEbPhi){
            cout << "iPhi0 (= " << iPhi0 << ") is out of range ("
                 << "[0," << nEbPhi -1 << "]\n";
          }
          ebEnergies[iEta0][iPhi0].recE = hit.energy();
        }
      }
    }


//     {
//       PgTiming t("analyzeEB: crystal sorting");

//       //sorts crystal in increasing sim hit energy. ebEnergies[][].simE
//       //must be set beforehand:
//       sort(xtalEtaPhi.begin(), xtalEtaPhi.end(), Sorter(this));
//       cout << "\niEta\tiPhi\tsimE\tnoZsE\tzsE\n";
//     }


    {
      PgTiming t("analyzeEB: loop on energies");

        for(unsigned int i=0; i<xtalEtaPhi.size(); ++i){
          int iEta0 = xtalEtaPhi[i].first;
          int iPhi0=  xtalEtaPhi[i].second;
          energiesEb_t& energies = ebEnergies[iEta0][iPhi0];

          double recE = energies.recE;
          if(recE!=-numeric_limits<double>::max()){//not zero suppressed
            fill(meEbRecE_, ebEnergies[iEta0][iPhi0].recE);
            fill(meEbEMean_, ievt_+1, recE);
          } //not zero suppressed

          if(withEbSimHit_){
            if(!energies.simHit){//noise only crystal channel
              fill(meEbNoise_, energies.noZsRecE);
            } else{
              fill(meEbSimE_, energies.simE);
              fill(meEbRecEHitXtal_, energies.recE);
            }
            fill(meEbRecVsSimE_, energies.simE, energies.recE);
            fill(meEbNoZsRecVsSimE_, energies.simE, energies.noZsRecE);
          }
        }
    }

    {
      PgTiming t("analyzeEB: SRF");
      //SRF
      nEbFROCnt_ = 0;
      char ebSrfMark[2][17][72];
      bzero(ebSrfMark, sizeof(ebSrfMark));
      //      int idbg = 0;
      for(EBSrFlagCollection::const_iterator it = ebSrFlags_->begin();
          it != ebSrFlags_->end(); ++it){
        const EBSrFlag& srf = *it;
	int iEtaAbs = srf.id().ietaAbs();
	int iPhi = srf.id().iphi();
	int iZ = srf.id().zside();

// 	cout << "--> " << ++idbg << iEtaAbs << " " << iPhi << " "  << iZ
// 	     << " " << srf.id() << "\n";
	
	if(iEtaAbs < 1 || iEtaAbs > 17
	   || iPhi < 1 || iPhi > 72) throw cms::Exception("EcalSelectiveReadoutValidation")
	     << "Found a barrel SRF with an invalid det ID: " << srf.id() << ".\n";
	++ebSrfMark[iZ>0?1:0][iEtaAbs-1][iPhi-1];
	if(ebSrfMark[iZ>0?1:0][iEtaAbs-1][iPhi-1] > 1) throw cms::Exception("EcalSelectiveReadoutValidation")
	  << "Duplicate SRF for RU " << srf.id() << ".\n";
        int flag = srf.value() & ~EcalSrFlag::SRF_FORCED_MASK;
	if(flag == EcalSrFlag::SRF_ZS1){
	  fill(meZs1Ru_, ruGraphX(srf.id()), ruGraphY(srf.id()));
	}
        if(flag == EcalSrFlag::SRF_FULL){
	  fill(meFullRoRu_, ruGraphX(srf.id()), ruGraphY(srf.id()));
          ++nEbFROCnt_;
        }
        if(srf.value() & EcalSrFlag::SRF_FORCED_MASK){
	  fill(meForcedRu_, ruGraphX(srf.id()), ruGraphY(srf.id()));
        }
      }
    }

    {
      PgTiming t("analyzeEB: logSRerror");

      if(eventError) srApplicationErrorLog_ << event.id()
                                            << ": " << nEbZsErrors_
                                            << " ZS-flagged EB channels under "
                       "the ZS threshold, whose " << nEbZsErrorsType1_
                                            << " in a complete RU.\n";
    }
}

EcalSelectiveReadoutValidation::~EcalSelectiveReadoutValidation(){
}

void EcalSelectiveReadoutValidation::beginRun(const edm::Run& r, const edm::EventSetup& es){
  // endcap mapping
  edm::ESHandle<EcalTrigTowerConstituentsMap> hTriggerTowerMap;
  es.get<IdealGeometryRecord>().get(hTriggerTowerMap);
  triggerTowerMap_ = hTriggerTowerMap.product();

  //electronics map
  ESHandle< EcalElectronicsMapping > ecalmapping;
  es.get< EcalMappingRcd >().get(ecalmapping);
  elecMap_ = ecalmapping.product();

  initAsciiFile();
}

void EcalSelectiveReadoutValidation::endRun(const edm::Run& r, const edm::EventSetup& es){
  meL1aRate_->Fill(getL1aRate());
  if(useEventRate_) normalizeHists(ievt_);
  if(outputFile_.size()!=0) dbe_->save(outputFile_);
}

void
EcalSelectiveReadoutValidation::analyzeTP(const edm::Event& event,
					  const edm::EventSetup& es){
  EcalTPGScale ecalScale;
#if (CMSSW_COMPAT_VERSION>=210)
  ecalScale.setEventSetup(es) ;
#endif

  //  std::cout << __FILE__ << __LINE__
  //	    << "n TP: " << tps_->size() <<std::endl;

  for(EcalTrigPrimDigiCollection::const_iterator it = tps_->begin();
      it != tps_->end(); ++it){
    //    for(int i = 0; i < it->size(); ++i){
    //  double v = (*it)[i].raw() & 0xFF;
    //  if(v>0) std::cout << v << " " << i << std::endl;
    //}
    //    if(it->compressedEt() > 0){
    //  std::cout << "---------> " << it->id().ieta() << ", "
    //		<< it->id().iphi() << ", "
    //		<< it->compressedEt() << std::endl;
    //}

    //const int iTcc = elecMap_->TCCid(it->id());
    //const int iTt = elecMap_->iTt(it->id());
    double tpEt;
    if(tpInGeV_){
#if (CMSSW_COMPAT_VERSION<210)
      tpEt = ecalScale.getTPGInGeV(es, *it);
#else
      tpEt = ecalScale.getTPGInGeV(it->compressedEt(), it->id()) ;
#endif
    } else{
      tpEt = it->compressedEt();
    }
    int iEta = it->id().ieta();
    int iEta0 = iTtEta2cIndex(iEta);
    int iPhi = it->id().iphi();
    int iPhi0 = iTtEta2cIndex(iPhi);
    double etSum = ttEtSums[iEta0][iPhi0];
    fill(meTp_, tpEt);
    fill(meTpVsEtSum_, etSum, tpEt);
    fill(meTtf_, it->ttFlag());
    if((it->ttFlag() & 0x3) == 0){
      fill(meLiTtf_, iEta, iPhi);
    }
    if((it->ttFlag() & 0x3) == 1){
      fill(meMiTtf_, iEta, iPhi);
    }
    if((it->ttFlag() & 0x3) == 3){
      fill(meHiTtf_, iEta, iPhi);
    }
    if((it->ttFlag() & 0x4)){
      fill(meForcedTtf_, iEta, iPhi);
    }

    fill(meTtfVsTp_, tpEt, it->ttFlag());
    fill(meTtfVsEtSum_, etSum, it->ttFlag());
    fill(meTpMap_, iEta, iPhi, tpEt, 1.);
  }
}

void EcalSelectiveReadoutValidation::analyzeDataVolume(const Event& e,
						       const EventSetup& es){

  anaDigiInit();


  //Complete RU, i.e. RU actually fully readout
  for(int iDcc = minDccId_; iDcc <= maxDccId_; ++iDcc){
    for(int iCh = 1; iCh < nDccRus_[iDcc-minDccId_]; ++iCh){
      isRuComplete_[iDcc-minDccId_][iCh-1]
        = (nPerRu_[iDcc-minDccId_][iCh-1]==getCrystalCount(iDcc, iCh));
    }
  }


  //Barrel
  for (unsigned int digis=0; digis<ebDigis_->size(); ++digis){
    EBDataFrame ebdf = (*ebDigis_)[digis];
    anaDigi(ebdf, *ebSrFlags_);
  }

  // Endcap
  for (unsigned int digis=0; digis<eeDigis_->size(); ++digis){
    EEDataFrame eedf = (*eeDigis_)[digis];
    anaDigi(eedf, *eeSrFlags_);
  }

  //histos
  for(unsigned iDcc0 = 0; iDcc0 <  nDccs_; ++iDcc0){
    fill(meDccVol_, iDcc0+1, getDccEventSize(iDcc0, nPerDcc_[iDcc0])/kByte_);
    fill(meDccLiVol_, iDcc0+1,
         getDccSrDependentPayload(iDcc0, nLiRuPerDcc_[iDcc0],
                                  nLiPerDcc_[iDcc0])/kByte_);
    fill(meDccHiVol_, iDcc0+1,
         getDccSrDependentPayload(iDcc0, nHiRuPerDcc_[iDcc0],
                                  nHiPerDcc_[iDcc0])/kByte_);
    const FEDRawDataCollection& raw = *fedRaw_;
    fill(meDccVolFromData_, iDcc0+1,
	 ((double)raw.FEDData(601+iDcc0).size())/kByte_);
  }


  //low interesest channels:
  double a = nEbLI_*getBytesPerCrystal()/kByte_; //getEbEventSize(nEbLI_)/kByte_;
  fill(meVolBLI_, a);
  double b = nEeLI_*getBytesPerCrystal()/kByte_; //getEeEventSize(nEeLI_)/kByte_;
  fill(meVolELI_, b);
  fill(meVolLI_, a+b);

  //high interest chanels:
  a = nEbHI_*getBytesPerCrystal()/kByte_; //getEbEventSize(nEbHI_)/kByte_;
  fill(meVolBHI_, a);
  b = nEeHI_*getBytesPerCrystal()/kByte_; //getEeEventSize(nEeHI_)/kByte_;
  fill(meVolEHI_, b);
  fill(meVolHI_, a+b);

  //any-interest channels:
  a = getEbEventSize(nEb_)/kByte_;
  fill(meVolB_, a);
  b = getEeEventSize(nEe_)/kByte_;
  fill(meVolE_, b);
  fill(meVol_, a+b);
}


template<class T, class U>
void EcalSelectiveReadoutValidation::anaDigi(const T& frame,
					     const U& srFlagColl){
  const DetId& xtalId = frame.id();
  typedef typename U::key_type RuDetId;
  const RuDetId& ruId = readOutUnitOf(frame.id());
  typename U::const_iterator srf = srFlagColl.find(ruId);

  bool highInterest = false;
  int flag = 0;
  
  if(srf != srFlagColl.end()){
    //     throw cms::Exception("EcalSelectiveReadoutValidation")
    //       << __FILE__ << ":" << __LINE__ << ": SR flag not found";
    //   }
    
    flag = srf->value() & ~EcalSrFlag::SRF_FORCED_MASK;
    
    highInterest = (flag == EcalSrFlag::SRF_FULL);
    
  }

  bool barrel = (xtalId.subdetId()==EcalBarrel);

  pair<int,int> ch = dccCh(xtalId);
  
  if(barrel){
    ++nEb_;
    if(highInterest){
      ++nEbHI_;
    } else{//low interest
      ++nEbLI_;
    }
    int iEta0 = iEta2cIndex(static_cast<const EBDetId&>(xtalId).ieta());
    int iPhi0 = iPhi2cIndex(static_cast<const EBDetId&>(xtalId).iphi());
    if(!ebRuActive_[iEta0/ebTtEdge][iPhi0/ebTtEdge]){
      ++nRuPerDcc_[ch.first-minDccId_];
      if(highInterest){
	//	fill(meFullRoRu_, ruGraphX(ruId), ruGraphY(ruId));
        ++nHiRuPerDcc_[ch.first-minDccId_];
      } else{
        ++nLiRuPerDcc_[ch.first-minDccId_];
      }
//       if(flag & EcalSrFlag::SRF_FORCED_MASK){
// 	fill(meForcedRu_, ruGraphX(ruId), ruGraphY(ruId));
//       }
//       if(flag == EcalSrFlag::SRF_ZS1){
//         fill(meZs1Ru_, ruGraphX(ruId), ruGraphY(ruId));
//       }
      ebRuActive_[iEta0/ebTtEdge][iPhi0/ebTtEdge] = true;
    }
  } else{//endcap
    ++nEe_;
    if(highInterest){
      ++nEeHI_;
    } else{//low interest
      ++nEeLI_;
    }
    int iX0 = iXY2cIndex(static_cast<const EEDetId&>(frame.id()).ix());
    int iY0 = iXY2cIndex(static_cast<const EEDetId&>(frame.id()).iy());
    int iZ0 = static_cast<const EEDetId&>(frame.id()).zside()>0?1:0;

    if(!eeRuActive_[iZ0][iX0/scEdge][iY0/scEdge]){
      ++nRuPerDcc_[ch.first-minDccId_];
      if(highInterest){
	//	fill(meFullRoRu_, ruGraphX(ruId), ruGraphY(ruId));
        ++nHiRuPerDcc_[ch.first-minDccId_];
      } else{
        ++nLiRuPerDcc_[ch.first-minDccId_];
      }
//       if(flag == EcalSrFlag::SRF_ZS1){
// 	fill(meZs1Ru_, ruGraphX(ruId), ruGraphY(ruId));
//       }
//       if(srf->value() & EcalSrFlag::SRF_FORCED_MASK){
// 	fill(meForcedRu_, ruGraphX(ruId), ruGraphY(ruId));
//       }
      eeRuActive_[iZ0][iX0/scEdge][iY0/scEdge] = true;
    }
  }

  if(ch.second < 1 || ch.second > 68){
    throw cms::Exception("EcalSelectiveReadoutValidation")
      << "Error in DCC channel retrieval for crystal with detId "
      << xtalId.rawId() << "DCC channel out of allowed range [1..68]\n";
  }
  ++nPerDcc_[ch.first-minDccId_];
  ++nPerRu_[ch.first-minDccId_][ch.second-1];
  if(highInterest){
    ++nHiPerDcc_[ch.first-minDccId_];
  } else{//low interest channel
    ++nLiPerDcc_[ch.first-minDccId_];
  }
}

void EcalSelectiveReadoutValidation::anaDigiInit(){
  nEb_ = 0;
  nEe_ = 0;
  nEeLI_ = 0;
  nEeHI_ = 0;
  nEbLI_ = 0;
  nEbHI_ = 0;
  bzero(nPerDcc_, sizeof(nPerDcc_));
  bzero(nLiPerDcc_, sizeof(nLiPerDcc_));
  bzero(nHiPerDcc_, sizeof(nHiPerDcc_));
  bzero(nRuPerDcc_, sizeof(nRuPerDcc_));
  bzero(ebRuActive_, sizeof(ebRuActive_));
  bzero(eeRuActive_, sizeof(eeRuActive_));
  bzero(nPerRu_, sizeof(nPerRu_));
  bzero(nLiRuPerDcc_, sizeof(nLiRuPerDcc_));
  bzero(nHiRuPerDcc_, sizeof(nHiRuPerDcc_));
}

double EcalSelectiveReadoutValidation::frame2Energy(const EcalDataFrame& frame) const{
  static bool firstCall = true;
  if(firstCall){
    cout << "Weights:";
    for(unsigned i=0; i<weights_.size();++i){
      cout << "\t" << weights_[i];
    }
    cout << "\n";
    firstCall = false;
  }
  double adc2GeV = 0.;

  if(typeid(EBDataFrame)==typeid(frame)){//barrel APD
    adc2GeV = .035;
  } else if(typeid(EEDataFrame)==typeid(frame)){//endcap VPT
    adc2GeV = 0.06;
  } else{
    assert(false);
  }

  double acc = 0;

  const int n = min(frame.size(), (int)weights_.size());

  double gainInv[] = {12., 1., 6., 12.};

  for(int i=0; i < n; ++i){
    acc += weights_[i]*frame[i].adc()*gainInv[frame[i].gainId()]*adc2GeV;
  }
  return acc;
}

int EcalSelectiveReadoutValidation::getRuCount(int iDcc0) const{
  //   static int nEemRu[] = {34, 32, 33, 33, 32, 34, 33, 34, 33};
  //   static int nEepRu[] = {32, 33, 33, 32, 34, 33, 34, 33, 34};
  //   if(iDcc0<9){//EE-
  //     return nEemRu[iDcc0];
  //   } else if(iDcc0>=45){//EE+
  //     return nEepRu[iDcc0-45];
  //   } else{//EB
  //     return 68;
  //   }
  return nRuPerDcc_[iDcc0];
}

pair<int,int> EcalSelectiveReadoutValidation::dccCh(const DetId& detId) const{
  if(detId.det()!=DetId::Ecal){
    throw cms::Exception("InvalidParameter")
      << "Wrong type of DetId passed to the "
      "EcalSelectiveReadoutValidation::dccCh(const DetId&). "
      "An ECAL DetId was expected.\n";
  }
  
  DetId xtalId;
  switch(detId.subdetId()){
  case EcalTriggerTower:       //Trigger tower
    {
      const EcalTrigTowerDetId tt = detId;
      //pick up one crystal of the trigger tower: they are however all readout by
      //the same DCC channel in the barrel.
      //Arithmetic is easier on the "c" indices:
      const int iTtPhi0 = iTtPhi2cIndex(tt.iphi());
      const int iTtEta0 = iTtEta2cIndex(tt.ieta());
      const int oneXtalPhi0 = iTtPhi0 * 5;
      const int oneXtalEta0 = (iTtEta0 - nOneEeTtEta) * 5;

      xtalId = EBDetId(cIndex2iEta(oneXtalEta0),
		       cIndex2iPhi(oneXtalPhi0));
    }
    break;
  case EcalEndcap:
    if(detId.rawId() & 0x8000){ //Supercrystal
      return elecMap_->getDCCandSC(EcalScDetId(detId));
//       throw cms::Exception("InvalidParameter")
//	<< "Wrong type of DetId passed to the method "
//	"EcalSelectiveReadoutValidation::dccCh(const DetId&). "
//	"A valid EcalTriggerTower, EcalBarrel or EcalEndcap DetId was expected. "
//	"detid = " << xtalId.rawId() << ".\n";
    } else {                    //EE crystal
      xtalId = detId;
    }
    break;
  case EcalBarrel:              //EB crystal
    xtalId = detId;
    break;
  default:
    throw cms::Exception("InvalidParameter")
      << "Wrong type of DetId passed to the method "
      "EcalSelectiveReadoutValidation::dccCh(const DetId&). "
      "A valid EcalTriggerTower, EcalBarrel or EcalEndcap DetId was expected. "
      "detid = " << xtalId.rawId() << ".\n";
  }

  const EcalElectronicsId& EcalElecId = elecMap_->getElectronicsId(xtalId);

  pair<int,int> result;
  result.first = EcalElecId.dccId();

  if(result.first < minDccId_ || result.second > maxDccId_){
    throw cms::Exception("OutOfRange")
      << "Got an invalid DCC ID, DCCID = " << result.first
      << " for DetId 0x" << hex << detId.rawId()
      << " and 0x" << xtalId.rawId() << dec << "\n";
  }

  result.second = EcalElecId.towerId();

  if(result.second < 1 || result.second > 68){
    throw cms::Exception("OutOfRange")
      << "Got an invalid DCC channel ID, DCC_CH = " << result.second
      << " for DetId 0x" << hex << detId.rawId()
      << " and 0x"  << xtalId.rawId() << dec << "\n";
  }

  return result;
}

EcalScDetId
EcalSelectiveReadoutValidation::superCrystalOf(const EEDetId& xtalId) const
{

  const int scEdge = 5;
  EcalScDetId id = EcalScDetId((xtalId.ix()-1)/scEdge+1,
		     (xtalId.iy()-1)/scEdge+1,
		     xtalId.zside());
  return id;
  /*
  const EcalElectronicsId& EcalElecId = elecMap_->getElectronicsId(xtalId);
    int iDCC= EcalElecId.dccId();
    int iDccChan = EcalElecId.towerId();
    const vector<EcalScDetId> id = elecMap_->getEcalScDetId(iDCC, iDccChan);

    if(SkipInnerSC_)
    {
    if( (id.ix()>=9 && id.ix()<=12) && (id.iy()>=9 && id.iy()<=12) )
    return EcalScDetId();
    else
    return id;
    }
    else
    {
    if(id.ix()==9 && id.iy()==9)
    return EcalScDetId(2,5,xtalId.zside());
    else if(id.ix()==9 && id.iy()==12)
    return EcalScDetId(1,13,xtalId.zside());
    else if(id.ix()==12 && id.iy()==9)
    return EcalScDetId(19,5,xtalId.zside());
    else if(id.ix()==12 && id.iy()==12)
    return EcalScDetId(20,13,xtalId.zside());
    else
    return id;
    }
  */
}


EcalTrigTowerDetId
EcalSelectiveReadoutValidation::readOutUnitOf(const EBDetId& xtalId) const{
  return triggerTowerMap_->towerOf(xtalId);
}

EcalScDetId
EcalSelectiveReadoutValidation::readOutUnitOf(const EEDetId& xtalId) const{
  return superCrystalOf(xtalId);
}

void
EcalSelectiveReadoutValidation::setTtEtSums(const edm::EventSetup& es,
					    const EBDigiCollection& ebDigis,
					    const EEDigiCollection& eeDigis){
  //ecal geometry:
  static const CaloSubdetectorGeometry* eeGeometry = 0;
  static const CaloSubdetectorGeometry* ebGeometry = 0;
  if(eeGeometry==0 || ebGeometry==0){
    edm::ESHandle<CaloGeometry> geoHandle;
    es.get<MyCaloGeometryRecord>().get(geoHandle);
    eeGeometry
      = (*geoHandle).getSubdetectorGeometry(DetId::Ecal, EcalEndcap);
    ebGeometry
      = (*geoHandle).getSubdetectorGeometry(DetId::Ecal, EcalBarrel);
  }

  //init etSum array:
  for(int iEta0 = 0; iEta0 < nTtEta; ++iEta0){
    for(int iPhi0 = 0; iPhi0 < nTtPhi; ++iPhi0){
      ttEtSums[iEta0][iPhi0] = 0.;
    }
  }

  for(EBDigiCollection::const_iterator it = ebDigis_->begin();
      it != ebDigis_->end(); ++it){
    const EBDataFrame& frame = *it;
    const EcalTrigTowerDetId& ttId = triggerTowerMap_->towerOf(frame.id());
    //      LogDebug("TT")
    //        <<  ((EBDetId&)frame.id()).ieta()
    //        << "," << ((EBDetId&)frame.id()).iphi()
    //        << " -> " << ttId.ieta() << "," << ttId.iphi();
    const int iTtEta0 = iTtEta2cIndex(ttId.ieta());
    const int iTtPhi0 = iTtPhi2cIndex(ttId.iphi());
    double theta = ebGeometry->getGeometry(frame.id())->getPosition().theta();
    double e = frame2EnergyForTp(frame);
    if((frame2EnergyForTp(frame,-1) < e) && (frame2EnergyForTp(frame, 1) < e)){
      ttEtSums[iTtEta0][iTtPhi0] += e*sin(theta);
    }
  }

  for(EEDigiCollection::const_iterator it = eeDigis.begin();
      it != eeDigis.end(); ++it){
    const EEDataFrame& frame = *it;
    const EcalTrigTowerDetId& ttId = triggerTowerMap_->towerOf(frame.id());
    const int iTtEta0 = iTtEta2cIndex(ttId.ieta());
    const int iTtPhi0 = iTtPhi2cIndex(ttId.iphi());
    //     LogDebug("TT") << ": EE xtal->TT "
    //        <<  ((EEDetId&)frame.id()).ix()
    //        << "," << ((EEDetId&)frame.id()).iy()
    //        << " -> " << ttId.ieta() << "," << ttId.iphi() << "\n";
    double theta = eeGeometry->getGeometry(frame.id())->getPosition().theta();
    double e = frame2EnergyForTp(frame);
    if((frame2EnergyForTp(frame,-1) < e) && (frame2EnergyForTp(frame, 1) < e)){
      ttEtSums[iTtEta0][iTtPhi0] += e*sin(theta);
    }
  }

  //dealing with pseudo-TT in two inner EE eta-ring:
  int innerTTEtas[] = {0, 1, 54, 55};
  for(unsigned iRing = 0; iRing < sizeof(innerTTEtas)/sizeof(innerTTEtas[0]);
      ++iRing){
    int iTtEta0 = innerTTEtas[iRing];
    //this detector eta-section is divided in only 36 phi bins
    //For this eta regions,
    //current tower eta numbering scheme is inconsistent. For geometry
    //version 133:
    //- TT are numbered from 0 to 72 for 36 bins
    //- some TT have an even index, some an odd index
    //For geometry version 125, there are 72 phi bins.
    //The code below should handle both geometry definition.
    //If there are 72 input trigger primitives for each inner eta-ring,
    //then the average of the trigger primitive of the two pseudo-TT of
    //a pair (nEta, nEta+1) is taken as Et of both pseudo TTs.
    //If there are only 36 input TTs for each inner eta ring, then half
    //of the present primitive of a pseudo TT pair is used as Et of both
    //pseudo TTs.

    for(unsigned iTtPhi0 = 0; iTtPhi0 < nTtPhi-1; iTtPhi0 += 2){
      double et = .5*(ttEtSums[iTtEta0][iTtPhi0]
		      +ttEtSums[iTtEta0][iTtPhi0+1]);
      //divides the TT into 2 phi bins in order to match with 72 phi-bins SRP
      //scheme or average the Et on the two pseudo TTs if the TT is already
      //divided into two trigger primitives.
      ttEtSums[iTtEta0][iTtPhi0] = et;
      ttEtSums[iTtEta0][iTtPhi0+1] = et;
    }
  }
}

template<class T>
double EcalSelectiveReadoutValidation::frame2EnergyForTp(const T& frame,
							 int offset) const{
  //we have to start by 0 in order to handle offset=-1
  //(however Fenix FIR has AFAK only 5 taps)
  double weights[] = {0., -1/3., -1/3., -1/3., 0., 1.};

  double adc2GeV = 0.;
  if(typeid(frame) == typeid(EBDataFrame)){
    adc2GeV = 0.035;
  } else if(typeid(frame) == typeid(EEDataFrame)){
    adc2GeV = 0.060;
  } else{ //T is an invalid type!
    //TODO: replace message by a cms exception
    throw cms::Exception("Severe Error")
      << __FILE__ << ":" << __LINE__ << ": "
      << "this is a bug. Please report it.\n";
  }

  double acc = 0;

  const int n = min<int>(frame.size(), sizeof(weights)/sizeof(weights[0]));

  double gainInv[] = {12., 1., 6., 12};

  for(int i=offset; i < n; ++i){
    int iframe = i + offset;
    if(iframe>=0 && iframe<frame.size()){
      acc += weights[i]*frame[iframe].adc()
	*gainInv[frame[iframe].gainId()]*adc2GeV;
      //cout << (iframe>offset?"+":"")
      //     << frame[iframe].adc() << "*" << gainInv[frame[iframe].gainId()]
      //     << "*" << adc2GeV << "*(" << weights[i] << ")";
    }
  }
  //cout << "\n";
  return acc;
}

MonitorElement* EcalSelectiveReadoutValidation::bookFloat(const std::string& name){
  if(!registerHist(name, "")) return 0; //this histo is disabled
  MonitorElement* result = dbe_->bookFloat(name);
  if(result==0){
    throw cms::Exception("DQM")
      << "Failed to book integer DQM monitor element" << name;
  }
  return result;
}


MonitorElement* EcalSelectiveReadoutValidation::book1D(const std::string& name, const std::string& title, int nbins, double xmin, double xmax){
  if(!registerHist(name, title)) return 0; //this histo is disabled
  MonitorElement* result = dbe_->book1D(name, title, nbins, xmin, xmax);
  if(result==0){
    throw cms::Exception("Histo")
      << "Failed to book histogram " << name;
  }
  return result;
}

MonitorElement* EcalSelectiveReadoutValidation::book2D(const std::string& name, const std::string& title, int nxbins, double xmin, double xmax, int nybins, double ymin, double ymax){
  if(!registerHist(name, title)) return 0; //this histo is disabled
  MonitorElement* result = dbe_->book2D(name, title, nxbins, xmin, xmax,
					nybins, ymin, ymax);
  if(result==0){
    throw cms::Exception("Histo")
      << "Failed to book histogram " << name;
  }
  return result;
}

MonitorElement* EcalSelectiveReadoutValidation::bookProfile(const std::string& name, const std::string& title, int nbins, double xmin, double xmax){
  if(!registerHist(name, title)) return 0; //this histo is disabled
  MonitorElement* result = dbe_->bookProfile(name, title, nbins, xmin, xmax,
					     0, 0, 0);
  if(result==0){
    throw cms::Exception("Histo")
      << "Failed to book histogram " << name;
  }
  return result;
}

MonitorElement* EcalSelectiveReadoutValidation::bookProfile2D(const std::string& name, const std::string& title, int nbinx, double xmin, double xmax, int nbiny, double ymin, double ymax, const char* option){
  if(!registerHist(name, title)) return 0; //this histo is disabled
  MonitorElement* result
    = dbe_->bookProfile2D(name,
			  title,
			  nbinx, xmin, xmax,
			  nbiny, ymin, ymax,
			  0, 0, 0,
			  option);
  if(result==0){
    throw cms::Exception("Histo")
      << "Failed to book histogram " << name;
  }
  return result;
}

bool EcalSelectiveReadoutValidation::registerHist(const std::string& name,
						  const std::string& title){
  availableHistList_.insert(pair<string, string>(name, title));
  return allHists_ || histList_.find(name)!=histList_.end();
}

void EcalSelectiveReadoutValidation::readAllCollections(const edm::Event& event){
  ebRecHits_.read(event);
  eeRecHits_.read(event);
  ebDigis_.read(event);
  eeDigis_.read(event);
  ebNoZsDigis_.read(event);
  eeNoZsDigis_.read(event);
  ebSrFlags_.read(event);
  eeSrFlags_.read(event);
  ebComputedSrFlags_.read(event);
  eeComputedSrFlags_.read(event);
  ebSimHits_.read(event);
  eeSimHits_.read(event);
  tps_.read(event);
  fedRaw_.read(event);
}

void EcalSelectiveReadoutValidation::printAvailableHists(){
  LogInfo log("HistoList");
  log << "Avalailable histograms (DQM monitor elements): \n";
  for(map<string, string>::iterator it = availableHistList_.begin();
      it != availableHistList_.end();
      ++it){
    log << it->first << ": " << it->second << "\n";
  }
  log << "\nTo include an histogram add its name in the vstring parameter "
    "'histograms' of the EcalSelectiveReadoutValidation module\n";
}

double EcalSelectiveReadoutValidation::getEbEventSize(double nReadXtals) const{
  double ruHeaderPayload = 0.;
  const int firstEbDcc0 = nEeDccs/2;
  for(int iDcc0 = firstEbDcc0; iDcc0 < firstEbDcc0 + nEbDccs; ++iDcc0){
    ruHeaderPayload += getRuCount(iDcc0)*8.;
  }

  return getDccOverhead(EB)*nEbDccs + nReadXtals*getBytesPerCrystal()
    + ruHeaderPayload;
}

double EcalSelectiveReadoutValidation::getEeEventSize(double nReadXtals) const{
  double ruHeaderPayload = 0.;
  const unsigned firstEbDcc0 = nEeDccs/2;
  for(unsigned iDcc0 = 0; iDcc0 < nDccs_; ++iDcc0){
    //skip barrel:
    if(iDcc0== firstEbDcc0) iDcc0 += nEbDccs;
    ruHeaderPayload += getRuCount(iDcc0)*8.;
  }
  return getDccOverhead(EE)*nEeDccs + nReadXtals*getBytesPerCrystal()
    + ruHeaderPayload;
}

void EcalSelectiveReadoutValidation::normalizeHists(double eventCount){
  MonitorElement* mes[] = { meChOcc_, meTtf_, meTp_, meFullRoRu_,
			    meZs1Ru_, meForcedRu_, meLiTtf_, meHiTtf_,
                            meEbLiZsFir_, meEbHiZsFir_,
                            meEeLiZsFir_, meEeHiZsFir_, };

  double scale = 1./eventCount;

  for(unsigned i = 0; i < sizeof(mes)/sizeof(mes[0]); ++i){
    if(mes[i] == 0) continue;
    TH1* h = mes[i]->getTH1();
    if(dynamic_cast<TH2*>(h)){//TH2
      h->GetZaxis()->SetTitle("Frequency");
    } else{ //assuming TH1
      h->GetYaxis()->SetTitle("<Count>");
    }
    cout << "[SRValid] " << "Normalising " << h->GetName() << "\n";
    h->Scale(scale);
  }
}

//This implementation  assumes that int is coded on at least 28-bits,
//which in pratice should be always true.
int
EcalSelectiveReadoutValidation::dccZsFIR(const EcalDataFrame& frame,
					 const std::vector<int>& firWeights,
					 int firstFIRSample,
					 bool* saturated){
  const int nFIRTaps = 6;
  //FIR filter weights:
  const vector<int>& w = firWeights;

  //accumulator used to compute weighted sum of samples
  int acc = 0;
  bool gain12saturated = false;
  const int gain12 = 0x01;
  const int lastFIRSample = firstFIRSample + nFIRTaps - 1;
  //LogDebug("DccFir") << "DCC FIR operation: ";
  int iWeight = 0;
  for(int iSample=firstFIRSample-1;
      iSample<lastFIRSample; ++iSample, ++iWeight){
    if(iSample>=0 && iSample < frame.size()){
      EcalMGPASample sample(frame[iSample]);
      if(sample.gainId()!=gain12) gain12saturated = true;
      LogTrace("DccFir") << (iSample>=firstFIRSample?"+":"") << sample.adc()
			 << "*(" << w[iWeight] << ")";
      acc+=sample.adc()*w[iWeight];
    } else{
      edm::LogWarning("DccFir") << __FILE__ << ":" << __LINE__ <<
	": Not enough samples in data frame or 'ecalDccZs1stSample' module "
	"parameter is not valid...";
    }
  }
  LogTrace("DccFir") << "\n";
  //discards the 8 LSBs
  //(shift operator cannot be used on negative numbers because
  // the result depends on compilator implementation)
  acc = (acc>=0)?(acc >> 8):-(-acc >> 8);
  //ZS passed if weighted sum acc above ZS threshold or if
  //one sample has a lower gain than gain 12 (that is gain 12 output
  //is saturated)

  LogTrace("DccFir") << "acc: " << acc << "\n"
		     << "saturated: " << (gain12saturated?"yes":"no") << "\n";

  if(saturated){
    *saturated = gain12saturated;
  }

  return gain12saturated?numeric_limits<int>::max():acc;
}

std::vector<int>
EcalSelectiveReadoutValidation::getFIRWeights(const std::vector<double>&
					      normalizedWeights){
  const int nFIRTaps = 6;
  vector<int> firWeights(nFIRTaps, 0); //default weight: 0;
  const static int maxWeight = 0xEFF; //weights coded on 11+1 signed bits
  for(unsigned i=0; i < min((size_t)nFIRTaps,normalizedWeights.size()); ++i){
    firWeights[i] = lround(normalizedWeights[i] * (1<<10));
    if(abs(firWeights[i])>maxWeight){//overflow
      firWeights[i] = firWeights[i]<0?-maxWeight:maxWeight;
    }
  }
  return firWeights;
}

void
EcalSelectiveReadoutValidation::configFirWeights(vector<double> weightsForZsFIR){
  bool notNormalized  = false;
  bool notInt = false;
  for(unsigned i=0; i < weightsForZsFIR.size(); ++i){
    if(weightsForZsFIR[i] > 1.) notNormalized = true;
    if((int)weightsForZsFIR[i]!=weightsForZsFIR[i]) notInt = true;
  }
  if(notInt && notNormalized){
    throw cms::Exception("InvalidParameter")
      << "weigtsForZsFIR paramater values are not valid: they "
      << "must either be integer and uses the hardware representation "
      << "of the weights or less or equal than 1 and used the normalized "
      << "representation.";
  }
  LogInfo log("DccFir");
  if(notNormalized){
    firWeights_ = vector<int>(weightsForZsFIR.size());
    for(unsigned i = 0; i< weightsForZsFIR.size(); ++i){
      firWeights_[i] = (int)weightsForZsFIR[i];
    }
  } else{
    firWeights_ = getFIRWeights(weightsForZsFIR);
  }

  log << "Input weights for FIR: ";
  for(unsigned i = 0; i < weightsForZsFIR.size(); ++i){
    log << weightsForZsFIR[i] << "\t";
  }

  double s2 = 0.;
  log << "\nActual FIR weights: ";
  for(unsigned i = 0; i < firWeights_.size(); ++i){
    log << firWeights_[i] << "\t";
    s2 += firWeights_[i]*firWeights_[i];
  }

  s2 = sqrt(s2);
  log << "\nNormalized FIR weights after hw representation rounding: ";
  for(unsigned i = 0; i < firWeights_.size(); ++i){
    log << firWeights_[i] / (double)(1<<10) << "\t";
  }

  log <<"\nFirst FIR sample: " << firstFIRSample_;
}

void EcalSelectiveReadoutValidation::initAsciiFile(){
  if(logSrpAlgoErrors_){
    srpAlgoErrorLog_.open(srpAlgoErrorLogFileName_.c_str(), ios::out | ios::trunc);
    if(!srpAlgoErrorLog_.good()){
      throw cms::Exception("Output")
	<< "Failed to open the log file '"
	<< srpAlgoErrorLogFileName_
	<< "' for SRP algorithm result check.\n";
    }
  }

  if(logSrApplicationErrors_){
    srApplicationErrorLog_.open(srApplicationErrorLogFileName_.c_str(), ios::out | ios::trunc);
    if(!srApplicationErrorLog_.good()){
      throw cms::Exception("Output")
	<< "Failed to open the log file '"
	<< srApplicationErrorLogFileName_
	<< "' for Selective Readout decision application check.\n";
    }
  }
}

//Compares two SR flag sorted collections . Both collections
//are sorted by their key (the detid) and following algorithm is based on
//this feature.
template<class T> //T must be either an EBSrFlagCollection or an EESrFlagCollection
void EcalSelectiveReadoutValidation::compareSrfColl(const edm::Event& event, T& srfFromData, T& computedSrf){
  typedef class T::const_iterator SrFlagCollectionConstIt;
  typedef class T::key_type MyRuDetIdType;
  SrFlagCollectionConstIt itSrfFromData = srfFromData.begin();
  SrFlagCollectionConstIt itComputedSr = computedSrf.begin();

  {
    PgTiming t("collection comparison");
    //cout << __FILE__ << ":" << __LINE__ << ": "
    //	 <<  srfFromData.size() << " " << computedSrf.size() << "\n";
    //    int i = 0;
    while(itSrfFromData != srfFromData.end()
	  || itComputedSr != computedSrf.end()){
      //      cout << ++i << "\n";
      MyRuDetIdType inconsistentRu = 0;
      bool inconsistent = false;
      if(itComputedSr == computedSrf.end() ||
	 (itSrfFromData != srfFromData.end()
	  && itSrfFromData->id() < itComputedSr->id())){
	//computedSrf is missig a detid found in srfFromData
        pair<int, int> ch = dccCh(itSrfFromData->id());
	srpAlgoErrorLog_ << event.id() << ": " << itSrfFromData->id()
                         << ", DCC " << ch.first << " ch " << ch.second
			 << " found in data (SRF:" << itSrfFromData->flagName()
			 << ") but not in the set of SRFs computed from the data TTF.\n";
	inconsistentRu = itSrfFromData->id();
        inconsistent = true;
	++itSrfFromData;
      } else if(itSrfFromData==srfFromData.end() ||
		(itComputedSr != computedSrf.end()
                 && itComputedSr->id() < itSrfFromData->id())){
	//ebSrFlags is missing a detid found in computedSrf
        pair<int, int> ch = dccCh(itComputedSr->id());
	if(logErrForDccs_[ch.first-minDccId_]){
	  srpAlgoErrorLog_ << event.id() << ": " << itComputedSr->id()
                           << ", DCC " << ch.first << " ch " << ch.second
			   << " not found in data. Computed SRF: "
			   << itComputedSr->flagName() << ".\n";
          inconsistentRu = itComputedSr->id();
          inconsistent = true;
	}
        ++itComputedSr;
      } else{
	//*itSrfFromData and *itComputedSr has same detid
	if(itComputedSr->value()!=itSrfFromData->value()){
	  //if(!(itSrfFromData->value & EcalSrFlag::SRF_FORCED_MASK)){
	  pair<int, int> ch = dccCh(itSrfFromData->id());
	  srpAlgoErrorLog_ << event.id() << ", "
			   << itSrfFromData->id()
			   << ", DCC " << ch.first << " ch " << ch.second
			   << ", SRF inconsistency: "
			   << "from data: " << itSrfFromData->flagName()
			   << ", computed from TTF: "
                           << itComputedSr->flagName()
			   << "\n";
	  //}
	  inconsistentRu = itComputedSr->id();
          inconsistent = true;
	}
	if(itComputedSr != computedSrf.end()) ++itComputedSr;
	if(itSrfFromData != srfFromData.end()) ++itSrfFromData;
      }

      if(inconsistent) fill(meSRFlagsConsistency_, ruGraphX(inconsistentRu),
                            ruGraphY(inconsistentRu));
    }
  }
}


int EcalSelectiveReadoutValidation::dccId(const EcalScDetId& detId) const{
  return elecMap_->getDCCandSC(detId).first;
}

int EcalSelectiveReadoutValidation::dccId(const EcalTrigTowerDetId& detId) const{
  if(detId.ietaAbs()>17){
    throw cms::Exception("InvalidArgument")
      << "Argument of EcalSelectiveReadoutValidation::dccId(const EcalTrigTowerDetId&) "
       << "must be a barrel trigger tower Id\n";
  }
  return dccCh(detId).first;

  //   int iDccPhi0 = (detId.iphi()-1)/4; //4 TT along phi covered by a DCC
  //   int iDccEta0 = detId.zside()<0?0:1;
  //   const int nDccsInPhi = 18;
  //   return 1 + iDccEta0 * nDccsInPhi + iDccPhi0;
}



void EcalSelectiveReadoutValidation::selectFedsForLog(){
  logErrForDccs_ = vector<bool>(nDccs_, false);

  for(EBSrFlagCollection::const_iterator it = ebSrFlags_->begin();
      it != ebSrFlags_->end();
      ++it){

    //cout << __FILE__ << ":" << __LINE__ << ": "
    //	 <<  EcalTrigTowerDetId(it->id()) << "\n";

    int iDcc = dccId(it->id()) - minDccId_;
    //    cout << __FILE__ << ":" << __LINE__ << ": "
    //	 <<  it->id().rawId() << "-> DCC " << (iDcc+1) << "\n";
    logErrForDccs_.at(iDcc) = true;
  }

  for(EESrFlagCollection::const_iterator it = eeSrFlags_->begin();
      it != eeSrFlags_->end();
      ++it){
    int iDcc = dccId(it->id()) - minDccId_;
//     cout << __FILE__ << ":" << __LINE__ << ": "
//	 <<  it->id().rawId() << "-> DCC " << (iDcc+1) << "\n";
    logErrForDccs_.at(iDcc) = true;
  }

  stringstream buf;
  buf << "List of DCCs found in the first processed event: ";
  bool first = true;
  for(unsigned iDcc = 0; iDcc < nDccs_; ++iDcc){
    if(logErrForDccs_[iDcc]){
      buf << (first?"":", ") << (iDcc + minDccId_);
      first = false;
    }
  }
  buf <<  "\nOnly DCCs from this list will be considered for error logging\n";
  srpAlgoErrorLog_ << buf.str();
  srApplicationErrorLog_<<  buf.str();
  cout << buf;
}


template<class T>
void EcalSelectiveReadoutValidation::checkSrApplication(const edm::Event& event,
                                                        T& srfs){
  typedef class T::const_iterator SrFlagCollectionConstIt;
  typedef class T::key_type MyRuDetIdType;

  for(SrFlagCollectionConstIt itSrf = srfs.begin();
      itSrf != srfs.end(); ++itSrf){
    int flag = itSrf->value() & ~EcalSrFlag::SRF_FORCED_MASK;
    pair<int,int> ru = dccCh(itSrf->id());

    if(flag == EcalSrFlag::SRF_FULL){
      if(nPerRu_[ru.first-minDccId_][ru.second-1]==getCrystalCount(ru.first, ru.second)){ //no error
	fill(meIncompleteFRORateMap_, ruGraphX(itSrf->id()),
             ruGraphY(itSrf->id()), 0);
	fill(meDroppedFRORateMap_,
             ruGraphX(itSrf->id()), ruGraphY(itSrf->id()), 0);
      } else if(nPerRu_[ru.first-minDccId_][ru.second-1]==0) {//tower dropped!
	fill(meIncompleteFRORateMap_,
             ruGraphX(itSrf->id()), ruGraphY(itSrf->id()), 0);
	fill(meDroppedFRORateMap_,
             ruGraphX(itSrf->id()), ruGraphY(itSrf->id()), 1);
	fill(meDroppedFROMap_, ruGraphX(itSrf->id()), ruGraphY(itSrf->id()), 1);
        ++nDroppedFRO_;
	srApplicationErrorLog_ << event.id() << ": Flag of RU "
			       << itSrf->id() << " (DCC " << ru.first
                               << " ch " << ru.second << ") is 'Full readout' "
			       << "while none of its channel was read out\n";
      } else{ //tower partially read out
	fill(meIncompleteFRORateMap_,
             ruGraphX(itSrf->id()), ruGraphY(itSrf->id()), 1);
	fill(meDroppedFRORateMap_,
             ruGraphX(itSrf->id()), ruGraphY(itSrf->id()), 0);
	fill(meIncompleteFROMap_,
             ruGraphX(itSrf->id()), ruGraphY(itSrf->id()), 1);
        ++nIncompleteFRO_;
	srApplicationErrorLog_ << event.id() << ": Flag of RU"
			       << itSrf->id() << " (DCC " << ru.first
                               << " ch " << ru.second << ") is 'Full readout' "
			       << "while only "
                               << nPerRu_[ru.first-minDccId_][ru.second-1]
			       << " / " << getCrystalCount(ru.first, ru.second)
                               << " channels were read out.\n";
      }
    }

    if(flag == EcalSrFlag::SRF_ZS1 || flag == EcalSrFlag::SRF_ZS2){
      if(nPerRu_[ru.first-minDccId_][ru.second-1]
         ==getCrystalCount(ru.first, ru.second)){
	//ZS readout unit whose every channel was read

        fill(meCompleteZSMap_, ruGraphX(itSrf->id()), ruGraphY(itSrf->id()));
	fill(meCompleteZSRateMap_,
             ruGraphX(itSrf->id()), ruGraphY(itSrf->id()), 1);
	//srApplicationErrorLog_ << event.id() << ": "
	//                       << "All " << nMaxXtalPerRu << " channels of RU "
	//                       << itSrf->id() << " passed the Zero suppression.";
        ++nCompleteZS_;
      } else{
	fill(meCompleteZSRateMap_,
             ruGraphX(itSrf->id()), ruGraphY(itSrf->id()), 0);
      }
    }
  }
}

int EcalSelectiveReadoutValidation::getCrystalCount(int iDcc, int iDccCh){
  if(iDcc < minDccId_ || iDcc > maxDccId_){ //invalid DCC
    return 0;
  } else {
    int iDccPhi;
    if(iDcc < 10) iDccPhi = iDcc;
    else if(iDcc < 28) iDccPhi = iDcc - 9;
    else if(iDcc < 46) iDccPhi = iDcc - 27;
    else iDccPhi = iDcc - 45;
    switch(iDccPhi*100+iDccCh){
    case 110:
    case 232:
    case 312:
    case 412:
    case 532:
    case 610:
    case 830:
    case 806:
      //inner partials at 12, 3, and 9 o'clock
      return 20;
    case 134:
    case 634:
    case 827:
    case 803:
      return 10;
    case 330:
    case 430:
      return 20;
    case 203:
    case 503:
    case 721:
    case 921:
      return 21;
    default:
      return 25;
    }
  }
}
