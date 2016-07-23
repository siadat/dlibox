/* Automatically generated nanopb header */
/* Generated by nanopb-0.3.6-dev */

#ifndef PB_ANIM1D_MSG_PB_H_INCLUDED
#define PB_ANIM1D_MSG_PB_H_INCLUDED
#include <pb.h>

/* @@protoc_insertion_point(includes) */
#if PB_PROTO_HEADER_VERSION != 30
#error Regenerate this file with the current version of nanopb generator.
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Struct definitions */
typedef struct _MFrame {
    pb_callback_t colors;
/* @@protoc_insertion_point(struct:MFrame) */
} MFrame;

typedef struct _MRainbow {
    char dummy_field;
/* @@protoc_insertion_point(struct:MRainbow) */
} MRainbow;

typedef struct _MColor {
    uint32_t color;
/* @@protoc_insertion_point(struct:MColor) */
} MColor;

typedef struct _MRepeated {
    MFrame frame;
/* @@protoc_insertion_point(struct:MRepeated) */
} MRepeated;

typedef struct _MPattern {
    bool has_color;
    MColor color;
    bool has_frame;
    MFrame frame;
    bool has_rainbow;
    MRainbow rainbow;
    bool has_repeat;
    MRepeated repeat;
/* @@protoc_insertion_point(struct:MPattern) */
} MPattern;

/* Default values for struct fields */
extern const uint32_t MColor_color_default;

/* Initializer values for message structs */
#define MColor_init_default                      {0u}
#define MFrame_init_default                      {{{NULL}, NULL}}
#define MRainbow_init_default                    {0}
#define MRepeated_init_default                   {MFrame_init_default}
#define MPattern_init_default                    {false, MColor_init_default, false, MFrame_init_default, false, MRainbow_init_default, false, MRepeated_init_default}
#define MColor_init_zero                         {0}
#define MFrame_init_zero                         {{{NULL}, NULL}}
#define MRainbow_init_zero                       {0}
#define MRepeated_init_zero                      {MFrame_init_zero}
#define MPattern_init_zero                       {false, MColor_init_zero, false, MFrame_init_zero, false, MRainbow_init_zero, false, MRepeated_init_zero}

/* Field tags (for use in manual encoding/decoding) */
#define MFrame_colors_tag                        1
#define MColor_color_tag                         1
#define MRepeated_frame_tag                      1
#define MPattern_color_tag                       1
#define MPattern_frame_tag                       2
#define MPattern_rainbow_tag                     3
#define MPattern_repeat_tag                      4

/* Struct field encoding specification for nanopb */
extern const pb_field_t MColor_fields[2];
extern const pb_field_t MFrame_fields[2];
extern const pb_field_t MRainbow_fields[1];
extern const pb_field_t MRepeated_fields[2];
extern const pb_field_t MPattern_fields[5];

/* Maximum encoded size of messages (where known) */
#define MColor_size                              6
/* MFrame_size depends on runtime parameters */
#define MRainbow_size                            0
#define MRepeated_size                           (6 + MFrame_size)
#define MPattern_size                            (28 + MFrame_size + MFrame_size)

/* Message IDs (where set with "msgid" option) */
#ifdef PB_MSGID

#define ANIM1D_MSG_MESSAGES \


#endif

#ifdef __cplusplus
} /* extern "C" */
#endif
/* @@protoc_insertion_point(eof) */

#endif
