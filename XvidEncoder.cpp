#include "XvidEncoder.h"

#include <windows.h>
#include <vfw.h>
#include <memory>
#include <iostream> //TODO remove
#include <string> //TODO remove

extern "C" {
#define _MATH_DEFINES_DEFINED //avoid warning
#include "xvid.h"
}


XVidEncoder::XVidEncoder()
{
	setup();
}

///---------- infos to get or to define as constants ----------
static constexpr int ARG_DEBUG = 0;
static constexpr int XDIM = 640;
static constexpr int YDIM = 480;
///=============== xvid_enc_zone_t ===============
///----- xvid_plugin_single_t ------
static constexpr int ARG_BITRATE = 0; /* [in] target bitrate (bits per second) */
static constexpr int ARG_REACTION = 16; /*xvid_plugin_single_t.reaction_delay_factor*/
static constexpr int ARG_AVERAGING = 100; /*xvid_plugin_single_t.averaging_period*/
static constexpr int ARG_SMOOTHER = 100; /*xvid_plugin_single_t.buffer*/
///---------------------------------
///--------xvid_enc_zone_t ---------
static constexpr int FRAME_DEFAULT = 0; /*xvid_encraw.c L2720*/
static constexpr int MODE_DEFAULT = XVID_ZONE_QUANT; /*xvid_encraw.c L2720*/
static constexpr int BASE_DEFAULT = 100; /*xvid_encraw.c L2343*/
static constexpr int DEFAULT_QUANT = 400; /*default value for xvid_plugin_single_t.modifier or ARG_CQ for 1pass encoding without specified bitrate*/
///---------------------------------
static constexpr int THREAD_COUNT_DEFAULT = 1; /*extract from xvid_gbl_info.num_threads in xvid_encraw.c L2491*/
static constexpr int ARG_SLICES = 100; /* [in:opt] number of slices to code for each frame */
static constexpr int ARG_DWSCALE = 1; /* [in:opt] framerate increment; set to zero for variable framerate */
static constexpr int ARG_DWRATE = 25; /* [in] framerate base frame_duration = fincr/fbase seconds*/
static constexpr int ARG_MAXKEYINTERVAL = 300; /* [in:opt] the maximum interval between key frames */
static constexpr int ARG_QUANTS[] = { 2, 31, 2, 31, 2, 31 };
static constexpr int ARG_MAXBFRAMES = 2; /* [in:opt] max sequential bframes (0=disable bframes) */
static constexpr int ARG_BQRATIO = 150; /* [in:opt] bframe quantizer multipier/offeset; used to decide bframes quant when bquant==-1 */
static constexpr int ARG_BQOFFSET = 100; /* bquant = (avg(past_ref_quant,future_ref_quant)*bquant_ratio + bquant_offset) / 100 */
static constexpr int ARG_FRAMEDROP = 0;  /* [in:opt] frame dropping: 0=drop none... 100=drop all */
static constexpr int ARG_STARTFRAMENR = 0; /* [in:opt] frame number of start frame relative to zones definitions. allows to encode sub-sequences */
///----------- .global -------------
static constexpr int ARG_CLOSED_GOP = XVID_GLOBAL_CLOSED_GOP;/* closed_gop:	was DX50BVOP dx50 bvop compatibility */
static constexpr int ARG_PACKED = XVID_GLOBAL_PACKED; /* packed bitstream */
/* [in:opt] global flags; controls encoding behavior */
static constexpr int ARG_GLOBAL_OPT = 0
| ARG_CLOSED_GOP
| ARG_PACKED
;

std::unique_ptr<xvid_enc_zone_t> XVidEncoder::createDefaultQuantZone()
{
	/*---- prepare_cquant_zones -----*/
	auto zone = std::make_unique<xvid_enc_zone_t>();
	zone->frame = FRAME_DEFAULT;
	zone->mode = MODE_DEFAULT;
	zone->increment = DEFAULT_QUANT;
	zone->base = BASE_DEFAULT;
	return zone;
}

void XVidEncoder::setup()
{
	xvid_gbl_init_t xvid_gbl_init;
	
	/*------------------------------------------------------------------------
	* Xvid core initialization (enc_gbl)
	*----------------------------------------------------------------------*/
	/* Set version -- version checking will done by xvidcore */
	memset(&xvid_gbl_init, 0, sizeof(xvid_gbl_init));
	xvid_gbl_init.version = XVID_VERSION;
	xvid_gbl_init.debug = ARG_DEBUG;
	/* Initialize Xvid core -- Should be done once per __process__ */
	xvid_global(NULL, XVID_GBL_INIT, &xvid_gbl_init, NULL);
	const auto arg_cpu_flags = xvid_gbl_init.cpu_flags;
	/*------------------------------------------------------------------------
	* Xvid encore initialization (enc_init)
	*----------------------------------------------------------------------*/
	xvid_enc_create_t xvid_enc_create;
	xvid_enc_create.width = XDIM;
	xvid_enc_create.height = YDIM;
	xvid_enc_create.profile = 0xf5; /* Unrestricted */
	xvid_enc_plugin_t plugins[8];
	xvid_enc_create.plugins = plugins;
	xvid_enc_create.num_plugins = 0;
	//set single pass plugin
	xvid_plugin_single_t single;
	memset(&single, 0, sizeof(xvid_plugin_single_t));
	single.version = XVID_VERSION;
	single.bitrate = ARG_BITRATE;
	single.reaction_delay_factor = ARG_REACTION;
	single.averaging_period = ARG_AVERAGING;
	single.buffer = ARG_SMOOTHER;
	plugins[xvid_enc_create.num_plugins].func = xvid_plugin_single;
	plugins[xvid_enc_create.num_plugins].param = &single;
	xvid_enc_create.num_plugins++;
	auto zone = createDefaultQuantZone(); //to store somewhere
	xvid_enc_create.zones = zone.get();
	xvid_enc_create.num_zones = 1;
	xvid_enc_create.num_threads = THREAD_COUNT_DEFAULT;
	xvid_enc_create.num_slices = ARG_SLICES;
	xvid_enc_create.fincr = ARG_DWSCALE;
	xvid_enc_create.fbase = ARG_DWRATE;
	xvid_enc_create.max_key_interval = ARG_MAXKEYINTERVAL;
	// ---------- from xvid_encraw.c L2414 ---------- 
	xvid_enc_create.min_quant[0] = ARG_QUANTS[0];
	xvid_enc_create.min_quant[1] = ARG_QUANTS[2];
	xvid_enc_create.min_quant[2] = ARG_QUANTS[4];
	xvid_enc_create.max_quant[0] = ARG_QUANTS[1];
	xvid_enc_create.max_quant[1] = ARG_QUANTS[3];
	xvid_enc_create.max_quant[2] = ARG_QUANTS[5];
	// ----------------------------------------------
	/* Bframes settings */
	xvid_enc_create.max_bframes = ARG_MAXBFRAMES;
	xvid_enc_create.bquant_ratio = ARG_BQRATIO;
	xvid_enc_create.bquant_offset = ARG_BQOFFSET;
	
	/* Frame drop ratio */
	xvid_enc_create.frame_drop_ratio = ARG_FRAMEDROP;
	
	/* Start frame number */
	xvid_enc_create.start_frame_num = ARG_STARTFRAMENR;
	
	/* Global encoder options */
	xvid_enc_create.global = ARG_GLOBAL_OPT;
	auto xerr = xvid_encore(NULL, XVID_ENC_CREATE, &xvid_enc_create, NULL);
	if (xerr) {
		//error case 
		// TODO handle errors
		std::cout << "error\n";
	}
	/* Retrieve the encoder instance from the structure */
	m_enc_handle = xvid_enc_create.handle;
}

constexpr size_t XVidEncoder::getImageSize(size_t witdth, size_t height)
{
	return witdth * height * 3 / 2;
}

///----------------------------------
static constexpr int ARG_COLORSPACE = XVID_CSP_YV12; /* [in] colorspace; or with XVID_CSP_VFLIP to perform vertical flip */
static constexpr int ARG_PAR = XVID_PAR_11_VGA;  /* [in:opt] pixel aspect ratio (refer to XVID_PAR_xxx macros) */
static constexpr int VOP_FLAGS = 0 /* [in] (general)vop-based flags */
| XVID_VOP_INTER4V /* use 4 motion vectors per MB */
| XVID_VOP_HALFPEL /* use halfpel interpolation */
| XVID_VOP_HQACPRED /* high quality ac prediction */
| XVID_VOP_TRELLISQUANT /* use trellis based R-D "optimal" quantization */
 /* VHQ_MODE_DECISION */
	| XVID_VOP_MODEDECISION_RD /* enable DCT-ME and use it for mode decision */
	;
static constexpr int MOTION_FLAGS = 0 /* Motion Estimation options */
| XVID_ME_CHROMA_PVOP /* also use chroma for P_VOP/S_VOP ME */
| XVID_ME_CHROMA_BVOP /* also use chroma for B_VOP ME */
| XVID_ME_HALFPELREFINE16
| XVID_ME_EXTSEARCH16 /* extend PMV by more searches */
| XVID_ME_HALFPELREFINE8
| XVID_ME_USESQUARES16 /* use squares instead of diamonds as search pattern */
;
///===============================================


unsigned char *image = nullptr; // To get
//maybe change image type and cast it inside
void XVidEncoder::encode_frame(unsigned char *image)
{
	/*	enc_main(void *enc_handle,
			unsigned char *image, // input avi stream
			unsigned char *bitstream, // output mpt strean
			int *key,
			int *stats_type,
			int *stats_quant,
			int *stats_length,
			int sse[3],
			int framenum)
	*/
	int ret;

	xvid_enc_frame_t xvid_enc_frame;
	xvid_enc_stats_t xvid_enc_stats;

	/* Version for the frame and the stats */
	memset(&xvid_enc_frame, 0, sizeof(xvid_enc_frame));
	xvid_enc_frame.version = XVID_VERSION;

	memset(&xvid_enc_stats, 0, sizeof(xvid_enc_stats));
	xvid_enc_stats.version = XVID_VERSION;

	//size is *2 in orignal. TODO Check if it's necessary?
	auto ouptut = std::make_unique<unsigned char[]>(getImageSize(XDIM, YDIM));
	/* Bind output buffer */
	xvid_enc_frame.bitstream = ouptut.get();
	xvid_enc_frame.length = -1;

	/* Initialize input image fields */
	if (image) {
		xvid_enc_frame.input.plane[0] = image;
#ifndef READ_PNM
		xvid_enc_frame.input.csp = ARG_COLORSPACE;
		xvid_enc_frame.input.stride[0] = XDIM;
#else
		xvid_enc_frame.input.csp = XVID_CSP_BGR;
		xvid_enc_frame.input.stride[0] = XDIM * 3;
#endif
	}
	else {
		xvid_enc_frame.input.csp = XVID_CSP_NULL;
	}

	/* Set up core's general features */
	xvid_enc_frame.vol_flags = 0;
	xvid_enc_frame.par = ARG_PAR;

	/* Set up core's general features */
	xvid_enc_frame.vop_flags = VOP_FLAGS;

	//XVID_TYPE_PVOP if frame > stop_fram XVID_TYPE_AUTO otherwise
	int stats_type = XVID_TYPE_AUTO; //TODO impl 
	/* Frame type -- taken from function call parameter */
	/* Sometimes we might want to force the last frame to be a P Frame */
	xvid_enc_frame.type = stats_type;

	/* Force the right quantizer -- It is internally managed by RC plugins */
	xvid_enc_frame.quant = 0;
	/* Set up motion estimation flags */
	xvid_enc_frame.motion = MOTION_FLAGS;

	////Should accelarate encoding process if used
	/*	xvid_enc_frame.motion |= XVID_ME_FASTREFINE16 | XVID_ME_FASTREFINE8 |
		XVID_ME_SKIP_DELTASEARCH | XVID_ME_FAST_MODEINTERPOLATE |
		XVID_ME_BFRAME_EARLYSTOP;*/

		/* frame-based stuff */
		//apply_zone_modifiers(&xvid_enc_frame, framenum); //TODO check if it change sometinhg


	/* Encode the frame */
	ret = xvid_encore(m_enc_handle, XVID_ENC_ENCODE, &xvid_enc_frame,
		&xvid_enc_stats);

	//*key = (xvid_enc_frame.out_flags & XVID_KEYFRAME);
	//*stats_type = xvid_enc_stats.type;
	//*stats_quant = xvid_enc_stats.quant;
	//*stats_length = xvid_enc_stats.length;
	//sse[0] = xvid_enc_stats.sse_y;
	//sse[1] = xvid_enc_stats.sse_u;
	//sse[2] = xvid_enc_stats.sse_v;

	//return ret;
}
