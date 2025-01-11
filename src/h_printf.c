/****************************************************
 * SIMPLE PRINTF LIB
 * VERSION : 1.0
 * History:
 * 	+2022_08_12: Fix float format issue
 * 	+2022_08_12: Add config to enable/disable float support for printf
 * 	+2022_08_23: Fix format for %o,%x,%b with space format
 * 	+2023_03_29: Fix dot_detected state issue(not reseted when get % symbol)
 * 	+2023_07_14: Fix %s with fixed right align loop issue
 */

#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#define PRINTF_SUPPORT_FLOAT	1
#define PRINTF_BUF_SIZE			16
/*
typedef union {
	float 		f;
	uint32_t u32;
}float_u32_union;*/
//======================================================================
static const uint32_t div_table[]= {1000000000,100000000,10000000,1000000,100000,10000,1000,100,10,1};

#if PRINTF_BUF_SIZE < 8
#error "Invalid setting for PRINTF_BUF_SIZE"
#endif
//======================================================================
void __attribute__((weak)) outbyte(uint8_t character){

}
//======================================================================
#define  h_vprintf_outbyte(buf, buf_size,out_len,ch) do{\
		uint8_t ch1 =(ch);\
		if(buf!=NULL){ \
			if(out_len +1 < buf_size){\
				buf[out_len++] = ch1;\
				buf[out_len]   = 0;  \
			}\
		}\
		else outbyte(ch1);\
}while(0)
//======================================================================
#define out_u32(out_buf,out_buf_size,out_len,u_val,flags) {\
		uint8_t iii;\
		uint32_t uval1 = (u_val);\
		uint8_t flags1 = (flags);\
		for(iii=0;iii < sizeof(div_table)>>2;iii++){\
			uint8_t digit_val = uval1/div_table[iii];\
			uval1 -= digit_val*div_table[iii];\
			if(digit_val >0) (flags1) = 1;\
			if(flags1) h_vprintf_outbyte(out_buf,out_buf_size,out_len,(uint8_t)(digit_val +'0'));\
		}\
}
//======================================================================
//typedef union{
//	uint32_t h[2];
//	double 	 df;
//}float_typedef;
int h_vprintf(char *out_buf, int32_t out_buf_size, const char *format_const,va_list argp)// ...)
{
	int32_t out_len =0;
	int8_t format_state = 0;
	int16_t left_align =0,padding_zero=0,need_check_padding=0,total_charactor=0,dot_detected=0,fract_len=0;
	uint8_t *format = (uint8_t *)format_const;
	for(;(format != NULL) && (*format != (uint8_t)0);format++) {
		if(format_state ==0){
			if (*format != '%'){
				h_vprintf_outbyte(out_buf,out_buf_size,out_len,*format);
			}
			else {
				dot_detected =0;
				format_state = 1;
				left_align = 0;
				padding_zero = 0;
				need_check_padding =1;
				total_charactor =0;
				fract_len =0;
			}
		}
		else if(format_state ==1) {
			format_state = 0;
			switch(*format){
			//------------------------------------------------
			case '-'://%-
				left_align = 1;
				format_state =1;
				need_check_padding =1;
				break;
				//------------------------------------------------
			case '0':
				format_state =1;
				if(need_check_padding) padding_zero = 1;
				if(dot_detected==0) total_charactor = total_charactor*10 + (*format)-'0';
				else  				fract_len = fract_len*10 + (*format)-'0';
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				format_state =1;
				need_check_padding = 0;
				if(dot_detected==0) total_charactor = total_charactor*10 + (*format)-'0';
				else  				fract_len = fract_len*10 + (*format)-'0';
				break;
				//------------------------------------------------
			case '.':
				format_state = 1;
				dot_detected = 1;
				break;
				//------------------------------------------------
			case '%'://%%
				h_vprintf_outbyte(out_buf,out_buf_size,out_len,'%');
				break;
				//------------------------------------------------
			case 'c':	//%c
			{
				char ch =   va_arg(argp, int);
				h_vprintf_outbyte(out_buf,out_buf_size,out_len,ch);
			}
			break;
			//------------------------------------------------
			case 's'://%s
			{
				char *s =   va_arg(argp, char*);
				if(total_charactor){
					int16_t char_len =0;
					if(left_align){ //Left align adjustment
						for(;s != NULL && *s!= 0;s++){					//output data and count string len
							h_vprintf_outbyte(out_buf,out_buf_size,out_len,*s);
							char_len ++;
						}
						for(;char_len < total_charactor;char_len++) h_vprintf_outbyte(out_buf,out_buf_size,out_len,' ');//padding space at the end
					}
					else{
						char *s1 =s;
						for(;s1 != NULL && *s1!= 0;s1++) char_len ++; 	//Right align adjustment must check output len first
						while(char_len++ < total_charactor) h_vprintf_outbyte(out_buf,out_buf_size,out_len,' ');		//Space padding before output data
						for(;s != NULL && *s!= 0;s++)h_vprintf_outbyte(out_buf,out_buf_size,out_len,*s);	//then output data
					}
				}
				else{
					for(;s != NULL && *s!= 0;s++)h_vprintf_outbyte(out_buf,out_buf_size,out_len,*s);		//out data without padding
				}

			}
			break;
			//------------------------------------------------
			case 'b':
			case 'B':
			{
				int8_t  i,num_digit;
				uint32_t val = va_arg(argp, uint32_t);
				for(i=31,num_digit=32;i>=0;i -=1,num_digit--){
					if((val >>i)&1) break;
				}
				if(num_digit==0) num_digit = 1;
				//===left padding
				if(left_align==0){
					for(i=0;i< total_charactor - num_digit;i++) {
						if(padding_zero)h_vprintf_outbyte(out_buf,out_buf_size,out_len,'0');
						else 			h_vprintf_outbyte(out_buf,out_buf_size,out_len,' ');
					}
				}
				for(i=((num_digit-1));i>=0;i -=1){
					uint8_t val4 = (val >>i)&0x1;
					h_vprintf_outbyte(out_buf,out_buf_size,out_len,(uint8_t)(val4 +'0'));
				}
				if(left_align){
					for(i=0;i< total_charactor - num_digit;i++)  h_vprintf_outbyte(out_buf,out_buf_size,out_len,' ');
				}
			}
			break;
			//------------------------------------------------
			case 'o':
			case 'O':
			{
				int8_t  i,num_digit;
				uint32_t val = va_arg(argp, uint32_t);
				for(i=30,num_digit=11;i>=0;i -=3,num_digit--){
					if((val >>i)&0x7) break;
				}
				if(num_digit==0) num_digit = 1;
				//===left padding
				if(left_align==0){
					for(i=0;i< total_charactor - num_digit;i++) {
						if(padding_zero)h_vprintf_outbyte(out_buf,out_buf_size,out_len,'0');
						else 			h_vprintf_outbyte(out_buf,out_buf_size,out_len,' ');
					}
				}
				for(i=((num_digit-1)*3);i>=0;i -=3){
					uint8_t val4 = (val >>i)&0x7;
					h_vprintf_outbyte(out_buf,out_buf_size,out_len,(uint8_t)(val4 +'0'));
				}
				if(left_align){
					for(i=0;i< total_charactor - num_digit;i++)  h_vprintf_outbyte(out_buf,out_buf_size,out_len,' ');
				}
			}
			break;
			//------------------------------------------------
			case 'd'://%d
			case 'u'://%u
			{
				uint32_t u_val = va_arg(argp, uint32_t);
				uint8_t buf[PRINTF_BUF_SIZE];
				int8_t buf_len =0,i,flags=0;
				char padding_char = (padding_zero)?'0':' ';

				//invert negative signed to positive signed value
				if(*format =='d'){
					if(u_val >=((uint32_t)1<<31)){
						u_val = 0-u_val;
						if(buf_len < PRINTF_BUF_SIZE) buf[buf_len++] = '-';
					}
				}
				for(i=0;i< sizeof(div_table)>>2 ;i++){
					uint8_t digit_val = u_val/div_table[i];
					u_val -= digit_val*div_table[i];
					if(digit_val >0)flags =1;
					if(flags && buf_len < PRINTF_BUF_SIZE) buf[buf_len++] = '0' + digit_val;
				}
				if(buf_len ==0)buf[buf_len++] = '0';
				if(left_align==0){
					for(i=0; i < total_charactor - buf_len;i++) h_vprintf_outbyte(out_buf,out_buf_size,out_len,padding_char);
				}
				for(i=0;i< buf_len ;i++) h_vprintf_outbyte(out_buf,out_buf_size,out_len,buf[i]);
				if(left_align){
					for(; i < total_charactor - buf_len ;i++) h_vprintf_outbyte(out_buf,out_buf_size,out_len,' ' );
				}
			}
			break;
			//------------------------------------------------
			case 'l'://%lu,%ld,%llu,%lld implement same as %u,%d
			{
				uint8_t const * format_next =  format+1;
				if(format_next != NULL){
					if(*format_next == 'u' || *format_next == 'd'  || *format_next == 'l'){
						format_state =1;
					}
				}
			}
			break;
			//------------------------------------------------
			case 'x'://%x
			case 'X'://%X
			{
				int8_t  i,num_digit;
				uint32_t val = va_arg(argp, uint32_t);
				char CHAR1 = (*format =='x')?'a':'A';
				for(i=28,num_digit=8;i>=0;i -=4,num_digit--){
					if((val >>i)&0xf) break;
				}
				if(num_digit==0) num_digit = 1;
				//===left padding
				if(left_align==0){
					for(i=0;i< total_charactor - num_digit;i++) {
						if(padding_zero)h_vprintf_outbyte(out_buf,out_buf_size,out_len,'0');
						else 			h_vprintf_outbyte(out_buf,out_buf_size,out_len,' ');
					}
				}
				for(i=((num_digit-1)<<2);i>=0;i -=4){
					uint8_t val4 = (val >>i)&0xf;
					if(val4 <10) h_vprintf_outbyte(out_buf,out_buf_size,out_len,(uint8_t)(val4 +'0'));
					else h_vprintf_outbyte(out_buf,out_buf_size,out_len,(uint8_t)(val4 -10 + CHAR1));
				}
				if(left_align){
					for(i=0;i< total_charactor - num_digit;i++)  h_vprintf_outbyte(out_buf,out_buf_size,out_len,' ');
				}
			}
			break;
			//------------------------------------------------
#if PRINTF_SUPPORT_FLOAT
			case 'f':
			{
//				float_typedef tmpf;
//				tmpf.h[0] = va_arg(argp, uint32_t);
//				tmpf.h[1] = va_arg(argp, uint32_t);
//				double val = tmpf.df;
				double val = va_arg(argp, double);
				uint8_t buf[PRINTF_BUF_SIZE];
				uint8_t buf_len =0,buf_used=0,i,flags=0,fract_count =0;
				//sign
				if(val <0) { buf[buf_len++] ='-'; val = -val;}
				//Too large / too small range => convert to format 0.yyyyyyyyyyEzzzz
				if(val >= 1e9 || val < 1e-9){
					if(val == 0) h_vprintf_outbyte(out_buf,out_buf_size,out_len,'0');
					else{
						int8_t exp10 = 0;
						uint32_t u32;
						while(val >=1.0) { val *=0.1; exp10 ++; }
						while(val <0.1) { val *=10.0; exp10 --; }
						h_vprintf_outbyte(out_buf,out_buf_size,out_len,'0');
						h_vprintf_outbyte(out_buf,out_buf_size,out_len,'.');
						u32 = val * div_table[1];
						//Out fraction
						for(i=0;i< sizeof(div_table)>>2 && (fract_count < fract_len || fract_len ==0);i++){
							uint8_t digit_val = u32/div_table[i];
							u32 -= digit_val*div_table[i];
							if(digit_val >0)flags =1;
							if(flags){
								fract_count ++;
								if(buf_len < PRINTF_BUF_SIZE-4){
									buf[buf_len++] = '0' + digit_val;
									if(digit_val || fract_count <= fract_len ) buf_used = buf_len; //Tracking non zero character
								}
							}
						}
						//out exponent
						buf_len = buf_used;
						if(exp10 <0){
							exp10 = -exp10;
							if(buf_len< PRINTF_BUF_SIZE)buf[buf_len++] ='-';
							if(buf_len< PRINTF_BUF_SIZE)buf[buf_len++] ='E';
							out_u32(out_buf,out_buf_size,out_len,exp10,0);
						}
						else{
							if(buf_len< PRINTF_BUF_SIZE)buf[buf_len++] ='-';
							if(buf_len< PRINTF_BUF_SIZE)buf[buf_len++] ='E';
						}
						flags =0;
						for(i=0;i< sizeof(div_table)>>2;i++){
							uint8_t digit_val = exp10/div_table[i];
							exp10 -= digit_val*div_table[i];
							if(digit_val >0)flags =1;
							if(flags){
								if(buf_len < PRINTF_BUF_SIZE){
									buf[buf_len++] = '0' + digit_val;
									if(digit_val) buf_used = buf_len; //Tracking non zero character
								}
							}
						}
					}
				}
				else{ //normal range
					int8_t dot_index = 0;
					uint32_t u32;
					while(val < div_table[1] && dot_index <6) { val *=10; dot_index ++;}
					u32 = val;
					for(i=0;i< sizeof(div_table)>>2;i++){
						uint8_t digit_val = u32/div_table[i];
						u32 -= digit_val*div_table[i];
						if(digit_val >0)flags =1;

						if(fract_count) fract_count ++;

						if(i == (sizeof(div_table)>>2)-dot_index){
							if(buf_used==0){
								if(buf_len < PRINTF_BUF_SIZE) buf[buf_len++] = '0';
								if(buf_len < PRINTF_BUF_SIZE) buf[buf_len++] = '.';
							}
							else{
								if(buf_len < PRINTF_BUF_SIZE) buf[buf_len++] = '.';
							}
							flags = 1;
							buf_used = buf_len;
							fract_count =1;
						}
						if(flags){
							if(buf_len < PRINTF_BUF_SIZE){
								buf[buf_len++] = '0' + digit_val;
								if(digit_val || (fract_count >0 && fract_count <= fract_len)) buf_used = buf_len; //Tracking non zero character
							}
						}
						if(fract_count >= fract_len && fract_len >0) break;

					}
				}
				//Send buffer data out
				if(left_align==0){
					for(i=0; i < total_charactor - buf_len ;i++) h_vprintf_outbyte(out_buf,out_buf_size,out_len,' ');
				}
				for(i=0;i< buf_used ;i++) h_vprintf_outbyte(out_buf,out_buf_size,out_len,buf[i]);
				if(left_align){
					for(; i < total_charactor - buf_len ;i++) h_vprintf_outbyte(out_buf,out_buf_size,out_len,' ');
				}
			}

			break;
#endif
			//------------------------------------------------
			default: //un supported. Abort format
			{
//				uint32_t tmp = va_arg(argp, uint32_t);
//				(void)(tmp);
				return -1;
			}
			break;
			}
		}
	}
	//	va_end(argp);
	return 0;
}
//========================================================================
int h_printf(const char* format, ...)
{
	va_list va;
	va_start(va, format);
	const int ret = h_vprintf(NULL,0, format, va);
	va_end(va);
	return ret;
}
//========================================================================
int h_sprintf(char* buf,const char* format, ...)
{
	va_list va;
	va_start(va, format);
	const int ret = h_vprintf(buf,64, format, va);
	va_end(va);
	return ret;
}
//========================================================================
int h_snprintf(char* buf,size_t count,const char* format, ...)
{
	va_list va;
	va_start(va, format);
	const int ret = h_vprintf(buf,count, format, va);
	va_end(va);
	return ret;
}
