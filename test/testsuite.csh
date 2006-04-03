#!/bin/csh

# Script to run the standard Ecal Digis Validation 
# It assumes the Sim Hits input files and the Digis 
# reference plots are available
#
# F. Cossutti - 03-Apr-2006 13:21

eval `scramv1 ru -csh`

foreach i ( simevent )

    echo "===================> Step1: executing EDProducer (SimCalorimetry/EcalSimProducers) for ${i}"

    /bin/rm /tmp/testsuite1_${i}.cfg >& /dev/null

    sed 's/simevent.root/simevent_'${i}'.root/' $CMSSW_BASE/src/SimCalorimetry/EcalSimProducers/test/EcalSimProducer.cfg >&! /tmp/testsuite1_${i}.cfg

    cmsRun --parameter-set /tmp/testsuite1_${i}.cfg

    mv digis.root digis_${i}.root

    echo "===================> Step2: executing EDAnalyser (Validation/EcalDigis) for ${i}"

    /bin/rm /tmp/testsuite2_${i}.cfg >& /dev/null

    sed 's/digis.root/digis_'${i}'.root/' $CMSSW_BASE/src/Validation/EcalDigis/test/EcalDigisAnalysis.cfg >&! /tmp/testsuite2_${i}.cfg

    cmsRun --parameter-set /tmp/testsuite2_${i}.cfg

    mv EcalDigisValidation.root EcalDigisValidation_${i}.root
    
    echo "===================> Step3: executing ROOT macro for ${i}"

    /bin/rm /tmp/testsuite3_${i}.C >& /dev/null

    sed 's/EcalDigisValidation_old.root/EcalDigisValidation_old_'${i}'.root/' $CMSSW_BASE/src/Validation/EcalDigis/test/EcalDigisPlotCompare.C | sed 's/EcalDigisValidation_new.root/EcalDigisValidation_new_'${i}'.root/' | sed 's/EcalDigisPlotCompare/testsuite3_'${i}'/' >&! /tmp/testsuite3_${i}.C

    cp HistoCompare.C /tmp

    cp EcalDigisValidation_old_${i}.root /tmp
    cp EcalDigisValidation_new_${i}.root /tmp

    root -b -q /tmp/testsuite3_${i}.C

end
