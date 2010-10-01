/*
             LUFA Library
     Copyright (C) Dean Camera, 2010.
              
  dean [at] fourwalledcubicle [dot] com
      www.fourwalledcubicle.com
*/

/*
  Copyright 2010  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this 
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in 
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting 
  documentation, and that the name of the author not be used in 
  advertising or publicity pertaining to distribution of the 
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  USB Device Configuration Descriptor processing routines, to determine the correct pipe configurations
 *  needed to communication with an attached USB device. Descriptors are special  computer-readable structures
 *  which the host requests upon device enumeration, to determine the device's capabilities and functions.
 */
 
#include "ConfigDescriptor.h"

/** Reads and processes an attached device's descriptors, to determine compatibility and pipe configurations. This
 *  routine will read in the entire configuration descriptor, and configure the hosts pipes to correctly communicate
 *  with compatible devices.
 *
 *  This routine searches for a MIDI interface descriptor pair containing bulk data IN and OUT endpoints.
 *
 *  \return An error code from the \ref MIDIHost_GetConfigDescriptorDataCodes_t enum.
 */
uint8_t ProcessConfigurationDescriptor(void)
{
	uint8_t  ConfigDescriptorData[512];
	void*    CurrConfigLocation = ConfigDescriptorData;
	uint16_t CurrConfigBytesRem;
	
	USB_Descriptor_Interface_t* MIDIInterface   = NULL;
	USB_Descriptor_Endpoint_t*  DataINEndpoint  = NULL;
	USB_Descriptor_Endpoint_t*  DataOUTEndpoint = NULL;

	/* Retrieve the entire configuration descriptor into the allocated buffer */
	switch (USB_Host_GetDeviceConfigDescriptor(1, &CurrConfigBytesRem, ConfigDescriptorData, sizeof(ConfigDescriptorData)))
	{
		case HOST_GETCONFIG_Successful:
			break;
		case HOST_GETCONFIG_InvalidData:
			return InvalidConfigDataReturned;
		case HOST_GETCONFIG_BuffOverflow:
			return DescriptorTooLarge;
		default:
			return ControlError;
	}
	
	while (!(DataINEndpoint) || !(DataOUTEndpoint))
	{
		/* See if we've found a likely compatible interface, and if there is an endpoint within that interface */
		if (!(MIDIInterface) ||
		    USB_GetNextDescriptorComp(&CurrConfigBytesRem, &CurrConfigLocation,
		                              DComp_NextMIDIStreamingDataEndpoint) != DESCRIPTOR_SEARCH_COMP_Found)
		{
			/* Get the next Mass Storage interface from the configuration descriptor */
			if (USB_GetNextDescriptorComp(&CurrConfigBytesRem, &CurrConfigLocation,
										  DComp_NextMIDIStreamingInterface) != DESCRIPTOR_SEARCH_COMP_Found)
			{
				/* Descriptor not found, error out */
				return NoCompatibleInterfaceFound;
			}

			/* Save the interface in case we need to refer back to it later */
			MIDIInterface = DESCRIPTOR_PCAST(CurrConfigLocation, USB_Descriptor_Interface_t);

			/* Clear any found endpoints */
			DataINEndpoint  = NULL;
			DataOUTEndpoint = NULL;

			/* Skip the remainder of the loop as we have not found an endpoint yet */
			continue;
		}
		
		/* Retrieve the endpoint address from the endpoint descriptor */
		USB_Descriptor_Endpoint_t* EndpointData = DESCRIPTOR_PCAST(CurrConfigLocation, USB_Descriptor_Endpoint_t);

		/* If the endpoint is a IN type endpoint */
		if (EndpointData->EndpointAddress & ENDPOINT_DESCRIPTOR_DIR_IN)
		  DataINEndpoint  = EndpointData;
		else
		  DataOUTEndpoint = EndpointData;
	}
	
	/* Configure the MIDI data IN pipe */
	Pipe_ConfigurePipe(MIDI_DATA_IN_PIPE, EP_TYPE_BULK, PIPE_TOKEN_IN,
	                   DataINEndpoint->EndpointAddress, DataINEndpoint->EndpointSize, PIPE_BANK_SINGLE);

	/* Configure the MIDI data OUT pipe */
	Pipe_ConfigurePipe(MIDI_DATA_OUT_PIPE, EP_TYPE_BULK, PIPE_TOKEN_OUT,
					   DataOUTEndpoint->EndpointAddress, DataOUTEndpoint->EndpointSize, PIPE_BANK_SINGLE);

	/* Valid data found, return success */
	return SuccessfulConfigRead;
}

/** Descriptor comparator function. This comparator function is can be called while processing an attached USB device's
 *  configuration descriptor, to search for a specific sub descriptor. It can also be used to abort the configuration
 *  descriptor processing if an incompatible descriptor configuration is found.
 *
 *  This comparator searches for the next Interface descriptor of the correct MIDI Streaming Class, Subclass and Protocol values.
 *
 *  \return A value from the DSEARCH_Return_ErrorCodes_t enum
 */
uint8_t DComp_NextMIDIStreamingInterface(void* CurrentDescriptor)
{
	if (DESCRIPTOR_TYPE(CurrentDescriptor) == DTYPE_Interface)
	{
		/* Check the MIDI descriptor class, subclass and protocol, break out if correct data interface found */
		if ((DESCRIPTOR_CAST(CurrentDescriptor, USB_Descriptor_Interface_t).Class    == MIDI_STREAMING_CLASS)    &&
		    (DESCRIPTOR_CAST(CurrentDescriptor, USB_Descriptor_Interface_t).SubClass == MIDI_STREAMING_SUBCLASS) &&
		    (DESCRIPTOR_CAST(CurrentDescriptor, USB_Descriptor_Interface_t).Protocol == MIDI_STREAMING_PROTOCOL))
		{
			return DESCRIPTOR_SEARCH_Found;
		}
	}
	
	return DESCRIPTOR_SEARCH_NotFound;
}

/** Descriptor comparator function. This comparator function is can be called while processing an attached USB device's
 *  configuration descriptor, to search for a specific sub descriptor. It can also be used to abort the configuration
 *  descriptor processing if an incompatible descriptor configuration is found.
 *
 *  This comparator searches for the next bulk IN or OUT endpoint within the current interface, aborting the search if
 *  another interface descriptor is found before the required endpoint.
 *
 *  \return A value from the DSEARCH_Return_ErrorCodes_t enum
 */
uint8_t DComp_NextMIDIStreamingDataEndpoint(void* CurrentDescriptor)
{
	if (DESCRIPTOR_TYPE(CurrentDescriptor) == DTYPE_Endpoint)
	{
		uint8_t EndpointType = (DESCRIPTOR_CAST(CurrentDescriptor,
		                                        USB_Descriptor_Endpoint_t).Attributes & EP_TYPE_MASK);
	
		/* Check the endpoint type, break out if correct BULK type endpoint found */
		if (EndpointType == EP_TYPE_BULK)
		  return DESCRIPTOR_SEARCH_Found;
	}
	else if (DESCRIPTOR_TYPE(CurrentDescriptor) == DTYPE_Interface)
	{
		return DESCRIPTOR_SEARCH_Fail;
	}

	return DESCRIPTOR_SEARCH_NotFound;
}
