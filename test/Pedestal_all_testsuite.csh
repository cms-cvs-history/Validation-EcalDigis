#!/bin/csh

# Script to run the ECAL pedestal and noise validation
# the Sim Hits are produced on the fly (empty events, neutrino gun)
#
# F. Cossutti - 26-Jul-2006 17:21

eval `scramv1 ru -csh`

setenv WORKDIR `pwd`

setenv SWSOURCE $CMSSW_RELEASE_BASE
#setenv SWSOURCE $CMSSW_BASE

setenv ECALREFDIR  /afs/cern.ch/cms/data/CMSSW/Validation/EcalDigis/data
#setenv ECALREFDIR  `pwd`

echo "===================> Step1: executing EDProducer (SimCalorimetry/EcalSimProducers) for Pedestal_all"

/bin/rm ${WORKDIR}/Pedestal_all_testsuite1.cfg >& /dev/null

cp ${SWSOURCE}/src/Validation/EcalDigis/test/PedestalRunProducer.cfg ${WORKDIR}/Pedestal_all_testsuite1.cfg

cmsRun --parameter-set ${WORKDIR}/Pedestal_all_testsuite1.cfg

mv digis.root Pedestal_all_digis.root

echo "===================> Step2: executing EDAnalyser (Validation/EcalDigis) for Pedestal_all"

/bin/rm ${WORKDIR}/Pedestal_all_testsuite2.cfg >& /dev/null

sed s/digis.root/Pedestal_all_digis.root/ ${SWSOURCE}/src/Validation/EcalDigis/test/EcalDigisAnalysis.cfg >&! ${WORKDIR}/Pedestal_all_testsuite2.cfg

cmsRun --parameter-set ${WORKDIR}/Pedestal_all_testsuite2.cfg

mv EcalDigisValidation.root Pedestal_all_EcalDigisValidation_new.root
    
echo "===================> Step3: executing ROOT macro for Pedestal_all"

/bin/rm ${WORKDIR}/Pedestal_all_testsuite3.C >& /dev/null

sed 's/EcalDigisValidation_old.root/Pedestal_all_EcalDigisValidation_old.root/' ${SWSOURCE}/src/Validation/EcalDigis/test/EcalDigisPlotCompare.C | sed 's/EcalDigisValidation_new.root/Pedestal_all_EcalDigisValidation_new.root/' | sed 's/EcalDigisPlotCompare/Pedestal_all_testsuite3/' >&! ${WORKDIR}/Pedestal_all_testsuite3.C

cp ${SWSOURCE}/src/Validation/EcalDigis/test/HistoCompare.C ${WORKDIR}

cp ${SWSOURCE}/src/Validation/EcalDigis/data/Pedestal_all_EcalDigisValidation.root ${WORKDIR}/Pedestal_all_EcalDigisValidation_old.root
cp Pedestal_all_EcalDigisValidation_new.root ${WORKDIR}

cd ${WORKDIR} ; root -b -q ${WORKDIR}/Pedestal_all_testsuite3.C
