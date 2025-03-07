/*****************************************************************************
 *
 * Spinstrings.c -- definitions of string values
 *
 * Copyright 2018,2021
 *   James Fidell (james@openastroproject.org)
 *
 * License:
 *
 * This file is part of the Open Astro Project.
 *
 * The Open Astro Project is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The Open Astro Project is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Open Astro Project.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include <oa_common.h>

const char* nodeTypes[] =
{
	"Value",
	"Base",
	"Integer",
	"Boolean",
	"Float",
	"Command",
	"String",
	"Register",
	"Enumeration",
	"Enumeration Entry",
	"Category",
	"Port"
};

const char*	spinFeatureStrings[250] =
{
	"AcquisitionFrameCount",
	"AcquisitionFrameRate",
	"AcquisitionFrameRateAuto",
	"AcquisitionFrameRateEnabled",
	"AcquisitionMode",
	"AcquisitionStart",
	"AcquisitionStatus",
	"AcquisitionStatusSelector",
	"AcquisitionStop",
	"ActivePageNumber",
	"ActivePageOffset",
	"ActivePageSave",
	"ActivePageValue",
	"AutoExposureTimeLowerLimit",
	"AutoExposureTimeUpperLimit",
	"AutoFunctionAOIHeight",
	"AutoFunctionAOIOffsetX",
	"AutoFunctionAOIOffsetY",
	"AutoFunctionAOIsControl",
	"AutoFunctionAOIWidth",
	"AutoGainLowerLimit",
	"AutoGainUpperLimit",
	"BalanceRatio",
	"BalanceRatioSelector",
	"BalanceWhiteAuto",
	"BinningControl",
	"BinningHorizontal",
	"BinningVertical",
	"BlackLevel",
	"BlackLevelAuto",
	"BlackLevelClampingEnable",
	"BlackLevelEnabled",
	"ChunkBlackLevel",
	"ChunkCRC",
	"ChunkEnable",
	"ChunkExposureTime",
	"ChunkFrameCounter",
	"ChunkGain",
	"ChunkHeight",
	"ChunkImage",
	"ChunkModeActive",
	"ChunkOffsetX",
	"ChunkOffsetY",
	"ChunkPixelDynamicRangeMax",
	"ChunkPixelDynamicRangeMin",
	"ChunkPixelFormat",
	"ChunkSelector",
	"ChunkTimestamp",
	"ChunkTransmitFrameCount",
	"ChunkWidth",
	"DataFlashPageCount",
	"DataFlashPageSize",
	"DecimationHorizontal",
	"DecimationVertical",
	"DeviceFamilyName",
	"DeviceFirmwareVersion",
	"DeviceGenCpVersionMajor",
	"DeviceGenCPVersionMinor",
	"DeviceID",
	"DeviceIndicatorMode",
	"DeviceLinkThroughputLimit",
	"DeviceMaxThroughput",
	"DeviceModelName",
	"DeviceReset",
	"DeviceScanType",
	"DeviceSerialNumber",
	"DeviceSVNVersion",
	"DeviceTemperature",
	"DeviceUserID",
	"DeviceVendorName",
	"DeviceVersion",
	"EventAcquisitionEnd",
	"EventAcquisitionEndData",
	"EventAcquisitionStart",
	"EventAcquisitionStartData",
	"EventExposureEnd",
	"EventExposureEndData",
	"EventExposureStart",
	"EventNotification",
	"EventSelector",
	"EventTestData",
	"ExposureActiveMode",
	"ExposureAuto",
	"ExposureMode",
	"ExposureTime",
	"ExposureTimeAbs",
	"Gain",
	"GainAuto",
	"GainSelector",
	"Gamma",
	"GammaEnabled",
	"GevCCP",
	"GevCurrentDefaultGateway",
	"GevCurrentIPAddress",
	"GevCurrentIPConfigurationDHCP",
	"GevCurrentIPConfigurationLLA",
	"GevCurrentIPConfigurationPersistentIP",
	"GevCurrentSubnetMask",
	"GevDeviceModeCharacterSet",
	"GevDeviceModeIsBigEndian",
	"GevFirstURL",
	"GevGVCPHeartbeatDisable",
	"GevGVCPPendingAck",
	"GevGVCPPendingTimeout",
	"GevHeartbeatTimeout",
	"GevInterfaceSelector",
	"GevLinkSpeed",
	"GevMACAddress",
	"GevMessageChannelCount",
	"GevNumberOfInterfaces",
	"GevPersistentDefaultGateway",
	"GevPersistentIPAddress",
	"GevPersistentSubnetMask",
	"GevPrimaryApplicationIPAddress",
	"GevPrimaryApplicationSocket",
	"GevSCDA",
	"GevSCPD",
	"GevSCPDirection",
	"GevSCPHostPort",
	"GevSCPInterfaceIndex",
	"GevSCPSBigEndian",
	"GevSCPSDoNotFragment",
	"GevSCPSFireTestPacket",
	"GevSCPSPacketSize",
	"GevSCSP",
	"GevSecondURL",
	"GevStreamChannelCount",
	"GevStreamChannelSelector",
	"GevSupportedOption",
	"GevSupportedOptionSelector",
	"GevTimestampControlLatch",
	"GevTimestampControlReset",
	"GevTimestampTickFrequency",
	"GevTimestampValue",
	"GevVersionMajor",
	"GevVersionMinor",
	"Height",
	"HeightMax",
	"Hue",
	"HueAuto",
	"HueEnabled",
	"LineDebouncerTimeRaw",
	"LineInverter",
	"LineMode",
	"LineSelector",
	"LineSource",
	"LineStatus",
	"LineStatusAll",
	"LUTEnable",
	"LUTIndex",
	"LUTSelector",
	"LUTValue",
	"LUTValueAll",
	"OffsetX",
	"OffsetY",
	"OnBoardColorProcessEnabled",
	"ParameterSelector",
	"PayloadSize",
	"pgrAutoExposureCompensationLowerLimit",
	"pgrAutoExposureCompensationUpperLimit",
	"pgrDevicePowerSupplySelector",
	"pgrDeviceUptime",
	"pgrExposureCompensation",
	"pgrExposureCompensationAuto",
	"pgrHDR_ExposureTimeAbs",
	"pgrHDR_GainAbs",
	"pgrHDRImageSelector",
	"pgrHDRModeEnabled",
	"pgrPixelBigEndian",
	"pgrPowerSourcePresent",
	"pgrPowerSupplyCurrent",
	"pgrPowerSupplyEnable",
	"pgrPowerSupplyVoltage",
	"pgrSensorDescription",
	"PixelCoding",
	"PixelColorFilter",
	"PixelDefectControl",
	"PixelDynamicRangeMax",
	"PixelDynamicRangeMin",
	"PixelFormat",
	"PixelSize",
	"RemoveLimits",
	"ReverseX",
	"Saturation",
	"SaturationAuto",
	"SaturationEnabled",
	"SensorHeight",
	"SensorShutterMode",
	"SensorWidth",
	"Sharpness",
	"SharpnessAuto",
	"SharpnessEnabled",
	"SingleFrameAcquisitionMode",
	"StrobeDelay",
	"StrobeDuration",
	"StrobeEnabled",
	"TestImageSelector",
	"TestPattern",
	"TestPendingAck",
	"Timestamp",
	"TimestampIncrement",
	"TimestampLatch",
	"TLParamsLocked",
	"TransmitFailureCount",
	"TransmitFailureCountReset",
	"TransmitFrameCount",
	"TriggerActivation",
	"TriggerDelay",
	"TriggerDelayEnabled",
	"TriggerEventTest",
	"TriggerMode",
	"TriggerOverlap",
	"TriggerSelector",
	"TriggerSoftware",
	"TriggerSource",
	"U3VAccessPrivilege",
	"U3VCPCapability",
	"U3VCPCapabilityHigh",
	"U3VCPCapabilityLow",
	"U3VCPConfigurationHigh",
	"U3VCPConfigurationLow",
	"U3VCPEIRMAvailable",
	"U3VCPIIDC2Available",
	"U3VCPSIRMAvailable",
	"U3VCurrentSpeed",
	"U3VDeviceCapability",
	"U3VDeviceConfigurationHigh",
	"U3VDeviceConfigurationLow",
	"U3VMaxAcknowledgeTransferLength",
	"U3VMaxCommandTransferLength",
	"U3VMaxDeviceResponseTime",
	"U3VMessageChannelID",
	"U3VNumberOfStreamChannels",
	"U3VVersionMajor",
	"U3VVersionMinor",
	"USB3LinkRecoveryCount",
	"UserDefinedValue",
	"UserDefinedValueSelector",
	"UserOutputSelector",
	"UserOutputValue",
	"UserSetCurrent",
	"UserSetDefault",
	"UserSetDefaultSelector",
	"UserSetLoad",
	"UserSetSave",
	"UserSetSelector",
	"V3_3Enable",
	"VideoMode",
	"Width",
	"WidthMax"
};
