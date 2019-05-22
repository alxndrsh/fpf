#ifndef DECODE_RS_H_INCLUDED
#define DECODE_RS_H_INCLUDED
/*
    Include header for RS decoding function, taken from Phil Karn's libfec,
    see .c file for details.
*/
typedef unsigned char data_t;
int decode_rs_ccsds(data_t *data,int *eras_pos,int no_eras,int pad);

#endif // DECODE_RS_H_INCLUDED
