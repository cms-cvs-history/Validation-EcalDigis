#!/bin/csh

# Script to run the standard Ecal Digis Validation 
# It assumes the Sim Hits input files and the Digis 
# reference plots are available
#
# F. Cossutti - 03-Apr-2006 13:21

eval `scramv1 ru -csh`

setenv WORKDIR `pwd`

setenv SWSOURCE $CMSSW_RELEASE_BASE
#setenv SWSOURCE $CMSSW_BASE

setenv ECALREFDIR  /afs/cern.ch/cms/data/CMSSW/Validation/EcalDigis/data
#setenv ECALREFDIR  `pwd`

echo "===================> Step1: executing EDProducer (SimCalorimetry/EcalSimProducers) for Pion_Pt60GeV_all"

/bin/rm ${WORKDIR}/Pion_Pt60GeV_all_testsuite1.cfg >& /dev/null

#sed 's/simevent.root/Pion_Pt60GeV_all_simevent.root/' ${SWSOURCE}/src/SimCalorimetry/EcalSimProducers/test/EcalSimProducer.cfg >&! ${WORKDIR}/Pion_Pt60GeV_all_testsuite1.cfg
#sed 's/simevent.root/Pion_Pt60GeV_all_simevent.root/' ${SWSOURCE}/src/Validation/EcalDigis/test/EcalSimProducer.cfg >&! ${WORKDIR}/Pion_Pt60GeV_all_testsuite1.cfg
sed 's/reco-application-ecal-simulation.root/Pion_Pt60GeV_all_simevent.root/' ${CMSSW_BASE}/src/Configuration/Applications/data/reco-application-ecal-digitization.cfg >&! ${WORKDIR}/Pion_Pt60GeV_all_testsuite1.cfg

ln -sf ${ECALREFDIR}/Pion_Pt60GeV_all_simevent.root ${WORKDIR}/Pion_Pt60GeV_all_simevent.root

cmsRun --parameter-set ${WORKDIR}/Pion_Pt60GeV_all_testsuite1.cfg

/bin/rm ${WORKDIR}/Pion_Pt60GeV_all_simevent.root

mv reco-application-ecal-digitization.root Pion_Pt60GeV_all_digis.root

echo "===================> Step2: executing EDAnalyser (Validation/EcalDigis) for Pion_Pt60GeV_all"

/bin/rm ${WORKDIR}/Pion_Pt60GeV_all_testsuite2.cfg >& /dev/null

#sed s/digis.root/Pion_Pt60GeV_all_digis.root\',\'file:Pion_Pt60GeV_all_digis001.root/ ${SWSOURCE}/src/Validation/EcalDigis/test/EcalDigisAnalysis.cfg >&! ${WORKDIR}/Pion_Pt60GeV_all_testsuite2.cfg
sed s/digis.root/Pion_Pt60GeV_all_digis.root/ ${SWSOURCE}/src/Validation/EcalDigis/test/EcalDigisAnalysis.cfg >&! ${WORKDIR}/Pion_Pt60GeV_all_testsuite2.cfg

cmsRun --parameter-set ${WORKDIR}/Pion_Pt60GeV_all_testsuite2.cfg

mv EcalDigisValidation.root Pion_Pt60GeV_all_EcalDigisValidation_new.root
    
echo "===================> Step3: executing ROOT macro for Pion_Pt60GeV_all"

/bin/rm ${WORKDIR}/Pion_Pt60GeV_all_testsuite3.C >& /dev/null

sed 's/EcalDigisValidation_old.root/Pion_Pt60GeV_all_EcalDigisValidation_old.root/' ${SWSOURCE}/src/Validation/EcalDigis/test/EcalDigisPlotCompare.C | sed 's/EcalDigisValidation_new.root/Pion_Pt60GeV_all_EcalDigisValidation_new.root/' | sed 's/EcalDigisPlotCompare/Pion_Pt60GeV_all_testsuite3/' >&! ${WORKDIR}/Pion_Pt60GeV_all_testsuite3.C

cp ${SWSOURCE}/src/Validation/EcalDigis/test/HistoCompare.C ${WORKDIR}

cp ${SWSOURCE}/src/Validation/EcalDigis/data/Pion_Pt60GeV_all_EcalDigisValidation.root ${WORKDIR}/Pion_Pt60GeV_all_EcalDigisValidation_old.root
cp Pion_Pt60GeV_all_EcalDigisValidation_new.root ${WORKDIR}

cd ${WORKDIR} ; root -b -q ${WORKDIR}/Pion_Pt60GeV_all_testsuite3.C

