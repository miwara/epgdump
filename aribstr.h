#ifndef ARIBSTR_H
#define ARIBSTR_H 1

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

	int AribToString(char *lpszDst, const char *pSrcData, const int dwSrcLen);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* #undef ICONV_REQUIRES_CONST */
#ifdef ICONV_REQUIRES_CONST
#define ICONV_CONST const
#else
#define ICONV_CONST
#endif

#endif
