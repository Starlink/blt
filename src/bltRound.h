
#if (SIZEOF_FLOAT == 8)
#define REAL64	float
#else
#define REAL64	double
#endif	/* SIZE_VOID_P == 8 */

#define DOUBLE_MAGIC		6755399441055744.0
#define DEFAULT_CONVERSION	1
#ifdef WORDS_BIGENDIAN
#define IMAN 1
#define IEXP 0
#else
#define IMAN 0
#define IEXP 1
#endif	/* WORDS_BIGENDIAN */

static INLINE int 
CRoundToInt(REAL64 val)
{
#if DEFAULT_CONVERSION==0
    val += DOUBLE_MAGIC;
#ifdef WORDS_BIGENDIAN
    return ((int *)&val)[1];
#else
    return ((int *)&val)[0];
#endif	/* WORD_BIGENDIAN */
#else
    return (int)(floor(val+.5));
#endif	/* DEFAULT_CONVERSION */
}

