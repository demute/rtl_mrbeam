#ifndef _PARSER_H_
#define _PARSER_H_

void *mrbeam_setup (void);
void sdr_callback(unsigned char *iq_buf, uint32_t len, void *ctx);

#endif /* _PARSER_H_ */
