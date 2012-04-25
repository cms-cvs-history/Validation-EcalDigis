import FWCore.ParameterSet.Config as cms

# Run the complete ECAL digis validation set
from Validation.EcalDigis.ecalDigisValidation_cfi import *
from Validation.EcalDigis.ecalBarrelDigisValidation_cfi import *
from Validation.EcalDigis.ecalEndcapDigisValidation_cfi import *
from Validation.EcalDigis.ecalPreshowerDigisValidation_cfi import *
ecalUnsuppressedDigisValidationSequence = cms.Sequence(ecalDigisValidation*ecalBarrelDigisValidation*ecalEndcapDigisValidation*ecalPreshowerDigisValidation)
ecalDigisValidation.EBdigiCollection = cms.InputTag('mix', 'simEcalUnsuppressedDigis')
ecalDigisValidation.EEdigiCollection = cms.InputTag('mix', 'simEcalUnsuppressedDigis')
ecalDigisValidation.ESdigiCollection = cms.InputTag('mix', 'simEcalUnsuppressedDigis')
ecalBarrelDigisValidation.EBdigiCollection = cms.InputTag('mix', 'simEcalUnsuppressedDigis')
ecalEndcapDigisValidation.EEdigiCollection = cms.InputTag('mix', 'simEcalUnsuppressedDigis')
ecalPreshowerDigisValidation.ESdigiCollection = cms.InputTag('mix', 'simEcalUnsuppressedDigis')

