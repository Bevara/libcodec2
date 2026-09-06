/*
 *			GPAC - Multimedia Framework C SDK
 *
 *  This file is part of GPAC / Codec 2 decoder filter, based on David Rowe's
 *  libcodec2.
 *
 *  Codec 2 is a very-low-bitrate speech codec - 700 bit/s to 3200 bit/s - used
 *  mostly in amateur digital voice. It always works at 8 kHz mono.
 *
 *  A .c2 file opens with a seven-byte header (magic c0 de c2, a version pair,
 *  the mode and a flags byte) and then holds one packed frame after another,
 *  each of the size the mode dictates. Headerless .bin files exist too, but
 *  nothing in them says which mode they use, so they are refused rather than
 *  guessed at.
 */

#include <gpac/filters.h>
#include <gpac/constants.h>
#include <string.h>
#include <stdlib.h>

#include <codec2.h>

#define CODEC2_RATE 8000

typedef struct
{
	GF_FilterPid *ipid, *opid;
} GF_Codec2DecCtx;

static GF_Err codec2dec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_Codec2DecCtx *ctx = (GF_Codec2DecCtx *)gf_filter_get_udta(filter);

	if (is_remove)
	{
		if (ctx->opid)
		{
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	if (!ctx->opid)
		ctx->opid = gf_filter_pid_new(filter);

	/* Everything about the output is fixed by the codec: 8 kHz mono, 16-bit. */
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S16));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(CODEC2_RATE));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(CODEC2_RATE));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(1));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_LONGUINT(GF_AUDIO_CH_FRONT_CENTER));

	return GF_OK;
}

static GF_Err codec2dec_process(GF_Filter *filter)
{
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u32 size, pos, nb_frames, i;
	int mode, samples_per_frame, bits_per_frame, bytes_per_frame;
	struct CODEC2 *c2 = NULL;
	GF_Codec2DecCtx *ctx = (GF_Codec2DecCtx *)gf_filter_get_udta(filter);

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck)
	{
		if (gf_filter_pid_is_eos(ctx->ipid))
		{
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	data = (u8 *)gf_filter_pck_get_data(pck, &size);
	if (!data || (size < 8))
	{
		gf_filter_pid_drop_packet(ctx->ipid);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[Codec2Dec] File too short to hold a header and a frame\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if ((data[0] != 0xC0) || (data[1] != 0xDE) || (data[2] != 0xC2))
	{
		gf_filter_pid_drop_packet(ctx->ipid);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[Codec2Dec] Not a Codec 2 file with a header; a headerless stream does not say which mode it uses\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	mode = data[5];
	c2 = codec2_create(mode);
	if (!c2)
	{
		gf_filter_pid_drop_packet(ctx->ipid);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[Codec2Dec] Unsupported Codec 2 mode %d\n", mode));
		return GF_NOT_SUPPORTED;
	}

	samples_per_frame = codec2_samples_per_frame(c2);
	bits_per_frame = codec2_bits_per_frame(c2);
	bytes_per_frame = (bits_per_frame + 7) / 8;
	if ((samples_per_frame <= 0) || (bytes_per_frame <= 0))
	{
		codec2_destroy(c2);
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NOT_SUPPORTED;
	}

	pos = 7; /* magic(3) + version(2) + mode(1) + flags(1) */
	nb_frames = (size - pos) / (u32)bytes_per_frame;
	if (!nb_frames)
	{
		codec2_destroy(c2);
		gf_filter_pid_drop_packet(ctx->ipid);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[Codec2Dec] No complete frame after the header\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, nb_frames * (u32)samples_per_frame * 2, &output);
	if (!dst_pck)
	{
		codec2_destroy(c2);
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OUT_OF_MEM;
	}

	for (i = 0; i < nb_frames; i++)
	{
		codec2_decode(c2, (short *)(output + (size_t)i * samples_per_frame * 2),
		              data + pos + (size_t)i * bytes_per_frame);
	}
	codec2_destroy(c2);

	gf_filter_pck_set_cts(dst_pck, 0);
	gf_filter_pck_set_duration(dst_pck, nb_frames * (u32)samples_per_frame);
	gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
	gf_filter_pck_send(dst_pck);

	gf_filter_pid_drop_packet(ctx->ipid);
	gf_filter_pid_set_eos(ctx->opid);
	return GF_EOS;
}

static void codec2dec_finalize(GF_Filter *filter)
{
}

static const GF_FilterCapability Codec2DecCaps[] =
	{
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
		CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "c2"),
		CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/codec2|audio/x-codec2"),
		CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
		CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister Codec2DecoderRegister = {
	.name = "codec2dec",
	GF_FS_SET_DESCRIPTION("Codec 2 low-bitrate speech decoder")
		GF_FS_SET_HELP("This filter decodes Codec 2 speech files (.c2, with the seven-byte header that names the mode) using libcodec2. Output is always 8 kHz mono.")
			.private_size = sizeof(GF_Codec2DecCtx),
	SETCAPS(Codec2DecCaps),
	.configure_pid = codec2dec_configure_pid,
	.process = codec2dec_process,
	.finalize = codec2dec_finalize,
};

const GF_FilterRegister *EMSCRIPTEN_KEEPALIVE codec2dec_register(GF_FilterSession *session)
{
	return &Codec2DecoderRegister;
}

#include "filter_register.h"
__attribute__((constructor))
void register_codec2dec(void) {
    gf_filter_auto_register("codec2dec", codec2dec_register);
}
