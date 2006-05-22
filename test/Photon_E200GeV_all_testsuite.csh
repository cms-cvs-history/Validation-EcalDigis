#!/bin/csh

# Script to run the standard Ecal Digis Validation 
# It assumes the Sim Hits input files and the Digis 
# reference plots are available
#
# F. Cossutti - 03-Apr-2006 13:21

eval `scramv1 ru -csh`

setenv WORKDIR `pwd`

setenv ECALREFDIR  /afs/cern.ch/cms/data/CMSSW/Validation/EcalDigis/data

echo "===================> Step1: executing EDProducer (SimCalorimetry/EcalSimProducers) for Photon_E200GeV_all"

/bin/rm ${WORKDIR}/Photon_E200GeV_all_testsuite1_.cfg >& /dev/null

sed 's/simevent.root/Photon_E200GeV_all_simevent.root/' ${CMSSW_RELEASE_BASE}/src/SimCalorimetry/EcalSimProducers/test/EcalSimProducer.cfg >&! ${WORKDIR}/Photon_E200GeV_all_testsuite1.cfg

ln -sf ${ECALREFDIR}/Photon_E200GeV_all_simevent.root ${WORKDIR}/Photon_E200GeV_all_simevent.root

cmsRun --parameter-set ${WORKDIR}/Photon_E200GeV_all_testsuite1.cfg

/bin/rm ${WORKDIR}/Photon_E200GeV_all_simevent.root

mv digis.root Photon_E200GeV_all_digis.root

echo "===================> Step2: executing EDAnalyser (Validation/EcalDigis) for Photon_E200GeV_all"

/bin/rm ${WORKDIR}/Photon_E200GeV_all_testsuite2.cfg >& /dev/null

sed 's/digis.root/Photon_E200GeV_all_digis.root/' ${CMSSW_RELEASE_BASE}/src/Validation/EcalDigis/test/EcalDigisAnalysis.cfg >&! ${WORKDIR}/Photon_E200GeV_all_testsuite2.cfg

cmsRun --parameter-set ${WORKDIR}/Photon_E200GeV_all_testsuite2.cfg

mv EcalDigisValidation.root Photon_E200GeV_all_EcalDigisValidation_new.root
    
echo "===================> Step3: executing ROOT macro for Photon_E200GeV_all"

/bin/rm ${WORKDIR}/Photon_E200GeV_all_testsuite3.C >& /dev/null

sed 's/EcalDigisValidation_old.root/Photon_E200GeV_all_EcalDigisValidation_old.root/' ${CMSSW_RELEASE_BASE}/src/Validation/EcalDigis/test/EcalDigisPlotCompare.C | sed 's/EcalDigisValidation_new.root/Photon_E200GeV_all_EcalDigisValidation_new.root/' | sed 's/EcalDigisPlotCompare/Photon_E200GeV_all_testsuite3/' >&! ${WORKDIR}/Photon_E200GeV_all_testsuite3.C

cp ${CMSSW_RELEASE_BASE}/src/Validation/EcalDigis/test/HistoCompare.C ${WORKDIR}

cp ${CMSSW_RELEASE_BASE}/src/Validation/EcalDigis/data/Photon_E200GeV_all_EcalDigisValidation.root ${WORKDIR}/Photon_E200GeV_all_EcalDigisValidation_old.root
cp Photon_E200GeV_all_EcalDigisValidation_new.root ${WORKDIR}

cd ${WORKDIR} ; root -b -q ${WORKDIR}/Photon_E200GeV_all_testsuite3.C
