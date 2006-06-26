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
#setenv ECALREFDIR `pwd`

echo "===================> Step1: executing EDProducer (SimCalorimetry/EcalSimProducers) for Photon_E30GeV_endcap"

/bin/rm ${WORKDIR}/Photon_E30GeV_endcap_testsuite1_.cfg >& /dev/null

#sed 's/simevent.root/PhotonSimHit_E30GeV_endcap.root/' ${SWSOURCE}/src/SimCalorimetry/EcalSimProducers/test/EcalSimProducer.cfg >&! ${WORKDIR}/Photon_E30GeV_endcap_testsuite1.cfg
sed 's/simevent.root/PhotonSimHit_E30GeV_endcap.root/' ${SWSOURCE}/src/Validation/EcalDigis/test/EcalSimProducer.cfg >&! ${WORKDIR}/Photon_E30GeV_endcap_testsuite1.cfg

ln -sf ${ECALREFDIR}/PhotonSimHit_E30GeV_endcap.root ${WORKDIR}/PhotonSimHit_E30GeV_endcap.root

cmsRun --parameter-set ${WORKDIR}/Photon_E30GeV_endcap_testsuite1.cfg

/bin/rm ${WORKDIR}/PhotonSimHit_E30GeV_endcap.root

mv digis.root Photon_E30GeV_endcap_digis.root
mv digis001.root Photon_E30GeV_endcap_digis001.root

echo "===================> Step2: executing EDAnalyser (Validation/EcalDigis) for Photon_E30GeV_endcap"

/bin/rm ${WORKDIR}/Photon_E30GeV_endcap_testsuite2.cfg >& /dev/null

sed s/digis.root/Photon_E30GeV_endcap_digis.root\',\'file:Photon_E30GeV_endcap_digis001.root/ ${SWSOURCE}/src/Validation/EcalDigis/test/EcalDigisAnalysis.cfg >&! ${WORKDIR}/Photon_E30GeV_endcap_testsuite2.cfg

cmsRun --parameter-set ${WORKDIR}/Photon_E30GeV_endcap_testsuite2.cfg

mv EcalDigisValidation.root Photon_E30GeV_endcap_EcalDigisValidation_new.root
    
echo "===================> Step3: executing ROOT macro for Photon_E30GeV_endcap"

/bin/rm ${WORKDIR}/Photon_E30GeV_endcap_testsuite3.C >& /dev/null

sed 's/EcalDigisValidation_old.root/Photon_E30GeV_endcap_EcalDigisValidation_old.root/' ${SWSOURCE}/src/Validation/EcalDigis/test/EcalDigisPlotCompare.C | sed 's/EcalDigisValidation_new.root/Photon_E30GeV_endcap_EcalDigisValidation_new.root/' | sed 's/EcalDigisPlotCompare/Photon_E30GeV_endcap_testsuite3/' >&! ${WORKDIR}/Photon_E30GeV_endcap_testsuite3.C

cp ${SWSOURCE}/src/Validation/EcalDigis/test/HistoCompare.C ${WORKDIR}

cp ${SWSOURCE}/src/Validation/EcalDigis/data/Photon_E30GeV_endcap_EcalDigisValidation.root ${WORKDIR}/Photon_E30GeV_endcap_EcalDigisValidation_old.root
cp Photon_E30GeV_endcap_EcalDigisValidation_new.root ${WORKDIR}

cd ${WORKDIR} ; root -b -q ${WORKDIR}/Photon_E30GeV_endcap_testsuite3.C

