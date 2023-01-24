#pragma once
#include <memory>

extern "C" { // how to only declare C prototypes here?
#define _MATH_DEFINES_DEFINED //avoid warning
#include "xvid.h"
}

class XVidEncoder
{
public:
	XVidEncoder();
	void encode_frame(unsigned char *image);

protected:
	void setup();
	static std::unique_ptr<xvid_enc_zone_t> createDefaultQuantZone();
	static constexpr size_t getImageSize(size_t witdth, size_t height);


private:
	void *m_enc_handle = nullptr;
};
