/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <memory>

#include <taglib/tfilestream.h>
#include <taglib/oggflacfile.h>
#include <taglib/flacproperties.h>

#include "OggFLACMetadata.h"
#include "CFWrapper.h"
#include "CFErrorUtilities.h"
#include "AddXiphCommentToDictionary.h"
#include "SetXiphCommentFromMetadata.h"
#include "AddAudioPropertiesToDictionary.h"
#include "CFDictionaryUtilities.h"

namespace {

	void RegisterOggFLACMetadata() __attribute__ ((constructor));
	void RegisterOggFLACMetadata()
	{
		SFB::Audio::Metadata::RegisterSubclass<SFB::Audio::OggFLACMetadata>();
	}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::OggFLACMetadata::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("ogg"), CFSTR("oga") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 2, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::OggFLACMetadata::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/ogg") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 1, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::OggFLACMetadata::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("ogg"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("oga"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::OggFLACMetadata::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;
	
	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/ogg"), kCFCompareCaseInsensitive))
		return true;
	
	return false;
}

SFB::Audio::Metadata::unique_ptr SFB::Audio::OggFLACMetadata::CreateMetadata(CFURLRef url)
{
	return unique_ptr(new OggFLACMetadata(url));
}

#pragma mark Creation and Destruction

SFB::Audio::OggFLACMetadata::OggFLACMetadata(CFURLRef url)
	: Metadata(url)
{}

#pragma mark Functionality

bool SFB::Audio::OggFLACMetadata::_ReadMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;
	
	std::unique_ptr<TagLib::FileStream> stream(new TagLib::FileStream((const char *)buf, true));
	if(!stream->isOpen()) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” could not be opened for reading."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Input/output error"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file may have been renamed, moved, deleted, or you may not have appropriate permissions."), "");

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	TagLib::Ogg::FLAC::File file(stream.get());
	if(!file.isValid()) {
		if(nullptr != error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Ogg file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Not an Ogg file"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}
		
		return false;
	}

	CFDictionarySetValue(mMetadata, kFormatNameKey, CFSTR("Ogg FLAC"));

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		AddAudioPropertiesToDictionary(mMetadata, properties);
		
		if(properties->sampleWidth())
			AddIntToDictionary(mMetadata, kBitsPerChannelKey, properties->sampleWidth());
	}

	if(file.tag())
		AddXiphCommentToDictionary(mMetadata, mPictures, file.tag());

	return true;
}

bool SFB::Audio::OggFLACMetadata::_WriteMetadata(CFErrorRef *error)
{
	UInt8 buf [PATH_MAX];
	if(!CFURLGetFileSystemRepresentation(mURL, false, buf, PATH_MAX))
		return false;

	std::unique_ptr<TagLib::FileStream> stream(new TagLib::FileStream((const char *)buf));
	if(!stream->isOpen()) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” could not be opened for writing."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Input/output error"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file may have been renamed, moved, deleted, or you may not have appropriate permissions."), "");

			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	TagLib::Ogg::FLAC::File file(stream.get(), false);
	if(!file.isValid()) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Ogg file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Not an Ogg file"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}

		return false;
	}

	SetXiphCommentFromMetadata(*this, file.tag());

	if(!file.save()) {
		if(error) {
			SFB::CFString description = CFCopyLocalizedString(CFSTR("The file “%@” is not a valid Ogg file."), "");
			SFB::CFString failureReason = CFCopyLocalizedString(CFSTR("Unable to write metadata"), "");
			SFB::CFString recoverySuggestion = CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), "");
			
			*error = CreateErrorForURL(Metadata::ErrorDomain, Metadata::InputOutputError, description, mURL, failureReason, recoverySuggestion);
		}
		
		return false;
	}

	return true;
}
