import FWCore.ParameterSet.Config as cms

from Validation.EcalDigis.ecalMixingModuleValidation_cfi import *
ecalMixingModuleValidation.EBdigiCollection = cms.InputTag('mix', 'simEcalUnsuppressedDigis')
ecalMixingModuleValidation.EEdigiCollection = cms.InputTag('mix', 'simEcalUnsuppressedDigis')
ecalMixingModuleValidation.ESdigiCollection = cms.InputTag('mix', 'simEcalUnsuppressedDigis')

